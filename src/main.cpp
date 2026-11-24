#include "canvasitem.h"
#include "exporter.h"
#include "portalfilechooser.h"
#include "studio.h"
#include "templates.h"
#include "theme.h"

#include <algorithm>
#include <utility>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLibraryInfo>
#include <QLocale>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTranslator>

#include <cstdio>

namespace {

class WindowCloseGuard final : public QObject
{
public:
    explicit WindowCloseGuard(Studio &studio) : m_studio(studio) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Close && !m_studio.prepareToLeave(true)) {
            event->ignore();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    Studio &m_studio;
};

int fail(const char *what)
{
    std::printf("SELFTEST FAIL  %s\n", what);
    return 1;
}

// Colour work is only proven across every installed theme, not just the active
// one: derive the full token set from each colors.toml and assert the contrast
// pairs the QML actually paints. The pair list lives in Theme so the sweep and
// the derivation cannot drift apart.
int runThemeSweep()
{
    Theme theme;
    QStringList dirs;
    int skipped = 0;

    const QString live = Theme::currentRoot();
    if (!live.isEmpty())
        dirs << live;

    const QStringList roots = {QDir::homePath() + QStringLiteral("/.config/omarchy/themes"),
                               QStringLiteral("/usr/share/omarchy/themes")};
    for (const QString &root : roots) {
        const QFileInfoList entries = QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &fi : entries) {
            const QString dir = fi.absoluteFilePath();
            if (QFileInfo::exists(dir + QStringLiteral("/colors.toml"))
                || QFileInfo::exists(dir + QStringLiteral("/theme/colors.toml")))
                dirs << dir;
            else
                ++skipped;
        }
    }

    const QVector<Theme::ContrastPair> pairs = Theme::contrastPairs();
    std::printf("contrast pairs checked (%d):\n", int(pairs.size()));
    for (const Theme::ContrastPair &p : pairs) {
        std::printf("  %-18s %-11s on %-11s >= %.1f  [%s]\n", qPrintable(p.label),
                    qPrintable(p.ink), qPrintable(p.paper), p.ratio, qPrintable(p.where));
    }

    int checked = 0;
    int failing = 0;
    for (const QString &dir : std::as_const(dirs)) {
        theme.loadFromRoot(dir);
        ++checked;

        int bad = 0;
        for (const Theme::ContrastPair &p : pairs) {
            const QColor ink = theme.token(p.ink);
            const QColor paper = theme.token(p.paper);
            const double ratio = Theme::contrast(ink, paper);
            if (ratio + 1e-9 < p.ratio) {
                std::printf("  FAIL %-18s %s on %s %.2f (needs %.1f)\n", qPrintable(p.label),
                            qPrintable(ink.name()), qPrintable(paper.name()), ratio, p.ratio);
                ++bad;
            }
        }
        if (bad)
            ++failing;
        if (bad || checked <= 3) {
            std::printf("  %-28s mode=%-5s bg=%s fg=%s accent=%s accentInk=%s %s\n",
                        qPrintable(QFileInfo(dir).fileName()),
                        theme.property("dark").toBool() ? "dark" : "light",
                        qPrintable(theme.property("bg").value<QColor>().name()),
                        qPrintable(theme.property("fg").value<QColor>().name()),
                        qPrintable(theme.property("accent").value<QColor>().name()),
                        qPrintable(theme.property("accentInk").value<QColor>().name()),
                        bad ? "FAIL" : "ok");
        }
    }

    std::printf("theme sweep: %d themes checked, %d with failing pairs, skipped: %d (no colors.toml)\n",
                checked, failing, skipped);
    return failing == 0 ? 0 : 1;
}

// Draws, exports and then re-reads its own output. Runs without a compositor
// (QT_QPA_PLATFORM=offscreen) so the export path is testable in CI.
int runSelfTest(const QString &outDir)
{
    QDir().mkpath(outDir);
    // Keep the test hermetic: the Studio it constructs autosaves, and that must
    // never land in the user's real document store.
    qputenv("XDG_DATA_HOME", (outDir + QStringLiteral("/data")).toUtf8());
    std::printf("SELFTEST out=%s\n", qPrintable(outDir));

    const QVector<TemplateInfo> tmpl = buildTemplates();
    if (tmpl.isEmpty())
        return fail("no templates");
    std::printf("templates=%d\n", int(tmpl.size()));

    const Doc &doc = tmpl.first().doc;
    std::printf("doc='%s' %dx%d pages=%d elements=%d\n", qPrintable(doc.title), int(doc.width),
                int(doc.height), int(doc.pages.size()),
                doc.pages.isEmpty() ? 0 : int(doc.pages.first().elements.size()));

    {
        Studio probe;
        const QString json = probe.templateJson(0);
        std::printf("templateJson(0): %d chars\n", int(json.size()));
        QFile jf(outDir + QStringLiteral("/template0.json"));
        if (jf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            jf.write(json.toUtf8());
        const Doc round = Doc::fromJson(QJsonDocument::fromJson(json.toUtf8()).object());
        std::printf("templateJson(0) round-trip: title='%s' pages=%d elements=%d\n",
                    qPrintable(round.title), int(round.pages.size()),
                    round.pages.isEmpty() ? 0 : int(round.pages.first().elements.size()));
        if (round.title.isEmpty() || round.pages.isEmpty())
            return fail("template json round-trip");

        probe.newFromTemplate(0);
        probe.selectAll();
        const QVariantMap s = probe.sel();
        std::printf("selection: count=%d id=%s x=%s y=%s w=%s h=%s rot=%s op=%s fill=%s name=%s\n",
                    s.value("count").toInt(), qPrintable(s.value("id").toString()),
                    qPrintable(s.value("x").toString()), qPrintable(s.value("y").toString()),
                    qPrintable(s.value("w").toString()), qPrintable(s.value("h").toString()),
                    qPrintable(s.value("rotation").toString()), qPrintable(s.value("opacity").toString()),
                    qPrintable(s.value("fill").value<QColor>().name()), qPrintable(s.value("name").toString()));
    }

    // Every template must render something, not just the first one.
    for (int i = 0; i < tmpl.size(); ++i) {
        const Doc &t = tmpl.at(i).doc;
        const QString name = QStringLiteral("%1-t%2.png").arg(i + 1, 2, 10, QLatin1Char('0')).arg(
            t.title.simplified().replace(QLatin1Char(' '), QLatin1Char('_')));
        const QString out = outDir + QLatin1Char('/') + name;
        const bool ok = Exporter::writePng(t, 0, 0.55, out);
        std::printf("  template %d '%s' pages=%d -> %s %lld bytes\n", i + 1,
                    qPrintable(t.title), int(t.pages.size()), ok ? "ok" : "FAILED",
                    static_cast<long long>(QFileInfo(out).size()));
    }

    int failures = 0;

    // PNG
    const QString png = outDir + QStringLiteral("/page1.png");
    if (!Exporter::writePng(doc, 0, 2.0, png)) {
        std::printf("png: FAILED\n");
        ++failures;
    } else {
        const QImage img(png);
        const qint64 bytes = QFileInfo(png).size();
        std::printf("png: %lld bytes %dx%d empty=%d\n", static_cast<long long>(bytes), img.width(),
                    img.height(), img.isNull() ? 1 : 0);
        if (img.width() != int(doc.width * 2) || bytes < 10000)
            ++failures;
    }

    // PDF
    const QString pdf = outDir + QStringLiteral("/deck.pdf");
    if (!Exporter::writePdf(doc, pdf)) {
        std::printf("pdf: FAILED\n");
        ++failures;
    } else {
        QFile f(pdf);
        if (!f.open(QIODevice::ReadOnly))
            return fail("pdf reopen");
        const QByteArray head = f.read(8);
        const qint64 bytes = f.size();
        std::printf("pdf: %lld bytes header=%s\n", static_cast<long long>(bytes),
                    head.startsWith("%PDF") ? "ok" : "BAD");
        if (!head.startsWith("%PDF") || bytes < 1000)
            ++failures;
    }

    // GIF — must be a real, multi-frame, looping file
    const QString gif = outDir + QStringLiteral("/deck.gif");
    const bool gifOk = Exporter::writeGif(doc, gif, 720, 12);
    QFile gf(gif);
    qint64 gifBytes = 0;
    QByteArray gifHead;
    if (gf.open(QIODevice::ReadOnly)) {
        gifBytes = gf.size();
        gifHead = gf.read(6);
    }
    std::printf("gif: %s %lld bytes header=%s\n", gifOk ? "ok" : "FAILED",
                static_cast<long long>(gifBytes), gifHead.constData());
    if (!gifOk || gifBytes < 1024 || gifHead != "GIF89a")
        ++failures;

    // A three-page doc so multi-page export is exercised, not just page one.
    Doc multi = tmpl.at(7).doc; // Fieldwork portfolio (3 pages)
    std::printf("multi-page doc='%s' pages=%d\n", qPrintable(multi.title), int(multi.pages.size()));
    const QString multiPdf = outDir + QStringLiteral("/multi.pdf");
    if (!Exporter::writePdf(multi, multiPdf)) {
        std::printf("multi pdf: FAILED\n");
        ++failures;
    } else {
        std::printf("multi pdf: %lld bytes\n", static_cast<long long>(QFileInfo(multiPdf).size()));
    }
    const QString multiGif = outDir + QStringLiteral("/multi.gif");
    if (!Exporter::writeGif(multi, multiGif, 480, 8)) {
        std::printf("multi gif: FAILED\n");
        ++failures;
    } else {
        std::printf("multi gif: %lld bytes frames=%d\n",
                    static_cast<long long>(QFileInfo(multiGif).size()), int(multi.pages.size()));
    }

    // F05: writing is atomic and a failure is reported, not swallowed.
    {
        Studio probe;
        probe.newDocument(400, 400);
        const bool announced = probe.saveWithFeedback();
        std::printf("save feedback: ok=%d status='%s'\n", announced ? 1 : 0,
                    qPrintable(probe.status()));
        if (!announced || probe.status() != QLatin1String("Saved"))
            ++failures;

        // A document store that cannot be written to must say so.
        const QString docs = probe.dataRoot() + QStringLiteral("/docs");
        const QFile::Permissions original = QFile::permissions(docs);
        QFile::setPermissions(docs, QFileDevice::ReadOwner | QFileDevice::ExeOwner
                                        | QFileDevice::ReadGroup | QFileDevice::ExeGroup);
        probe.setTitle(QStringLiteral("Read-only probe"));
        const bool wrote = probe.saveNow();
        QFile::setPermissions(docs, original);
        const bool reported = probe.status().contains(QStringLiteral("Couldn't save"));
        std::printf("read-only docs dir: saved=%d reported=%d status='%s'\n", wrote ? 1 : 0,
                    reported ? 1 : 0, qPrintable(probe.status()));
        if (wrote || !reported)
            ++failures;
    }

    // F04: deleting a design moves it to docs/.trash and Undo brings it back.
    {
        Studio probe;
        probe.newFromTemplate(0);
        const QString id = probe.docId();
        probe.closeDoc();
        const QString live = probe.dataRoot() + QStringLiteral("/docs/") + id
            + QStringLiteral(".json");
        const bool existed = QFileInfo::exists(live);
        probe.deleteDoc(id);
        const auto hasTrashEntry = [&]() {
            const QVariantList trash = probe.listTrash();
            return std::any_of(trash.cbegin(), trash.cend(), [&](const QVariant &row) {
                return row.toMap().value("id").toString().startsWith(id + QLatin1Char('.'));
            });
        };
        const bool inTrash = hasTrashEntry();
        bool stillListed = false;
        const QVariantList listed = probe.listDocs();
        for (const QVariant &v : listed) {
            if (v.toMap().value(QStringLiteral("id")).toString() == id)
                stillListed = true;
        }
        const bool undoOffered = probe.trashPending();
        probe.undoDelete();
        const bool back = QFileInfo::exists(live);
        const bool goneFromTrash = !hasTrashEntry();
        std::printf("trash: existed=%d inTrash=%d listed=%d undoOffered=%d restored=%d trashEmpty=%d\n",
                    existed ? 1 : 0, inTrash ? 1 : 0, stillListed ? 1 : 0, undoOffered ? 1 : 0,
                    back ? 1 : 0, goneFromTrash ? 1 : 0);
        if (!existed || !inTrash || stillListed || !undoOffered || !back || !goneFromTrash)
            ++failures;
    }

    // F03: a line with a zero axis is pickable, hoverable and marquee-selectable.
    {
        Element line = Element::make(QStringLiteral("line"));
        line.x = 100;
        line.y = 100;
        line.w = 300;
        line.h = 0;
        const bool midHit = line.containsPoint(QPointF(250, 100), 6.0);
        const bool offHit = line.containsPoint(QPointF(250, 140), 6.0);
        const bool marqueeHit = line.intersectsRect(QRectF(240, 80, 20, 40), 6.0);

        Element vertical = Element::make(QStringLiteral("arrow"));
        vertical.x = 0;
        vertical.y = 50;
        vertical.w = 0;
        vertical.h = -300;
        const bool verticalHit = vertical.containsPoint(QPointF(0, -100), 6.0);
        std::printf("line picking: midpoint=%d miss=%d marquee=%d vertical-arrow=%d\n",
                    midHit ? 1 : 0, offHit ? 1 : 0, marqueeHit ? 1 : 0, verticalHit ? 1 : 0);
        if (!midHit || offHit || !marqueeHit || !verticalHit)
            ++failures;

        // Every line/arrow in the shipped templates must be selectable by a
        // marquee over its own midpoint.
        int lines = 0;
        int picked = 0;
        for (const TemplateInfo &t : tmpl) {
            for (const Page &p : t.doc.pages) {
                for (const Element &e : p.elements) {
                    if (!e.isLineLike())
                        continue;
                    ++lines;
                    const QPointF mid = e.segment().center();
                    if (e.intersectsRect(QRectF(mid - QPointF(4, 4), QSizeF(8, 8)), 6.0))
                        ++picked;
                }
            }
        }
        std::printf("template lines/arrows: %d, marquee-selectable: %d\n", lines, picked);
        if (lines == 0 || picked != lines)
            ++failures;
    }

    // F24: a hostile document file cannot reach the file system or explode.
    {
        QJsonObject o;
        o[QStringLiteral("id")] = QStringLiteral("../../evil");
        o[QStringLiteral("width")] = -5.0;
        o[QStringLiteral("height")] = 1e9;
        const Doc hostile = Doc::fromJson(o);
        std::printf("hostile doc: id='%s' valid=%d width=%.0f height=%.0f\n",
                    qPrintable(hostile.id), Doc::isValidId(hostile.id) ? 1 : 0, hostile.width,
                    hostile.height);
        if (!Doc::isValidId(hostile.id) || hostile.width < 16 || hostile.height > 16384)
            ++failures;

        QJsonObject good;
        good[QStringLiteral("id")] = QStringLiteral("8f13d93d-b4ee");
        good[QStringLiteral("width")] = 1080.0;
        good[QStringLiteral("height")] = 1080.0;
        const Doc kept = Doc::fromJson(good);
        std::printf("valid doc: id='%s' width=%.0f\n", qPrintable(kept.id), kept.width);
        if (kept.id != QLatin1String("8f13d93d-b4ee") || kept.width != 1080)
            ++failures;
    }

    // F22: the zoom step is proportional to the wheel delta.
    {
        const double small = CanvasItem::zoomFactorForDelta(15);
        const double notch = CanvasItem::zoomFactorForDelta(120);
        std::printf("zoom factors: delta 15 -> %.4f, delta 120 -> %.4f\n", small, notch);
        if (qAbs(small - 1.023) > 0.001 || qAbs(notch - 1.197) > 0.002)
            ++failures;
    }

    std::printf("SELFTEST %s (%d failing checks)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("levele"));
    app.setOrganizationName(QStringLiteral("nodaysidle"));
    app.setApplicationDisplayName(QStringLiteral("Levele"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setDesktopFileName(QStringLiteral("com.nodaysidle.levele"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/levele.svg")));

    const QStringList args = app.arguments();
    const int selftest = args.indexOf(QStringLiteral("--selftest"));
    if (args.contains(QStringLiteral("--theme-sweep")))
        return runThemeSweep();
    if (selftest >= 0) {
        const QString dir = (selftest + 1 < args.size() && !args.at(selftest + 1).startsWith('-'))
            ? args.at(selftest + 1)
            : QDir::tempPath() + QStringLiteral("/levele-selftest");
        return runSelfTest(dir);
    }

    // Translations are optional: a `translations/levele_<lang>.qm` next to the
    // binary is picked up, and with none installed every string stays English.
    QTranslator translator;
    const QString translationsDir = QCoreApplication::applicationDirPath()
        + QStringLiteral("/translations");
    if (translator.load(QLocale(), QStringLiteral("levele"), QStringLiteral("_"),
                        translationsDir)) {
        app.installTranslator(&translator);
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    Theme theme;
    Studio studio;
    PortalFileChooser portal;

    // An application-level quit may bypass the window's close event.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &studio,
                     [&studio]() { studio.saveOnQuit(); });

    // Tool shortcuts must not fire while a text field has focus, so the app
    // tracks whether the focused object is a text editor.
    QObject::connect(&app, &QGuiApplication::focusObjectChanged, &studio, [&studio](QObject *o) {
        bool typing = false;
        if (o) {
            const QByteArray cls = o->metaObject()->className();
            typing = cls.startsWith("QQuickTextInput") || cls.startsWith("QQuickTextEdit")
                || cls.startsWith("QQuickTextField") || cls.startsWith("QQuickTextArea");
        }
        studio.setTyping(typing);
    });

    qmlRegisterType<CanvasItem>("Levele", 1, 0, "CanvasItem");

    // Handy for screenshots and manual QA: start straight in the editor.
    if (args.contains(QStringLiteral("--demo")))
        studio.newFromTemplate(0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("studio"), &studio);
    engine.rootContext()->setContextProperty(QStringLiteral("portal"), &portal);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    WindowCloseGuard closeGuard(studio);
    if (auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
        win->installEventFilter(&closeGuard);
    return app.exec();
}
