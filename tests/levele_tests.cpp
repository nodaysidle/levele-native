// Unit tests for the data and geometry layers. Everything here runs headless
// (QT_QPA_PLATFORM=offscreen) and writes only under a temporary directory.
//
// The document model, the pick geometry, the undo rules, the save path, the
// trash and the contrast derivation are covered here. Window-level behaviour
// (quit-time save, the Space modifier, the drawn line vector, drag reorder) is
// covered by the offscreen QML harness in the handoff, because it needs real
// mouse and key events.
#include "canvasitem.h"
#include "document.h"
#include "exporter.h"
#include "portalfilechooser.h"
#include "studio.h"
#include "templates.h"
#include "theme.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <cstdio>

// ---------------------------------------------------------------- fake portal
// A minimal stand-in for org.freedesktop.portal.Desktop, so the FileChooser
// client is exercised end to end on a private session bus. Adaptors are used
// because plain QObject signals are not relayed over D-Bus.
class FakePortalService : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    QString lastTitle;
    QString lastSuggestedName;
    uint response = 0;
};

class FakeRequestAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Request")
public:
    explicit FakeRequestAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}
public slots:
    void Close() {}
signals:
    void Response(uint response, const QVariantMap &results);
};

class FakeFileChooserAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.FileChooser")
public:
    explicit FakeFileChooserAdaptor(FakePortalService *service)
        : QDBusAbstractAdaptor(service)
        , m_service(service)
    {
    }

public slots:
    QDBusObjectPath SaveFile(const QString &parentWindow, const QString &title,
                             const QVariantMap &options);
    QDBusObjectPath OpenFile(const QString &parentWindow, const QString &title,
                             const QVariantMap &options);

private:
    QDBusObjectPath answer(const QString &title, const QVariantMap &options);
    FakePortalService *m_service;
};

QDBusObjectPath FakeFileChooserAdaptor::SaveFile(const QString &parentWindow, const QString &title,
                                                  const QVariantMap &options)
{
    Q_UNUSED(parentWindow);
    return answer(title, options);
}

QDBusObjectPath FakeFileChooserAdaptor::OpenFile(const QString &parentWindow, const QString &title,
                                                  const QVariantMap &options)
{
    Q_UNUSED(parentWindow);
    return answer(title, options);
}

QDBusObjectPath FakeFileChooserAdaptor::answer(const QString &title, const QVariantMap &options)
{
    m_service->lastTitle = title;
    m_service->lastSuggestedName = options.value(QStringLiteral("current_name")).toString();

    // The client pre-subscribes to the Request path built from its own unique
    // bus name and the token it asked for, so answer on exactly that path. The
    // client and this fake share one connection, so its unique name is ours.
    QString sender = QDBusConnection::sessionBus().baseService();
    if (sender.startsWith(QLatin1Char(':')))
        sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString token = options.value(QStringLiteral("handle_token")).toString();
    const QString path = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
                             .arg(sender, token);

    auto *host = new QObject(m_service);
    auto *request = new FakeRequestAdaptor(host);
    QDBusConnection::sessionBus().registerObject(path, host, QDBusConnection::ExportAdaptors);
    QTimer::singleShot(0, m_service, [request, response = m_service->response]() {
        QVariantMap results;
        results.insert(QStringLiteral("uris"),
                       QStringList{QStringLiteral("file:///tmp/levele-portal-test.png")});
        emit request->Response(response, results);
    });
    return QDBusObjectPath(path);
}

class LeveleTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void linePick_data();
    void linePick();
    void templateLinesArePickable();

    void hostileDocumentIsClamped();
    void validDocumentSurvives();

    void saveIsAtomicAndReportsFailure();
    void saveFeedbackShowsSaved();
    void openedFilenameOwnsSave();
    void failedSaveKeepsDocumentOpen();

    void deleteGoesToTrashAndBack();
    void listDocsIgnoresTrash();
    void trashAgeAndRestoreCollision();
    void deleteRespectsLockedLayers();
    void gifBudgetKeepsPreviousExport();

    void clicksDoNotConsumeUndo();
    void nudgesCoalesceIntoOneStep();
    void alignWithoutSelectionPushesNothing();
    void selectionDoesNotDirtyTheDocument();

    void accentInkIsReadable();
    void contrastPairsHold();
    void zoomFactorIsProportional();

    void themeReadsRadiusAndReloadsOnConfigReload();
    void themeReloadsWhenTheLiveThemeChanges();
    void portalFileChooserRoundTrip();

    void lockedLayersIgnoreSetSelProp();
    void hostileElementFieldsAndNonRegularImageAreGuarded();
    void pdfExportIsAtomic();
    void remoteExportUrlIsRejected();
    void backgroundExportCompletesCleanlyOnDestroy();
    void signedLineAlignAndListDocsFilter();

    void movePageTracksActivePageAndClearsSelection();
    void imageGuardsRejectNonRegularAndLocked();
    void nudgeGuardsLockedAndResetsAcrossUndo();
    void noOpUndoGuardsAndNewDocumentClamp();
    void jsonEmptyIdHostileWeightAndRotatedIntersects();
    void renameAndSelLockedExposed();

private:
    QTemporaryDir m_dir;
};

void LeveleTests::initTestCase()
{
    QVERIFY(m_dir.isValid());
    qputenv("XDG_DATA_HOME", (m_dir.path() + QStringLiteral("/data")).toUtf8());
}

// ------------------------------------------------------------------- F03

void LeveleTests::linePick_data()
{
    QTest::addColumn<double>("x");
    QTest::addColumn<double>("y");
    QTest::addColumn<double>("w");
    QTest::addColumn<double>("h");
    QTest::addColumn<bool>("expectHit");

    QTest::newRow("300x0 midpoint") << 100.0 << 100.0 << 300.0 << 0.0 << true;
    QTest::newRow("300x0 far") << 100.0 << 100.0 << 300.0 << 0.0 << false;
    QTest::newRow("vertical") << 0.0 << 50.0 << 0.0 << -300.0 << true;
}

void LeveleTests::linePick()
{
    QFETCH(double, x);
    QFETCH(double, y);
    QFETCH(double, w);
    QFETCH(double, h);
    QFETCH(bool, expectHit);

    Element line = Element::make(QStringLiteral("line"));
    line.x = x;
    line.y = y;
    line.w = w;
    line.h = h;

    const QPointF probe = expectHit ? line.segment().center() : QPointF(250, 380);
    QCOMPARE(line.containsPoint(probe, 6.0), expectHit);
    // The bounding rect of a zero-height line is empty: the old code used it
    // and could never hit the line at all.
    QVERIFY(line.rect().isEmpty() || !line.rect().contains(probe));
}

void LeveleTests::templateLinesArePickable()
{
    int lines = 0;
    for (const TemplateInfo &t : buildTemplates()) {
        for (const Page &p : t.doc.pages) {
            for (const Element &e : p.elements) {
                if (!e.isLineLike())
                    continue;
                ++lines;
                const QPointF mid = e.segment().center();
                QVERIFY2(e.intersectsRect(QRectF(mid - QPointF(4, 4), QSizeF(8, 8)), 6.0),
                         qPrintable(QStringLiteral("template '%1' line not marquee-selectable")
                                        .arg(t.name)));
            }
        }
    }
    QVERIFY(lines > 0);
}

// ------------------------------------------------------------------- F24

void LeveleTests::hostileDocumentIsClamped()
{
    QJsonObject o;
    o[QStringLiteral("id")] = QStringLiteral("../../evil");
    o[QStringLiteral("width")] = -5.0;
    o[QStringLiteral("height")] = 1e9;
    const Doc d = Doc::fromJson(o);

    QVERIFY(Doc::isValidId(d.id));
    QVERIFY(!d.id.contains(QLatin1Char('/')));
    QVERIFY(d.width >= 16.0 && d.width <= 16384.0);
    QVERIFY(d.height >= 16.0 && d.height <= 16384.0);
}

void LeveleTests::validDocumentSurvives()
{
    QJsonObject o;
    o[QStringLiteral("id")] = QStringLiteral("8f13d93d-b4ee");
    o[QStringLiteral("width")] = 1080.0;
    o[QStringLiteral("height")] = 720.0;
    const Doc d = Doc::fromJson(o);
    QCOMPARE(d.id, QStringLiteral("8f13d93d-b4ee"));
    QCOMPARE(d.width, 1080.0);
    QCOMPARE(d.height, 720.0);
}

// ------------------------------------------------------------------- F05

void LeveleTests::saveIsAtomicAndReportsFailure()
{
    Studio studio;
    studio.newDocument(400, 400);
    QVERIFY(studio.saveNow());
    const QString live = studio.dataRoot() + QStringLiteral("/docs/") + studio.docId()
        + QStringLiteral(".json");
    QVERIFY(QFileInfo::exists(live));

    // No .tmp litter is left behind: QSaveFile renames into place.
    const QStringList leftovers = QDir(studio.dataRoot() + QStringLiteral("/docs"))
                                      .entryList({QStringLiteral("*.tmp*")}, QDir::Files);
    QVERIFY2(leftovers.isEmpty(), qPrintable(leftovers.join(QStringLiteral(", "))));

    const QString docs = studio.dataRoot() + QStringLiteral("/docs");
    const QFile::Permissions original = QFile::permissions(docs);
    QVERIFY(QFile::setPermissions(docs, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    studio.setTitle(QStringLiteral("Read-only probe"));
    const bool wrote = studio.saveNow();
    const QString status = studio.status();
    QFile::setPermissions(docs, original);

    QVERIFY2(!wrote, "writing into a read-only store must fail");
    QVERIFY2(status.contains(QStringLiteral("Couldn't save")), qPrintable(status));
}

void LeveleTests::saveFeedbackShowsSaved()
{
    Studio studio;
    studio.newDocument(200, 200);
    QVERIFY(studio.saveWithFeedback());
    QCOMPARE(studio.status(), QStringLiteral("Saved"));
}

void LeveleTests::openedFilenameOwnsSave()
{
    Studio studio;
    studio.newDocument(200, 200);
    const QString originalId = studio.docId();
    const QString copiedId = QStringLiteral("copy-") + originalId;
    const QString docs = studio.dataRoot() + QStringLiteral("/docs/");
    const auto docPath = [&](const QString &id) { return docs + id + QStringLiteral(".json"); };
    QVERIFY(studio.closeDoc());
    QVERIFY(QFile::copy(docPath(originalId), docPath(copiedId)));

    QVERIFY(studio.openDoc(copiedId));
    QCOMPARE(studio.docId(), copiedId);
    QVERIFY(studio.closeDoc());
    const auto stored = [&](const QString &id) {
        QFile f(docPath(id));
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
    };
    QCOMPARE(stored(copiedId).value("id").toString(), originalId);

    QVERIFY(studio.openDoc(copiedId));
    studio.setTitle(QStringLiteral("Edited copy"));
    QVERIFY(studio.closeDoc());
    QCOMPARE(stored(copiedId).value("id").toString(), copiedId);
    QCOMPARE(stored(copiedId).value("title").toString(), QStringLiteral("Edited copy"));
    QCOMPARE(stored(originalId).value("title").toString(), QStringLiteral("Untitled"));
}

void LeveleTests::failedSaveKeepsDocumentOpen()
{
    Studio studio;
    studio.newDocument(200, 200);
    const QString id = studio.docId();
    const QString live = studio.dataRoot() + QStringLiteral("/docs/") + id + QStringLiteral(".json");
    const QString backup = live + QStringLiteral(".backup");
    QVERIFY(QFile::rename(live, backup));
    QVERIFY(QDir().mkdir(live));

    QSignalSpy blocked(&studio, &Studio::leaveBlocked);
    studio.setTitle(QStringLiteral("Keep this edit"));
    QVERIFY(!studio.closeDoc());
    QVERIFY(!studio.prepareToLeave(true));
    QVERIFY(studio.editing());
    QCOMPARE(studio.title(), QStringLiteral("Keep this edit"));
    QCOMPARE(blocked.count(), 2);
    QCOMPARE(blocked.at(0).at(0).toBool(), false);
    QCOMPARE(blocked.at(1).at(0).toBool(), true);

    QVERIFY(QDir().rmdir(live));
    QVERIFY(QFile::rename(backup, live));
    QVERIFY(studio.closeDoc());
    QVERIFY(studio.openDoc(id));
    QCOMPARE(studio.title(), QStringLiteral("Keep this edit"));
    studio.setTitle(QStringLiteral("Discard this edit"));
    studio.discardChanges();
    QVERIFY(studio.closeDoc());
    QVERIFY(studio.openDoc(id));
    QCOMPARE(studio.title(), QStringLiteral("Keep this edit"));
}

// ------------------------------------------------------------------- F04

QString findTrashKey(const Studio &studio, const QString &id)
{
    for (const QVariant &row : studio.listTrash()) {
        const QString key = row.toMap().value("id").toString();
        if (key.startsWith(id + QLatin1Char('.')))
            return key;
    }
    return {};
}

void LeveleTests::deleteGoesToTrashAndBack()
{
    Studio studio;
    studio.newFromTemplate(0);
    const QString id = studio.docId();
    QVERIFY(studio.closeDoc());

    const QString live = studio.dataRoot() + QStringLiteral("/docs/") + id + QStringLiteral(".json");
    studio.deleteDoc(id);
    QVERIFY(!QFileInfo::exists(live));
    QVERIFY(!findTrashKey(studio, id).isEmpty());
    QVERIFY(studio.trashPending());

    studio.undoDelete();
    QVERIFY(QFileInfo::exists(live));
    QVERIFY(findTrashKey(studio, id).isEmpty());
    QVERIFY(!studio.trashPending());
}

void LeveleTests::listDocsIgnoresTrash()
{
    Studio studio;
    studio.newFromTemplate(1);
    const QString id = studio.docId();
    QVERIFY(studio.closeDoc());
    studio.deleteDoc(id);

    for (const QVariant &v : studio.listDocs())
        QVERIFY(v.toMap().value(QStringLiteral("id")).toString() != id);
}

void LeveleTests::trashAgeAndRestoreCollision()
{
    Studio studio;
    studio.newDocument(200, 200);
    studio.setTitle(QStringLiteral("Original in trash"));
    const QString id = studio.docId();
    QVERIFY(studio.closeDoc());
    const QString docs = studio.dataRoot() + QStringLiteral("/docs/");
    QFile original(docs + id + QStringLiteral(".json"));
    QVERIFY(original.open(QIODevice::ReadOnly));
    QVERIFY(original.setFileTime(QDateTime::currentDateTime().addDays(-10), QFileDevice::FileModificationTime));
    original.close();
    studio.deleteDoc(id);

    Studio restarted;
    const QString trashKey = findTrashKey(restarted, id);
    QVERIFY(!trashKey.isEmpty());

    restarted.newDocument(200, 200);
    restarted.setTitle(QStringLiteral("Keep live design"));
    const QString otherId = restarted.docId();
    QVERIFY(restarted.closeDoc());
    QVERIFY(QFile::rename(docs + otherId + QStringLiteral(".json"), original.fileName()));
    QVERIFY(restarted.restoreDoc(trashKey));
    QVERIFY(findTrashKey(restarted, id).isEmpty());
    QVERIFY(restarted.openDoc(id));
    QCOMPARE(restarted.title(), QStringLiteral("Keep live design"));

    QString restoredId;
    for (const QVariant &row : restarted.listDocs()) {
        if (row.toMap().value("title").toString() == QLatin1String("Original in trash"))
            restoredId = row.toMap().value("id").toString();
    }
    QVERIFY(!restoredId.isEmpty());
    QVERIFY(restoredId != id);
    QVERIFY(restarted.openDoc(restoredId));
    QCOMPARE(restarted.title(), QStringLiteral("Original in trash"));
}

void LeveleTests::deleteRespectsLockedLayers()
{
    Studio studio;
    studio.newDocument(200, 200);
    const QString locked = studio.addElement(QStringLiteral("rect"));
    const QString unlocked = studio.addElement(QStringLiteral("ellipse"));
    studio.setElementLocked(locked, true);
    studio.selectAll();
    QVERIFY(studio.isSelected(unlocked));
    QVERIFY(!studio.isSelected(locked));

    studio.selectElement(locked, true); // The layers panel can select a locked row.
    studio.deleteSelected();
    QVERIFY(studio.elementById(locked));
    QVERIFY(!studio.elementById(unlocked));
    studio.undo();
    QVERIFY(studio.elementById(locked));
    QVERIFY(studio.elementById(unlocked));
}

// ------------------------------------------------------------------- F08

void LeveleTests::gifBudgetKeepsPreviousExport()
{
    QFile file(m_dir.filePath(QStringLiteral("existing.gif")));
    const QByteArray previous = QByteArrayLiteral("previous export");
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(previous), previous.size());
    file.close();

    Doc doc = makeBlankDoc(1080, 1920, QStringLiteral("Portrait animation"));
    const Page page = doc.pages.first();
    doc.pages.resize(80, page);
    QString error;
    QVERIFY(!Exporter::writeGif(doc, file.fileName(), 2000, 12, &error));
    QVERIFY(error.contains(QStringLiteral("too large")));

    doc.pages.resize(1);
    doc.height = 8000;
    error.clear();
    QVERIFY(!Exporter::writeGif(doc, file.fileName(), 2000, 12, &error));
    QVERIFY(error.contains(QStringLiteral("too large")));

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), previous);
}

void LeveleTests::clicksDoNotConsumeUndo()
{
    Studio studio;
    studio.newFromTemplate(0);
    studio.selectAll();
    QVERIFY(!studio.canUndo());

    // A press/release pair that never moves must discard its snapshot.
    for (int i = 0; i < 10; ++i) {
        studio.takeGestureSnapshot();
        studio.selectElement(studio.primaryId(), false);
        studio.discardGestureSnapshot();
    }
    QVERIFY(!studio.canUndo());

    // A gesture that did move keeps exactly one step. A drag mutates the
    // elements directly (setSelProp is the Inspector path and pushes its own
    // snapshot).
    studio.takeGestureSnapshot();
    studio.elements().first().x += 25.0;
    studio.commitGestureSnapshot();
    QVERIFY(studio.canUndo());
    studio.undo();
    QVERIFY(!studio.canUndo());
}

void LeveleTests::nudgesCoalesceIntoOneStep()
{
    Studio studio;
    studio.newFromTemplate(0);
    studio.selectAll();
    const double x0 = studio.primaryElement()->x;

    for (int i = 0; i < 10; ++i)
        studio.beginNudge();
    for (Element &e : studio.elements())
        e.x += 10;
    QVERIFY(studio.canUndo());

    studio.undo();
    QCOMPARE(studio.primaryElement()->x, x0);
    QVERIFY(!studio.canUndo());
}

void LeveleTests::alignWithoutSelectionPushesNothing()
{
    Studio studio;
    studio.newFromTemplate(0);
    studio.clearSelection();
    QVERIFY(!studio.canUndo());
    studio.alignSelected(QStringLiteral("left"));
    QVERIFY(!studio.canUndo());
}

void LeveleTests::selectionDoesNotDirtyTheDocument()
{
    Studio studio;
    studio.newFromTemplate(0);

    QSignalSpy contents(&studio, &Studio::contentsChanged);
    QSignalSpy selection(&studio, &Studio::selectionChanged);
    studio.selectAll();
    QCOMPARE(selection.count(), 1);
    QCOMPARE(contents.count(), 0);
}

// --------------------------------------------------------------- F02 / F18

void LeveleTests::accentInkIsReadable()
{
    // A mid-tone accent is exactly where the old luminance threshold chose
    // white (2.86:1 on Alan's theme). Black and white are both tried now.
    const QColor midTone(QStringLiteral("#d48473"));
    const QColor ink = Theme::bestInkOn(midTone);
    QVERIFY(Theme::contrast(ink, midTone) >= 4.5);

    const QColor dark(QStringLiteral("#101010"));
    QCOMPARE(Theme::bestInkOn(dark), QColor(Qt::white));
    const QColor light(QStringLiteral("#f0f0f0"));
    QCOMPARE(Theme::bestInkOn(light), QColor(Qt::black));
}

void LeveleTests::contrastPairsHold()
{
    Theme theme;
    const QString live = Theme::currentRoot();
    QVERIFY(!live.isEmpty());

    const QVector<Theme::ContrastPair> pairs = Theme::contrastPairs();
    QCOMPARE(pairs.size(), 12);

    QVector<QString> dirs;
    if (!live.isEmpty())
        dirs << live;
    for (const QString &root : {QDir::homePath() + QStringLiteral("/.config/omarchy/themes"),
                                QStringLiteral("/usr/share/omarchy/themes")}) {
        const QFileInfoList entries = QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : entries) {
            const QString dir = fi.absoluteFilePath();
            if (QFileInfo::exists(dir + QStringLiteral("/colors.toml"))
                || QFileInfo::exists(dir + QStringLiteral("/theme/colors.toml")))
                dirs << dir;
        }
    }
    QVERIFY(dirs.size() > 50);

    for (const QString &dir : std::as_const(dirs)) {
        theme.loadFromRoot(dir);
        for (const Theme::ContrastPair &p : pairs) {
            const double ratio = Theme::contrast(theme.token(p.ink), theme.token(p.paper));
            QVERIFY2(ratio + 1e-9 >= p.ratio,
                     qPrintable(QStringLiteral("%1 in %2: %3 < %4")
                                    .arg(p.label, QFileInfo(dir).fileName(),
                                         QString::number(ratio, 'f', 2),
                                         QString::number(p.ratio, 'f', 1))));
        }
    }
}

// ------------------------------------------------------------------- F22

void LeveleTests::zoomFactorIsProportional()
{
    QVERIFY(qAbs(CanvasItem::zoomFactorForDelta(15) - 1.023) < 0.001);
    QVERIFY(qAbs(CanvasItem::zoomFactorForDelta(120) - 1.197) < 0.002);
    QCOMPARE(CanvasItem::zoomFactorForDelta(0), 1.0);
}

// ------------------------------------------------------------------- F29

void LeveleTests::themeReadsRadiusAndReloadsOnConfigReload()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());

    // A stand-in hyprctl that answers 14 first and 20 after a config reload.
    const QString binDir = sandbox.path() + QStringLiteral("/bin");
    const QString stateDir = sandbox.path() + QStringLiteral("/state");
    const QString runDir = sandbox.path() + QStringLiteral("/run");
    QVERIFY(QDir().mkpath(binDir));
    QVERIFY(QDir().mkpath(stateDir + QStringLiteral("/omarchy/current")));
    QVERIFY(QDir().mkpath(runDir + QStringLiteral("/hypr/levele-test")));

    const QString calls = sandbox.path() + QStringLiteral("/calls");
    const QString script = binDir + QStringLiteral("/hyprctl");
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("#!/bin/sh\n"
            "n=$(cat '" + calls.toUtf8() + "' 2>/dev/null || echo 0)\n"
            "n=$((n+1)); echo $n > '" + calls.toUtf8() + "'\n"
            "if [ \"$n\" -le 1 ]; then echo '{\"int\": 14}'; else echo '{\"int\": 20}'; fi\n");
    f.close();
    QVERIFY(QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                              | QFileDevice::ExeOwner));

    // A minimal live theme, so currentRoot() resolves inside the sandbox.
    QFile colors(stateDir + QStringLiteral("/omarchy/current/colors.toml"));
    QVERIFY(colors.open(QIODevice::WriteOnly));
    colors.write("mode = \"dark\"\nbackground = \"#101010\"\nforeground = \"#eeeeee\"\n"
                 "accent = \"#ff8800\"\n");
    colors.close();

    // socket2 for the configreloaded event.
    QLocalServer events;
    QVERIFY(events.listen(runDir + QStringLiteral("/hypr/levele-test/.socket2.sock")));

    qputenv("PATH", (binDir + QLatin1Char(':') + QString::fromLocal8Bit(qgetenv("PATH"))).toUtf8());
    qputenv("XDG_STATE_HOME", stateDir.toUtf8());
    qputenv("XDG_RUNTIME_DIR", runDir.toUtf8());
    qputenv("HYPRLAND_INSTANCE_SIGNATURE", "levele-test");

    Theme theme;
    QTRY_COMPARE_WITH_TIMEOUT(theme.property("radius").toInt(), 14, 5000);

    QVERIFY(events.waitForNewConnection(3000));
    QLocalSocket *client = events.nextPendingConnection();
    QVERIFY(client != nullptr);
    client->write("configreloaded>>decoration:rounding\n");
    client->flush();

    QTRY_COMPARE_WITH_TIMEOUT(theme.property("radius").toInt(), 20, 5000);

    qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
    qunsetenv("XDG_RUNTIME_DIR");
    qunsetenv("XDG_STATE_HOME");
}

void LeveleTests::themeReloadsWhenTheLiveThemeChanges()
{
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    const QString stateDir = sandbox.path();
    const QString live = stateDir + QStringLiteral("/omarchy/current");
    QVERIFY(QDir().mkpath(live));

    QFile colors(live + QStringLiteral("/colors.toml"));
    QVERIFY(colors.open(QIODevice::WriteOnly));
    colors.write("mode = \"dark\"\nbackground = \"#101010\"\nforeground = \"#eeeeee\"\n"
                 "accent = \"#ff8800\"\n");
    colors.close();

    qputenv("XDG_STATE_HOME", stateDir.toUtf8());
    Theme theme;
    const QColor before = theme.property("accent").value<QColor>();
    QCOMPARE(before, QColor(QStringLiteral("#ff8800")));

    // omarchy-theme-set rewrites the file in place; the debounced watcher must
    // pick the new accent up while the window is open.
    QVERIFY(colors.open(QIODevice::WriteOnly | QIODevice::Truncate));
    colors.write("mode = \"dark\"\nbackground = \"#101010\"\nforeground = \"#eeeeee\"\n"
                 "accent = \"#00ff00\"\n");
    colors.close();

    QTRY_VERIFY_WITH_TIMEOUT(theme.property("accent").value<QColor>()
                                 == QColor(QStringLiteral("#00ff00")),
                             5000);
    qunsetenv("XDG_STATE_HOME");
}

// ------------------------------------------------------------------- F28

void LeveleTests::portalFileChooserRoundTrip()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        QSKIP("no session bus (run under dbus-run-session)");

    FakePortalService portalService;
    FakeFileChooserAdaptor adaptor(&portalService);
    if (!bus.registerService(QStringLiteral("org.freedesktop.portal.Desktop"))) {
        QSKIP("the portal name is already taken on this bus "
              "(run with QT_QPA_PLATFORMTHEME= so the GTK theme does not probe it)");
    }
    QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"),
                               &portalService, QDBusConnection::ExportAdaptors));

    PortalFileChooser chooser;
    QVERIFY(chooser.available());

    const QUrl testFile(QStringLiteral("file:///tmp/levele-portal-test.png"));
    QSignalSpy openFinished(&chooser, &PortalFileChooser::openFinished);
    QSignalSpy saveFinished(&chooser, &PortalFileChooser::saveFinished);
    QVERIFY(chooser.saveFile(QStringLiteral("Export to file"), QStringLiteral("My design.png")));
    QVERIFY(chooser.busy());
    QVERIFY(!chooser.openFile(QStringLiteral("Choose an image")));

    QTRY_COMPARE_WITH_TIMEOUT(saveFinished.count(), 1, 5000);
    QCOMPARE(openFinished.count(), 0);
    QCOMPARE(saveFinished.first().at(0).toUrl(), testFile);
    QCOMPARE(saveFinished.first().at(1).toBool(), false);
    QCOMPARE(portalService.lastSuggestedName, QStringLiteral("My design.png"));
    QCOMPARE(portalService.lastTitle, QStringLiteral("Export to file"));
    QVERIFY(!chooser.busy());

    QVERIFY(chooser.openFile(QStringLiteral("Choose an image")));
    QTRY_COMPARE_WITH_TIMEOUT(openFinished.count(), 1, 5000);
    QCOMPARE(saveFinished.count(), 1);
    QCOMPARE(openFinished.first().at(0).toUrl(), testFile);
    QCOMPARE(openFinished.first().at(1).toBool(), false);

    portalService.response = 1;
    QVERIFY(chooser.openFile(QStringLiteral("Choose an image")));
    QTRY_COMPARE_WITH_TIMEOUT(openFinished.count(), 2, 5000);
    QVERIFY(openFinished.last().at(0).toUrl().isEmpty());
    QCOMPARE(openFinished.last().at(1).toBool(), false);

    portalService.response = 2;
    QVERIFY(chooser.openFile(QStringLiteral("Choose an image")));
    QTRY_COMPARE_WITH_TIMEOUT(openFinished.count(), 3, 5000);
    QVERIFY(openFinished.last().at(0).toUrl().isEmpty());
    QCOMPARE(openFinished.last().at(1).toBool(), true);

    bus.unregisterObject(QStringLiteral("/org/freedesktop/portal/desktop"));
    bus.unregisterService(QStringLiteral("org.freedesktop.portal.Desktop"));
}

void LeveleTests::lockedLayersIgnoreSetSelProp()
{
    Studio studio;
    studio.newDocument(400, 400);
    const QString locked = studio.addElement(QStringLiteral("rect"));
    const double x0 = studio.elementById(locked)->x;
    studio.setElementLocked(locked, true);
    studio.selectElement(locked, false);
    const int undoBefore = studio.canUndo() ? 1 : 0;
    studio.setSelProp(QStringLiteral("x"), x0 + 99.0);
    QCOMPARE(studio.elementById(locked)->x, x0);
    QCOMPARE(studio.canUndo() ? 1 : 0, undoBefore);
}

void LeveleTests::hostileElementFieldsAndNonRegularImageAreGuarded()
{
    QJsonObject el;
    el["id"] = QStringLiteral("el1");
    el["type"] = QStringLiteral("image");
    el["x"] = 1e12;
    el["opacity"] = -5.0;
    el["fontSize"] = 1e9;
    el["strokeWidth"] = 1e9;
    el["lineHeight"] = 1e6;
    el["imagePath"] = QStringLiteral("/dev/zero");

    QJsonObject page;
    page["id"] = QStringLiteral("p1");
    page["background"] = QStringLiteral("not-a-color");
    page["elements"] = QJsonArray{el};

    QJsonObject root;
    root["id"] = QStringLiteral("doc1");
    root["width"] = 400;
    root["height"] = 400;
    root["pages"] = QJsonArray{page};

    const Doc d = Doc::fromJson(root);
    QCOMPARE(d.pages.first().background, QStringLiteral("#ffffff"));
    const Element &e = d.pages.first().elements.first();
    QVERIFY(e.x <= 1e6);
    QCOMPARE(e.opacity, 0.0);
    QVERIFY(e.fontSize <= 2000.0);
    QVERIFY(e.strokeWidth <= 2000.0);
    QVERIFY(e.lineHeight <= 10.0);

    // Rendering an image pointing at /dev/zero must return quickly without hanging.
    const QImage rendered = Exporter::renderPage(d, 0, 0.5);
    QVERIFY(!rendered.isNull());
}

void LeveleTests::pdfExportIsAtomic()
{
    const QString path = m_dir.filePath(QStringLiteral("atomic.pdf"));
    const Doc doc = makeBlankDoc(400, 400, QStringLiteral("Atomic PDF"));
    QString error;
    QVERIFY(Exporter::writePdf(doc, path, &error));
    QVERIFY(error.isEmpty());
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.read(5) == "%PDF-");
}

void LeveleTests::remoteExportUrlIsRejected()
{
    Studio studio;
    studio.newDocument(200, 200);
    studio.exportPng(QUrl(QStringLiteral("https://example.com/out.png")), 0, 1.0);
    QVERIFY(!studio.exporting());
    QVERIFY(studio.status().contains(QStringLiteral("no file chosen")));
}

void LeveleTests::backgroundExportCompletesCleanlyOnDestroy()
{
    const QString path = m_dir.filePath(QStringLiteral("bg-destroy.png"));
    {
        Studio studio;
        studio.newFromTemplate(0);
        studio.exportPng(QUrl::fromLocalFile(path), 0, 1.0);
        QVERIFY(studio.exporting());
    }
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(QFileInfo(path).size() > 0);
}

void LeveleTests::signedLineAlignAndListDocsFilter()
{
    Studio studio;
    studio.newDocument(400, 400);
    const QString lineId = studio.addElement(QStringLiteral("line"));
    Element *line = studio.elementById(lineId);
    QVERIFY(line != nullptr);
    line->x = 300;
    line->y = 100;
    line->w = -100;
    line->h = 0;

    studio.selectElement(lineId, false);
    studio.setSelProp(QStringLiteral("h"), 0.0);
    QCOMPARE(studio.elementById(lineId)->h, 0.0);

    studio.alignSelected(QStringLiteral("left"));
    QCOMPARE(studio.elementById(lineId)->rect().left(), 0.0);
    QCOMPARE(studio.elementById(lineId)->w, -100.0);

    // Invalid document filenames must not appear in listDocs().
    QFile junk(studio.dataRoot() + QStringLiteral("/docs/not a valid id!.json"));
    QVERIFY(junk.open(QIODevice::WriteOnly));
    junk.write("{\"title\":\"Bad\"}");
    junk.close();
    const QVariantList docs = studio.listDocs();
    for (const QVariant &v : docs)
        QVERIFY(Doc::isValidId(v.toMap().value(QStringLiteral("id")).toString()));
}

void LeveleTests::movePageTracksActivePageAndClearsSelection()
{
    Studio studio;
    studio.newDocument(400, 400);
    const QString p0 = studio.currentPageId();
    studio.addPage();
    const QString p1 = studio.currentPageId();
    studio.addPage();
    const QString p2 = studio.currentPageId();

    // Active page p2 (index 2): move page 0 -> 1; active index stays 2 (p2).
    studio.movePage(0, 1);
    QCOMPARE(studio.currentPage(), 2);
    QCOMPARE(studio.currentPageId(), p2);

    // Move active page 2 -> 0; active index tracks p2 to 0.
    const QString el = studio.addElement(QStringLiteral("rect"));
    QVERIFY(studio.isSelected(el));
    studio.movePage(2, 0);
    QCOMPARE(studio.currentPage(), 0);
    QCOMPARE(studio.currentPageId(), p2);

    // Move non-active page 1 (p1) past active page 0 -> 2; active p2 remains at 0.
    Q_UNUSED(p0);
    Q_UNUSED(p1);
    studio.movePage(1, 2);
    QCOMPARE(studio.currentPage(), 0);
    QCOMPARE(studio.currentPageId(), p2);

    // Switch to page 1, then move page 0 -> 2; active shifts from 1 -> 0.
    studio.setCurrentPage(1);
    const QString activeId = studio.currentPageId();
    const QString el1 = studio.addElement(QStringLiteral("ellipse"));
    QVERIFY(studio.isSelected(el1));
    studio.movePage(0, 2);
    QCOMPARE(studio.currentPage(), 0);
    QCOMPARE(studio.currentPageId(), activeId);
}

void LeveleTests::imageGuardsRejectNonRegularAndLocked()
{
    Studio studio;
    studio.newDocument(400, 400);
    const QString devZeroId = studio.addImage(QUrl::fromLocalFile(QStringLiteral("/dev/zero")), 100, 100);
    QVERIFY(devZeroId.isEmpty());
    QVERIFY(studio.elements().isEmpty());

    const QString imgA = m_dir.filePath(QStringLiteral("guard-a.png"));
    const QString imgB = m_dir.filePath(QStringLiteral("guard-b.png"));
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QVERIFY(img.save(imgA, "PNG"));
    img.fill(Qt::green);
    QVERIFY(img.save(imgB, "PNG"));

    const QString id = studio.addImage(QUrl::fromLocalFile(imgA), 0, 0);
    QVERIFY(!id.isEmpty());
    const QString origPath = studio.elementById(id)->imagePath;

    QVERIFY(!studio.replaceImage(id, QUrl::fromLocalFile(QStringLiteral("/dev/zero"))));
    QCOMPARE(studio.elementById(id)->imagePath, origPath);

    studio.setElementLocked(id, true);
    QVERIFY(!studio.replaceImage(id, QUrl::fromLocalFile(imgB)));
    QCOMPARE(studio.elementById(id)->imagePath, origPath);
}

void LeveleTests::nudgeGuardsLockedAndResetsAcrossUndo()
{
    Studio studio;
    studio.newDocument(400, 400);
    const QString rectId = studio.addElement(QStringLiteral("rect"));
    studio.setElementLocked(rectId, true);
    studio.selectElement(rectId, false);

    CanvasItem canvas;
    canvas.setStudio(&studio);
    const double x0 = studio.elementById(rectId)->x;
    QSignalSpy contentsSpy(&studio, &Studio::contentsChanged);
    QSignalSpy historySpy(&studio, &Studio::historyChanged);
    canvas.nudge(10, 0);
    QCOMPARE(studio.elementById(rectId)->x, x0);
    QCOMPARE(contentsSpy.count(), 0);
    QCOMPARE(historySpy.count(), 0);

    // Unlocked element: nudge -> undo -> immediate nudge must NOT coalesce with undone step.
    studio.setElementLocked(rectId, false);
    canvas.nudge(5, 0);
    QVERIFY(studio.elementById(rectId) != nullptr);
    QCOMPARE(studio.elementById(rectId)->x, x0 + 5.0);
    QVERIFY(studio.canUndo());
    studio.undo();
    QVERIFY(studio.elementById(rectId) != nullptr);
    QCOMPARE(studio.elementById(rectId)->x, x0);

    canvas.nudge(7, 0);
    QCOMPARE(studio.elementById(rectId)->x, x0 + 7.0);
    QVERIFY(studio.canUndo());
    studio.undo();
    QVERIFY(studio.elementById(rectId) != nullptr);
    QCOMPARE(studio.elementById(rectId)->x, x0);
}

void LeveleTests::noOpUndoGuardsAndNewDocumentClamp()
{
    Studio studio;
    studio.newDocument(-50, 999999);
    QCOMPARE(studio.docWidth(), 16.0);
    QCOMPARE(studio.docHeight(), 16384.0);

    const QString a = studio.addElement(QStringLiteral("rect"));
    studio.selectElement(a, false);
    QSignalSpy historySpy(&studio, &Studio::historyChanged);
    QSignalSpy contentsSpy(&studio, &Studio::contentsChanged);

    // Single element: raise/lower/front/back are no-ops and must not push undo.
    studio.raiseSelected();
    studio.lowerSelected();
    studio.frontSelected();
    studio.backSelected();
    QCOMPARE(historySpy.count(), 0);
    QCOMPARE(contentsSpy.count(), 0);

    // Invalid or identical background color must not push undo.
    studio.setPageBackground(QStringLiteral("not-a-color"));
    QCOMPARE(studio.pageBackground(), QStringLiteral("#ffffff"));
    QCOMPARE(historySpy.count(), 0);
    studio.setPageBackground(QStringLiteral("#ffffff"));
    QCOMPARE(historySpy.count(), 0);

    studio.setPageBackground(QStringLiteral("#112233"));
    QCOMPARE(studio.pageBackground(), QStringLiteral("#112233"));
    QCOMPARE(historySpy.count(), 1);
}

void LeveleTests::jsonEmptyIdHostileWeightAndRotatedIntersects()
{
    QJsonObject el;
    el[QStringLiteral("id")] = QStringLiteral("");
    el[QStringLiteral("type")] = QStringLiteral("text");
    el[QStringLiteral("fontWeight")] = 1e20;

    QJsonObject page;
    page[QStringLiteral("id")] = QStringLiteral("");
    page[QStringLiteral("elements")] = QJsonArray{el};

    QJsonObject root;
    root[QStringLiteral("id")] = QStringLiteral("");
    root[QStringLiteral("width")] = 400;
    root[QStringLiteral("height")] = 400;
    root[QStringLiteral("pages")] = QJsonArray{page};

    const Doc d = Doc::fromJson(root);
    QVERIFY(Doc::isValidId(d.id));
    QVERIFY(!d.pages.first().id.isEmpty());
    QVERIFY(!d.pages.first().elements.first().id.isEmpty());
    QCOMPARE(d.pages.first().elements.first().fontWeight, 1000);

    // A 200x20 rect centered at (200, 200) rotated 90 deg spans y=[100..300], x=[190..210].
    Element rot = Element::make(QStringLiteral("rect"));
    rot.x = 100;
    rot.y = 190;
    rot.w = 200;
    rot.h = 20;
    rot.rotation = 90.0;
    QVERIFY(rot.intersectsRect(QRectF(195, 110, 10, 10)));
    QVERIFY(!rot.intersectsRect(QRectF(105, 195, 10, 10)));
}

void LeveleTests::renameAndSelLockedExposed()
{
    Studio studio;
    studio.newDocument(400, 400);
    const QString id = studio.addElement(QStringLiteral("rect"));
    QCOMPARE(studio.sel().value(QStringLiteral("locked")).toBool(), false);

    QSignalSpy selSpy(&studio, &Studio::selectionChanged);
    studio.renameElement(id, QStringLiteral("Hero Card"));
    QCOMPARE(selSpy.count(), 1);
    QCOMPARE(studio.sel().value(QStringLiteral("name")).toString(), QStringLiteral("Hero Card"));

    studio.setElementLocked(id, true);
    studio.selectElement(id, false);
    QCOMPARE(studio.sel().value(QStringLiteral("locked")).toBool(), true);
}

QTEST_MAIN(LeveleTests)
#include "levele_tests.moc"
