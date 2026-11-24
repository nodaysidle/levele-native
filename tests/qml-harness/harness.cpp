// Offscreen QML acceptance harness. Lives in /tmp, links the app's sources
// (never the GUI binary) and loads Main.qml from the working tree so QML edits
// are exercised without rebuilding resources.
#include "canvasitem.h"
#include "exporter.h"
#include "portalfilechooser.h"
#include "studio.h"
#include "theme.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQmlProperty>
#include <QAccessible>
#include <functional>
#include <QFont>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>
#include <QTemporaryDir>

#include <cstdio>

static int g_failures = 0;

class HarnessCloseGuard final : public QObject
{
public:
    explicit HarnessCloseGuard(Studio &studio) : m_studio(studio) {}

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

static void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

static QString eval(QObject *scope, const QString &js)
{
    QQmlExpression expr(qmlContext(scope), scope, js);
    const QVariant v = expr.evaluate();
    if (expr.hasError())
        return QStringLiteral("<error: %1>").arg(expr.error().toString());
    return v.toString();
}

static QObject *findByObjectName(QObject *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    const QList<QObject *> kids = root->findChildren<QObject *>();
    for (QObject *o : kids) {
        if (o->objectName() == name)
            return o;
    }
    return nullptr;
}

static QQuickItem *findItemByObjectName(QObject *root, const QString &name)
{
    QList<QQuickItem *> items;
    if (auto *window = qobject_cast<QQuickWindow *>(root))
        items << window->contentItem();
    else if (auto *item = qobject_cast<QQuickItem *>(root))
        items << item;
    while (!items.isEmpty()) {
        QQuickItem *item = items.takeLast();
        if (item->objectName() == name)
            return item;
        items << item->childItems();
    }
    return qobject_cast<QQuickItem *>(findByObjectName(root, name));
}

static void sendMouse(QQuickWindow *win, QEvent::Type type, const QPointF &pos,
                      Qt::MouseButton button, Qt::MouseButtons buttons,
                      Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent ev(type, pos, win->mapToGlobal(pos.toPoint()), button, buttons, mods);
    QCoreApplication::sendEvent(win, &ev);
}

static QString cls0(QQuickItem *item)
{
    return QString::fromLatin1(item->metaObject()->className());
}

static QJsonObject readDoc(const QString &docsDir, const QString &id)
{
    QFile f(docsDir + QLatin1Char('/') + id + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

int main(int argc, char **argv)
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("levele-harness"));

    const QStringList args = app.arguments();
    const QString scenario = args.value(1);
    if (scenario.isEmpty()) {
        std::printf("usage: harness <scenario> [qml dir]\n");
        return 2;
    }
    const QString qmlDir = args.value(2, QStringLiteral("/home/arch/dev/nodaysidle/levele-native/qml"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterType<CanvasItem>("Levele", 1, 0, "CanvasItem");

    // Isolate the harness from the user's real document store unless explicitly
    // opted out with LEVELE_HARNESS_KEEP_DATA=1.
    QTemporaryDir isolatedData;
    if (qEnvironmentVariableIsEmpty("LEVELE_HARNESS_KEEP_DATA") && isolatedData.isValid())
        qputenv("XDG_DATA_HOME", isolatedData.path().toUtf8());

    auto makeFixturePng = [](const QString &path, const QColor &colour) {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(colour);
        img.save(path, "PNG");
        return path;
    };

    Theme theme;
    Studio studio;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &studio,
                     [&studio]() { studio.saveOnQuit(); });

    PortalFileChooser portal;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("portal"), &portal);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("studio"), &studio);
    engine.load(QUrl::fromLocalFile(qmlDir + QStringLiteral("/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        std::printf("FAIL  Main.qml did not load\n");
        return 1;
    }
    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!win) {
        std::printf("FAIL  root object is not a window\n");
        return 1;
    }
    win->resize(1440, 900);
    HarnessCloseGuard closeGuard(studio);
    win->installEventFilter(&closeGuard);

    const QString docsDir = studio.dataRoot() + QStringLiteral("/docs");

    // --------------------------------------------------------- F03 + F13
    if (scenario == QLatin1String("f03-f13")) {
        studio.newDocument(800, 800);
        QTest::qWait(300);
        auto *canvas = findItemByObjectName(win, QStringLiteral("canvasStage"));
        check(canvas != nullptr, "canvas item found");
        if (!canvas)
            return 1;
        const QPointF origin = canvas->mapToScene(QPointF(0, 0));
        const qreal zoom = canvas->property("zoom").toReal();
        const qreal panX = canvas->property("panX").toReal();
        const qreal panY = canvas->property("panY").toReal();
        auto toWindow = [&](const QPointF &doc) {
            return QPointF(origin.x() + panX + doc.x() * zoom, origin.y() + panY + doc.y() * zoom);
        };

        // A 300x0 line, exactly what the Line tool and the template rules make.
        const QString id = studio.addElement(QStringLiteral("line"));
        studio.setSelProp(QStringLiteral("x"), 100.0);
        studio.setSelProp(QStringLiteral("y"), 300.0);
        studio.setSelProp(QStringLiteral("w"), 300.0);
        studio.setSelProp(QStringLiteral("h"), 0.0);
        QTest::qWait(50);
        check(canvas->metaObject()->indexOfMethod("hitTest(QPointF)") >= 0,
              "hitTest is invokable from QML");
        QString hit;
        QMetaObject::invokeMethod(canvas, "hitTest", Q_RETURN_ARG(QString, hit),
                                  Q_ARG(QPointF, QPointF(250, 300)));
        check(hit == id, "hitTest at the midpoint of a 300x0 line returns its id");
        QString miss;
        QMetaObject::invokeMethod(canvas, "hitTest", Q_RETURN_ARG(QString, miss),
                                  Q_ARG(QPointF, QPointF(250, 380)));
        check(miss.isEmpty(), "hitTest 80 px off the line selects nothing");

        // Marquee across the line selects it.
        studio.clearSelection();
        sendMouse(win, QEvent::MouseButtonPress, toWindow(QPointF(80, 250)), Qt::LeftButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseMove, toWindow(QPointF(200, 300)), Qt::NoButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseMove, toWindow(QPointF(420, 350)), Qt::NoButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseButtonRelease, toWindow(QPointF(420, 350)), Qt::LeftButton,
                  Qt::NoButton);
        QTest::qWait(20);
        check(studio.isSelected(id), "marquee over a zero-height line selects it");

        // F13: the Line tool keeps the drawn vector (300x2, not 240x0).
        studio.clearSelection();
        canvas->setProperty("tool", QStringLiteral("line"));
        QTest::qWait(20);
        const int before = studio.elements().size();
        sendMouse(win, QEvent::MouseButtonPress, toWindow(QPointF(100, 100)), Qt::LeftButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseMove, toWindow(QPointF(250, 101)), Qt::NoButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseMove, toWindow(QPointF(400, 102)), Qt::NoButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseButtonRelease, toWindow(QPointF(400, 102)), Qt::LeftButton,
                  Qt::NoButton);
        QTest::qWait(20);
        check(studio.elements().size() == before + 1, "Line tool created one element");
        if (studio.elements().size() > before) {
            const Element &drawn = studio.elements().last();
            std::printf("  drawn line: x=%.1f y=%.1f w=%.1f h=%.1f\n", drawn.x, drawn.y, drawn.w,
                        drawn.h);
            check(qAbs(drawn.w - 300.0) < 6.0 && qAbs(drawn.h - 2.0) < 6.0,
                  "drawn vector kept: w=300 h=2 (not 240x0)");
        }

        // A horizontal drag with Shift snaps to 45° steps: (300,2) stays flat.
        canvas->setProperty("tool", QStringLiteral("arrow"));
        const int before2 = studio.elements().size();
        sendMouse(win, QEvent::MouseButtonPress, toWindow(QPointF(100, 600)), Qt::LeftButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseMove, toWindow(QPointF(400, 605)), Qt::NoButton,
                  Qt::LeftButton, Qt::ShiftModifier);
        sendMouse(win, QEvent::MouseButtonRelease, toWindow(QPointF(400, 605)), Qt::LeftButton,
                  Qt::NoButton, Qt::ShiftModifier);
        QTest::qWait(20);
        if (studio.elements().size() == before2 + 1) {
            const Element &a = studio.elements().last();
            std::printf("  shift arrow: w=%.1f h=%.1f\n", a.w, a.h);
            check(qAbs(a.h) < 0.01, "Shift snaps the arrow to horizontal");
        }

        std::printf("scenario f03-f13: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ---------------------------------------------------------------- F01
    if (scenario == QLatin1String("f01-quit-save")) {
        studio.newFromTemplate(0);
        QTest::qWait(200);
        const QString id = studio.docId();
        QObject *titleField = findByObjectName(win, QStringLiteral("titleField"));
        check(titleField != nullptr, "title field found");
        if (titleField) {
            // Typing does not commit; only focus loss does.
            titleField->setProperty("text", QStringLiteral("Closed in a hurry"));
        }
        studio.selectAll();
        std::printf("  selectionCount=%d\n", studio.selectionCount());
        studio.setSelProp(QStringLiteral("x"), 12.5);
        std::printf("  after move: first x=%.2f\n", studio.elements().isEmpty() ? -1.0 : studio.elements().first().x);
        // Close inside the 800 ms autosave window.
        QTest::qWait(50);
        std::printf("  before close: visible=%d\n", win->isVisible() ? 1 : 0);
        win->close();
        std::printf("  after close: visible=%d title on disk: see checks\n",
                    win->isVisible() ? 1 : 0);
        const QJsonObject onDisk = readDoc(docsDir, id);
        check(onDisk.value(QStringLiteral("title")).toString()
                  == QLatin1String("Closed in a hurry"),
              "title typed then closed is on disk");
        const QJsonArray pages = onDisk.value(QStringLiteral("pages")).toArray();
        check(!pages.isEmpty(), "pages written");
        if (!pages.isEmpty()) {
            const QJsonArray els = pages.first().toObject().value(QStringLiteral("elements")).toArray();
            bool found = false;
            for (const QJsonValue &v : els) {
                if (qAbs(v.toObject().value(QStringLiteral("x")).toDouble() - 12.5) < 0.001)
                    found = true;
            }
            check(found, "element moved 50 ms before close is on disk");
        }
        std::printf("scenario f01-quit-save: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ----------------------------------------------------------------- grab
    if (scenario == QLatin1String("grab")) {
        QTest::qWait(400);
        win->grabWindow().save(QStringLiteral("/tmp/grab-dashboard.png"));
        studio.newFromTemplate(0);
        QTest::qWait(500);
        studio.selectAll();
        QTest::qWait(200);
        win->grabWindow().save(QStringLiteral("/tmp/grab-editor.png"));
        std::printf("grabbed /tmp/grab-dashboard.png and /tmp/grab-editor.png\n");
        return 0;
    }

    // ---------------------------------------------------------- QML load check
    if (scenario == QLatin1String("check-qml")) {
        const QStringList files = {QStringLiteral("Main.qml"), QStringLiteral("Dashboard.qml"),
                                   QStringLiteral("Editor.qml"), QStringLiteral("CanvasStage.qml"),
                                   QStringLiteral("LayersPanel.qml"), QStringLiteral("Inspector.qml"),
                                   QStringLiteral("PageStrip.qml"), QStringLiteral("ExportDialog.qml"),
                                   QStringLiteral("ShortcutsOverlay.qml"),
                                   QStringLiteral("ui/Btn.qml"), QStringLiteral("ui/IconBtn.qml"),
                                   QStringLiteral("ui/AlignBtn.qml"),
                                   QStringLiteral("ui/NumField.qml"), QStringLiteral("ui/ColorChip.qml"),
                                   QStringLiteral("ui/Combo.qml"), QStringLiteral("ui/Section.qml"),
                                   QStringLiteral("ui/Caption.qml"), QStringLiteral("ui/Toggle.qml")};
        for (const QString &f : files) {
            QQmlComponent component(&engine, QUrl::fromLocalFile(qmlDir + QLatin1Char('/') + f));
            if (component.isError()) {
                for (const QQmlError &e : component.errors())
                    std::printf("  %s: %s\n", qPrintable(f), qPrintable(e.toString()));
                ++g_failures;
            } else {
                std::printf("  ok   %s\n", qPrintable(f));
            }
        }
        std::printf("scenario check-qml: %d files with errors\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------- F22 (wheel/touchpad) + F25 (inline editor)
    if (scenario == QLatin1String("f22-f25")) {
        studio.newDocument(800, 800);
        QTest::qWait(400);
        auto *canvas = findItemByObjectName(win, QStringLiteral("canvasStage"));
        check(canvas != nullptr, "canvas item found");
        if (!canvas)
            return 1;
        const QPointF origin = canvas->mapToScene(QPointF(0, 0));
        QPointF centre = origin + QPointF(canvas->width() / 2.0, canvas->height() / 2.0);

        // --- F22: a touchpad reports pixel deltas, a mouse reports angle deltas.
        const qreal panY0 = canvas->property("panY").toReal();
        {
            QWheelEvent wheel(centre, win->mapToGlobal(centre.toPoint()), QPoint(0, 7),
                              QPoint(0, 0), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                              false);
            QCoreApplication::sendEvent(win, &wheel);
        }
        QTest::qWait(30);
        const qreal panY1 = canvas->property("panY").toReal();
        std::printf("  pixel-delta pan: panY %.1f -> %.1f\n", panY0, panY1);
        check(qAbs(panY1 - (panY0 + 7.0)) < 0.01, "pan follows pixelDelta exactly (touchpad)");

        {
            QWheelEvent wheel(centre, win->mapToGlobal(centre.toPoint()), QPoint(),
                              QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                              false);
            QCoreApplication::sendEvent(win, &wheel);
        }
        QTest::qWait(30);
        check(qAbs(canvas->property("panY").toReal() - (panY1 + 72.0)) < 0.01,
              "angle-notch pan is 0.6 px per unit");

        // 15-unit steps (a touchpad pinch/scroll) must zoom gently, not 12 %.
        const qreal zoom0 = canvas->property("zoom").toReal();
        {
            QWheelEvent wheel(centre, win->mapToGlobal(centre.toPoint()), QPoint(), QPoint(0, 15),
                              Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(win, &wheel);
        }
        QTest::qWait(30);
        const qreal zoom1 = canvas->property("zoom").toReal();
        std::printf("  ctrl+15 zoom: %.4f -> %.4f (ratio %.4f)\n", zoom0, zoom1, zoom1 / zoom0);
        check(qAbs(zoom1 / zoom0 - 1.0227) < 0.002, "a 15-unit wheel step zooms ~2.3 %, not 12 %");

        // --- F25: the inline editor follows the element's typography.
        studio.newDocument(800, 800);
        QTest::qWait(150);
        const QString id = studio.addElementAt(QStringLiteral("text"), QPointF(100, 100));
        studio.setSelProp(QStringLiteral("text"), QStringLiteral("one\ntwo\nthree\nfour"));
        studio.setSelProp(QStringLiteral("fontSize"), 40.0);
        studio.setSelProp(QStringLiteral("lineHeight"), 1.0);
        studio.setSelProp(QStringLiteral("letterSpacing"), 4.0);
        QTest::qWait(100);
        canvas->metaObject()->invokeMethod(canvas, "beginEditText", Q_ARG(QString, id));
        QTest::qWait(200);
        auto *editor = findItemByObjectName(win, QStringLiteral("inlineEditor"));
        check(editor != nullptr, "inline text editor found");
        if (!editor)
            return 1;
        const qreal tight = editor->property("contentHeight").toReal();
        const int weight = editor->property("font").value<QFont>().weight();
        const qreal spacing = editor->property("font").value<QFont>().letterSpacing();
        QTest::qWait(50);
        studio.setSelProp(QStringLiteral("lineHeight"), 2.0);
        QTest::qWait(250);
        const qreal loose = editor->property("contentHeight").toReal();
        std::printf("  editor: contentHeight %.0f at lineHeight 1.0 -> %.0f at 2.0 "
                    "(weight %d, spacing %.1f)\n", tight, loose, weight, spacing);
        check(loose > tight * 1.6, "the inline editor applies the element's line height");
        check(weight == 600, "the inline editor applies the element's weight");
        // Letter spacing is scaled to screen pixels like the font size.
        check(qAbs(spacing - 4.0 * canvas->property("zoom").toReal()) < 0.6,
              "the inline editor applies letter spacing");
        canvas->metaObject()->invokeMethod(canvas, "cancelEditText");

        std::printf("scenario f22-f25: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------- F09 / F10 / F21 (perf + import)
    if (scenario == QLatin1String("f09-f10-f21")) {
        studio.newFromTemplate(7); // Fieldwork portfolio: 3 pages, text-heavy
        QTest::qWait(400);
        auto *canvas = findItemByObjectName(win, QStringLiteral("canvasStage"));
        if (!canvas)
            return 1;

        int selectionChanged = 0;
        QObject::connect(&studio, &Studio::selectionChanged, &studio,
                         [&selectionChanged]() { ++selectionChanged; });

        // Paint count per preview item, so the check is "per thumbnail".
        auto previewPaints = [&]() {
            QHash<QQuickItem *, int> map;
            QList<QQuickItem *> stack;
            stack << win->contentItem();
            while (!stack.isEmpty()) {
                QQuickItem *item = stack.takeLast();
                stack << item->childItems();
                const QMetaObject *mo = item->metaObject();
                if (mo->indexOfMethod("paintCount()") < 0)
                    continue;
                if (mo->indexOfProperty("preview") < 0 || !item->property("preview").toBool())
                    continue;
                int n = 0;
                QMetaObject::invokeMethod(item, "paintCount", Q_RETURN_ARG(int, n));
                map.insert(item, n);
            }
            return map;
        };

        // Grab an element and drag it 200 times. Geometry is read *after* the
        // view has fitted itself to the new document.
        studio.selectAll();
        QTest::qWait(120);
        win->grabWindow();
        const QHash<QQuickItem *, int> paintsBefore = previewPaints();
        selectionChanged = 0;

        const QPointF origin = canvas->mapToScene(QPointF(0, 0));
        const qreal zoom = canvas->property("zoom").toReal();
        const qreal panX = canvas->property("panX").toReal();
        const qreal panY = canvas->property("panY").toReal();
        auto toWindow = [&](const QPointF &doc) {
            return QPointF(origin.x() + panX + doc.x() * zoom, origin.y() + panY + doc.y() * zoom);
        };
        const Element *prim = studio.primaryElement();
        const QPointF grab = prim ? QPointF(prim->x + prim->w / 2.0, prim->y + prim->h / 2.0)
                                  : QPointF(200, 200);
        std::printf("  grabbing at doc %.0f,%.0f (zoom %.2f)\n", grab.x(), grab.y(), zoom);
        const QPointF start = toWindow(grab);
        sendMouse(win, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int i = 0; i < 200; ++i)
            sendMouse(win, QEvent::MouseMove, start + QPointF(i * 0.5, i * 0.3), Qt::NoButton,
                      Qt::LeftButton);
        sendMouse(win, QEvent::MouseButtonRelease, start + QPointF(100, 60), Qt::LeftButton,
                  Qt::NoButton);
        QTest::qWait(100);
        win->grabWindow();
        const QHash<QQuickItem *, int> paintsAfter = previewPaints();
        int worst = 0;
        for (auto it = paintsAfter.begin(); it != paintsAfter.end(); ++it)
            worst = qMax(worst, it.value() - paintsBefore.value(it.key(), 0));
        std::printf("  200 moves: selectionChanged=%d, worst repaints of one thumbnail=%d "
                    "(%lld thumbnails)\n", selectionChanged, worst, (long long)paintsAfter.size());
        check(selectionChanged <= 2, "200 synthetic moves emit selectionChanged at most twice");
        check(worst <= 2, "200 synthetic moves repaint each page thumbnail at most twice");

        // --- F10: export runs off the GUI thread and reports progress.
        const QString pngPath = QStringLiteral("/tmp/levele-harness-export.png");
        QFile::remove(pngPath);
        studio.exportPng(QUrl::fromLocalFile(pngPath), 0, 2.0);
        const bool busy = studio.exporting();
        const QString busyStatus = studio.status();
        std::printf("  export started: exporting=%d status='%s'\n", busy ? 1 : 0,
                    qPrintable(busyStatus));
        check(busy && busyStatus.contains(QStringLiteral("Exporting")),
              "export reports progress while it runs");
        for (int i = 0; i < 100 && studio.exporting(); ++i)
            QTest::qWait(50);
        std::printf("  export finished: exporting=%d status='%s' bytes=%lld\n",
                    studio.exporting() ? 1 : 0, qPrintable(studio.status()),
                    (long long)QFileInfo(pngPath).size());
        check(!studio.exporting(), "export finished");
        check(QFileInfo(pngPath).size() > 1000, "the PNG was written");
        check(studio.status().startsWith(QStringLiteral("Exported PNG")), "success is reported");

        // A path that cannot be written is refused with a reason.
        studio.exportPng(QUrl::fromLocalFile(QStringLiteral("/proc/levele/nope.png")), 0, 1.0);
        std::printf("  unwritable: exporting=%d status='%s'\n", studio.exporting() ? 1 : 0,
                    qPrintable(studio.status()));
        check(!studio.exporting() && studio.status().contains(QStringLiteral("not writable")),
              "an unwritable target is reported, not silently attempted");

        // --- F21: a non-image is refused and changes nothing.
        studio.newDocument(600, 600);
        QTest::qWait(100);
        const int countBefore = studio.elements().size();
        const QString added = studio.addImage(QUrl::fromLocalFile(QStringLiteral("/etc/hostname")), 0, 0);
        std::printf("  addImage(/etc/hostname): id='%s' status='%s' count=%d->%d\n",
                    qPrintable(added), qPrintable(studio.status()), countBefore,
                    int(studio.elements().size()));
        check(added.isEmpty() && studio.elements().size() == countBefore,
              "a non-image is not added");
        check(studio.status().contains(QStringLiteral("Not an image")),
              "a non-image reports why");

        // A missing file renders a placeholder instead of nothing.
        {
            QTemporaryDir fixtureDir;
            const QString imgA = makeFixturePng(fixtureDir.filePath(QStringLiteral("levele-img-a.png")),
                                                QColor(0x33, 0x66, 0x99));
            const QString id = studio.addImage(QUrl::fromLocalFile(imgA), 0, 0);
            Element *img = studio.elementById(id);
            check(img != nullptr, "image added for the placeholder check");
            if (img) {
                const QString saved = img->imagePath;
                img->imagePath = QStringLiteral("/tmp/definitely-not-here.png");
                const QImage rendered = Exporter::renderPage(studio.doc(), 0, 0.5);
                check(!rendered.isNull(), "page with a missing image still renders");
                // The placeholder border is grey: look for a non-background pixel
                // inside the element box.
                const QRect box(int(img->x * 0.5) + 2, int(img->y * 0.5) + 2,
                                qMax(2, int(img->w * 0.5) - 4), qMax(2, int(img->h * 0.5) - 4));
                bool drew = false;
                for (int y = box.top(); y < box.bottom() && !drew; ++y)
                    for (int x = box.left(); x < box.right(); ++x)
                        if (rendered.pixelColor(x, y) != QColor(Qt::white)) {
                            drew = true;
                            break;
                        }
                check(drew, "a missing image draws a visible placeholder");
                img->imagePath = saved;
            }
        }

        std::printf("scenario f09-f10-f21: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------------ F16 (strip) + F20 (layers)
    if (scenario == QLatin1String("f16-f20")) {
        // A narrow window, so 15 pages cannot all fit in the strip.
        win->resize(800, 900);
        studio.newDocument(400, 400);
        QTest::qWait(300);

        // ---- F16: 15 pages must be reachable in a strip narrower than they are.
        for (int i = 0; i < 14; ++i)
            studio.addPage();
        QTest::qWait(200);
        auto *pages = findItemByObjectName(win, QStringLiteral("pageListView"));
        check(pages != nullptr, "page strip is a ListView");
        if (!pages)
            return 1;
        check(studio.pageCount() == 15, "15 pages in the document");
        const qreal contentW = pages->property("contentWidth").toReal();
        const qreal viewW = pages->property("width").toReal();
        std::printf("  strip: content %.0f px in a %.0f px view\n", contentW, viewW);
        check(contentW > viewW, "15 pages overflow the strip (so it must scroll)");
        pages->setProperty("contentX", contentW - viewW);
        QTest::qWait(100);
        QQuickItem *lastPage = nullptr;
        QMetaObject::invokeMethod(pages, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, lastPage),
                                  Q_ARG(int, 14));
        check(lastPage != nullptr, "page 15 is instantiated after scrolling to the end");

        // Drag page 1 onto page 3 and check the order changed.
        QList<QString> order;
        for (const Page &p : studio.doc().pages)
            order << p.id;
        QQuickItem *first = nullptr;
        QQuickItem *third = nullptr;
        pages->setProperty("contentX", 0.0);
        QTest::qWait(100);
        QMetaObject::invokeMethod(pages, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, first),
                                  Q_ARG(int, 0));
        QMetaObject::invokeMethod(pages, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, third),
                                  Q_ARG(int, 2));
        check(first && third, "page chips are instantiated");
        if (first && third) {
            const QPointF from = first->mapToScene(QPointF(38, 38));
            const QPointF to = third->mapToScene(QPointF(38, 38));
            sendMouse(win, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
            sendMouse(win, QEvent::MouseMove, QPointF(from.x() + 20, from.y()), Qt::NoButton,
                      Qt::LeftButton);
            sendMouse(win, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
            sendMouse(win, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
            QTest::qWait(50);
            QList<QString> after;
            for (const Page &p : studio.doc().pages)
                after << p.id;
            std::printf("  page order 0..2 before=%s%s%s after=%s%s%s\n",
                        qPrintable(order.value(0).left(4)), qPrintable(order.value(1).left(4)),
                        qPrintable(order.value(2).left(4)), qPrintable(after.value(0).left(4)),
                        qPrintable(after.value(1).left(4)), qPrintable(after.value(2).left(4)));
            check(after != order, "dragging a page chip reorders pages (movePage)");
        }

        // ---- F20: 40 layers, drag the top row down five rows.
        studio.newDocument(400, 400);
        QTest::qWait(200);
        for (int i = 0; i < 40; ++i)
            studio.addElementAt(QStringLiteral("rect"), QPointF(10 + i, 10));
        studio.clearSelection();
        QTest::qWait(300);
        auto *layers = findItemByObjectName(win, QStringLiteral("layerList"));
        check(layers != nullptr, "layer list found");
        if (!layers)
            return 1;
        check(layers->property("contentHeight").toReal() > layers->property("height").toReal(),
              "40 layers make the list scrollable (the F20 precondition)");
        check(studio.elements().size() == 40, "40 elements");

        QList<QString> layerOrder;
        for (const Element &e : std::as_const(studio.elements()))
            layerOrder << e.id;
        QMetaObject::invokeMethod(layers, "positionViewAtIndex", Q_ARG(int, 39), Q_ARG(int, 1));
        QTest::qWait(100);
        QQuickItem *topRow = nullptr;
        QQuickItem *targetRow = nullptr;
        QMetaObject::invokeMethod(layers, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, topRow),
                                  Q_ARG(int, 39));
        QMetaObject::invokeMethod(layers, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, targetRow),
                                  Q_ARG(int, 37));
        check(topRow && targetRow, "layer rows are instantiated");
        if (topRow && targetRow) {
            const QPointF from = topRow->mapToScene(QPointF(60, 15));
            const QPointF to = targetRow->mapToScene(QPointF(60, 15));
            sendMouse(win, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
            for (int step = 1; step <= 5; ++step)
                sendMouse(win, QEvent::MouseMove,
                          from + (to - from) * (step / 5.0), Qt::NoButton, Qt::LeftButton);
            sendMouse(win, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
            QTest::qWait(80);
            QList<QString> afterOrder;
            for (const Element &e : std::as_const(studio.elements()))
                afterOrder << e.id;
            std::printf("  layer 39 now at index %d\n", int(afterOrder.indexOf(layerOrder.value(39))));
            check(afterOrder != layerOrder, "dragging a layer row reorders (preventStealing works)");
        }

        std::printf("scenario f16-f20: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------- F11 / F14 / F15 / F25 / F26 (editor UX)
    if (scenario == QLatin1String("f11-f26")) {
        studio.newDocument(800, 800);
        QTest::qWait(300);
        auto *canvas = findItemByObjectName(win, QStringLiteral("canvasStage"));
        if (!canvas)
            return 1;
        const QPointF origin = canvas->mapToScene(QPointF(0, 0));
        const qreal zoom = canvas->property("zoom").toReal();
        const qreal panX = canvas->property("panX").toReal();
        const qreal panY = canvas->property("panY").toReal();
        auto toWindow = [&](const QPointF &doc) {
            return QPointF(origin.x() + panX + doc.x() * zoom, origin.y() + panY + doc.y() * zoom);
        };

        // --- F11: the Text tool inserts where it is clicked.
        const int before = studio.elements().size();
        canvas->setProperty("tool", QStringLiteral("text"));
        QTest::qWait(20);
        sendMouse(win, QEvent::MouseButtonPress, toWindow(QPointF(100, 100)), Qt::LeftButton,
                  Qt::LeftButton);
        sendMouse(win, QEvent::MouseButtonRelease, toWindow(QPointF(100, 100)), Qt::LeftButton,
                  Qt::NoButton);
        QTest::qWait(30);
        check(studio.elements().size() == before + 1, "text tool added one element");
        if (studio.elements().size() > before) {
            const Element &t = studio.elements().last();
            std::printf("  text at %.1f,%.1f\n", t.x, t.y);
            check(qAbs(t.x - 100) < 0.6 && qAbs(t.y - 100) < 0.6,
                  "text inserted at the press point (100,100), not the page centre");
        }

        // --- F26: a text element shows no Fill section; Text is shown.
        studio.selectAll();
        QTest::qWait(50);
        QStringList sections;
        QList<QQuickItem *> stack;
        stack << win->contentItem();
        while (!stack.isEmpty()) {
            QQuickItem *item = stack.takeLast();
            stack << item->childItems();
            if (item->metaObject()->indexOfProperty("title") >= 0
                && item->metaObject()->indexOfProperty("expanded") >= 0) {
                sections << QStringLiteral("%1=%2").arg(item->property("title").toString(),
                                                        item->property("visible").toBool() ? "shown" : "hidden");
            }
        }
        std::printf("  sections with a text selected: %s\n", qPrintable(sections.join(", ")));
        check(sections.contains(QStringLiteral("Fill=hidden")), "text selection hides the Fill section");
        check(sections.contains(QStringLiteral("Text=shown")), "text selection shows the Text section");

        studio.setSelProp(QStringLiteral("textVAlign"), QStringLiteral("middle"));
        QTest::qWait(20);
        check(studio.sel().value(QStringLiteral("textVAlign")).toString()
                  == QLatin1String("middle"),
              "textVAlign can be set from the Inspector control");
        studio.setSelProp(QStringLiteral("textAlign"), QStringLiteral("justify"));
        check(studio.sel().value(QStringLiteral("textAlign")).toString()
                  == QLatin1String("justify"),
              "justify is a selectable alignment");

        // --- F14: replacing an image keeps the element, not adds one.
        studio.newDocument(800, 800);
        QTest::qWait(200);
        QTemporaryDir replaceFixtureDir;
        const QString imgA = makeFixturePng(replaceFixtureDir.filePath(QStringLiteral("levele-img-a.png")),
                                            QColor(0x33, 0x66, 0x99));
        const QString imgB = makeFixturePng(replaceFixtureDir.filePath(QStringLiteral("levele-img-b.png")),
                                            QColor(0x99, 0x33, 0x66));
        const QString first = studio.addImage(QUrl::fromLocalFile(imgA), 0, 0);
        check(!first.isEmpty(), "image imported");
        studio.setSelProp(QStringLiteral("x"), 40.0);
        studio.setSelProp(QStringLiteral("y"), 60.0);
        studio.setSelProp(QStringLiteral("w"), 300.0);
        studio.setSelProp(QStringLiteral("h"), 150.0);
        const int countBefore = studio.elements().size();
        const bool replaced = studio.replaceImage(first, QUrl::fromLocalFile(imgB));
        const Element *img = studio.elementById(first);
        std::printf("  replace: ok=%d count=%d->%d x=%.0f y=%.0f w=%.0f h=%.0f\n", replaced ? 1 : 0,
                    countBefore, int(studio.elements().size()), img ? img->x : -1, img ? img->y : -1,
                    img ? img->w : -1, img ? img->h : -1);
        check(replaced, "replaceImage reports success");
        check(studio.elements().size() == countBefore, "replace kept the element count");
        check(img && qAbs(img->x - 40) < 0.5 && qAbs(img->y - 60) < 0.5
                  && qAbs(img->w - 300) < 0.5 && qAbs(img->h - 150) < 0.5,
              "replace kept position and size");
        check(img && img->imagePath.contains(QStringLiteral("levele-img-b")) == false,
              "replace points at a copied asset, not the source path");
        check(img && QFileInfo::exists(img->imagePath), "the replaced asset exists in the doc store");

        // --- F15: multi-select X is a delta from the primary.
        studio.newDocument(800, 800);
        QTest::qWait(100);
        const QString a = studio.addElementAt(QStringLiteral("rect"), QPointF(0, 0));
        const QString b = studio.addElementAt(QStringLiteral("rect"), QPointF(200, 0));
        studio.selectElement(a, false);
        studio.selectElement(b, true);
        studio.setSelProp(QStringLiteral("x"), 50.0);
        const Element *ea = studio.elementById(a);
        const Element *eb = studio.elementById(b);
        std::printf("  multi X: %.1f and %.1f (expected 50 and 250)\n", ea ? ea->x : -1,
                    eb ? eb->x : -1);
        check(ea && eb && qAbs(ea->x - 50) < 0.5 && qAbs(eb->x - 250) < 0.5,
              "X applies as a delta, elements keep their spacing");
        studio.setSelProp(QStringLiteral("rotation"), 45.0);
        check(ea && eb && qAbs(ea->rotation - 45) < 0.5 && qAbs(eb->rotation - 45) < 0.5,
              "absolute properties still apply to every selected element");
        check(studio.selMixed("x"), "selMixed reports differing X values");
        check(!studio.selMixed("rotation"), "selMixed is false once the values agree");

        std::printf("scenario f11-f26: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------- F12 / F08 / F07 (keys, undo)
    if (scenario == QLatin1String("f12-f08-f07")) {
        studio.newFromTemplate(0);
        QTest::qWait(300);
        auto *canvas = findItemByObjectName(win, QStringLiteral("canvasStage"));
        check(canvas != nullptr, "canvas item found");
        if (!canvas)
            return 1;
        const QPointF origin = canvas->mapToScene(QPointF(0, 0));
        const qreal zoom = canvas->property("zoom").toReal();
        const qreal panX = canvas->property("panX").toReal();
        const qreal panY = canvas->property("panY").toReal();
        auto toWindow = [&](const QPointF &doc) {
            return QPointF(origin.x() + panX + doc.x() * zoom, origin.y() + panY + doc.y() * zoom);
        };

        // --- F12: space release must drop the pan modifier.
        studio.clearSelection();
        QTest::keyPress(win, Qt::Key_Space);
        QTest::keyRelease(win, Qt::Key_Space);
        const QPointF clickDoc(620 + 240, 360 + 240); // centre of the ellipse in template 1
        QTest::mouseClick(win, Qt::LeftButton, Qt::NoModifier, toWindow(clickDoc).toPoint());
        QTest::qWait(30);
        std::printf("  selection after space+click: %d\n", studio.selectionCount());
        check(studio.selectionCount() == 1, "click after Space selects (does not pan)");

        // --- F08: clicks that do not move must not create undo steps.
        while (studio.canUndo())
            studio.undo();
        check(!studio.canUndo(), "history is empty before the click test");
        for (int i = 0; i < 10; ++i) {
            QTest::mouseClick(win, Qt::LeftButton, Qt::NoModifier, toWindow(clickDoc).toPoint());
            QTest::qWait(10);
        }
        std::printf("  canUndo after 10 clicks: %d\n", studio.canUndo() ? 1 : 0);
        check(!studio.canUndo(), "10 plain clicks create no undo steps");

        // --- F08: a burst of nudges is one step.
        const double x0 = studio.sel().value(QStringLiteral("x")).toDouble();
        for (int i = 0; i < 10; ++i)
            canvas->metaObject()->invokeMethod(canvas, "nudge", Q_ARG(qreal, 1.0), Q_ARG(qreal, 0.0));
        QTest::qWait(20);
        const double x1 = studio.sel().value(QStringLiteral("x")).toDouble();
        check(qAbs(x1 - (x0 + 10)) < 0.001, "ten nudges moved the element 10 px");
        studio.undo();
        QTest::qWait(20);
        const double x2 = studio.sel().value(QStringLiteral("x")).toDouble();
        std::printf("  x before=%.1f after nudges=%.1f after one undo=%.1f\n", x0, x1, x2);
        check(qAbs(x2 - x0) < 0.001, "one undo reverts the whole nudge burst");
        check(!studio.canUndo(), "the burst was a single undo step");

        // --- F07: undo keeps the view.
        studio.setSelProp(QStringLiteral("x"), x0 + 40);
        QTest::qWait(20);
        canvas->setProperty("zoom", 3.0);
        canvas->setProperty("panX", 11.0);
        QTest::qWait(20);
        studio.undo();
        QTest::qWait(50);
        const qreal zoomAfter = canvas->property("zoom").toReal();
        const qreal panAfter = canvas->property("panX").toReal();
        std::printf("  zoom after undo: %.2f (panX %.1f)\n", zoomAfter, panAfter);
        check(qAbs(zoomAfter - 3.0) < 0.001 && qAbs(panAfter - 11.0) < 0.001,
              "undo keeps zoom and pan");

        std::printf("scenario f12-f08-f07: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------------ F06 accessibility tree
    if (scenario == QLatin1String("a11y-tree")) {
        // What the AT-SPI bridge would publish (and Orca would read), queried
        // straight from Qt's accessibility layer.
        // One saved design, so the Recent rows (and their named buttons) exist.
        studio.newFromTemplate(0);
        studio.saveWithFeedback();
        studio.closeDoc();
        QTest::qWait(400);
        auto scan = [&](const char *where) {
            QHash<QString, int> roles;
            QStringList unnamed;
            QStringList names;
            QList<QQuickItem *> stack;
            stack << win->contentItem();
            while (!stack.isEmpty()) {
                QQuickItem *item = stack.takeLast();
                stack << item->childItems();
                if (!item->isVisible())
                    continue;
                QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item);
                if (!iface)
                    continue;
                const QAccessible::Role role = iface->role();
                if (role == QAccessible::NoRole)
                    continue;
                const QString name = iface->text(QAccessible::Name);
                roles[QString::number(int(role))]++;
                names << name;
                const bool needsName = role == QAccessible::Button || role == QAccessible::CheckBox
                    || role == QAccessible::ComboBox || role == QAccessible::PageTab;
                if (needsName && name.isEmpty())
                    unnamed << QStringLiteral("%1#%2").arg(int(role)).arg(item->objectName());
            }
            std::printf("  %s: %d accessible objects, roles=%lld, unnamed=%lld\n", where,
                        int(names.size()), (long long)roles.size(), (long long)unnamed.size());
            if (!unnamed.isEmpty())
                std::printf("    unnamed: %s\n", qPrintable(unnamed.join(QStringLiteral(", "))));
            check(unnamed.isEmpty(), "every control with a role has an accessible name");
            return names;
        };
        const QStringList dashboardNames = scan("dashboard");
        for (const QString &want : {QStringLiteral("New design"), QStringLiteral("Open Make it bold"),
                                    QStringLiteral("Delete Make it bold")}) {
            check(dashboardNames.contains(want),
                  qPrintable(QStringLiteral("dashboard exposes %1").arg(want)));
        }

        studio.newFromTemplate(0);
        QTest::qWait(500);
        studio.selectAll();
        QTest::qWait(200);
        const QStringList editorNames = scan("editor");
        for (const QString &want : {QStringLiteral("Undo"), QStringLiteral("Redo"),
                                    QStringLiteral("Export"), QStringLiteral("Back"),
                                    QStringLiteral("Select tool"), QStringLiteral("Zoom in"),
                                    QStringLiteral("Add page")}) {
            check(editorNames.contains(want),
                  qPrintable(QStringLiteral("editor exposes %1").arg(want)));
        }
        std::printf("scenario a11y-tree: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------- a11y tree shape (AT-SPI style)
    if (scenario == QLatin1String("a11y-shape")) {
        QTest::qWait(400);
        QAccessibleInterface *root = QAccessible::queryAccessibleInterface(win);
        if (!root) {
            std::printf("FAIL  no accessible interface for the window\n");
            return 1;
        }
        int total = 0;
        std::function<void(QAccessibleInterface *, int)> walk = [&](QAccessibleInterface *iface,
                                                                    int depth) {
            if (!iface || depth > 6 || total > 80)
                return;
            ++total;
            std::printf("%srole %-4d %s\n", QByteArray(depth * 2, ' ').constData(),
                        int(iface->role()), qPrintable(iface->text(QAccessible::Name)));
            for (int i = 0; i < iface->childCount(); ++i)
                walk(iface->child(i), depth + 1);
        };
        walk(root, 0);
        std::printf("total objects in the published tree: %d\n", total);
        return 0;
    }

    // ------------------------------------------------------------- F19 targets
    if (scenario == QLatin1String("f19-targets")) {
        auto audit = [&](const char *where) {
            QList<QQuickItem *> stack;
            stack << win->contentItem();
            int checked = 0;
            int walked = 0;
            while (!stack.isEmpty()) {
                QQuickItem *item = stack.takeLast();
                stack << item->childItems();
                ++walked;
                if (walked < 16)
                    std::printf("    [tree] %s %.0fx%.0f vis=%d kids=%lld\n",
                                qPrintable(cls0(item)), item->width(), item->height(),
                                item->isVisible() ? 1 : 0, (long long)item->childItems().size());
                if (!item->isVisible())
                    continue;
                const QMetaObject *mo = item->metaObject();
                const QString cls = QString::fromLatin1(mo->className());
                const bool isButton = mo->indexOfProperty("pressed") >= 0
                    && mo->indexOfSignal("clicked()") >= 0;
                const bool isClickArea = cls.contains(QLatin1String("MouseArea"))
                    && item->property("acceptedButtons").toInt() != 0;
                if (!isButton && !isClickArea)
                    continue;
                ++checked;
                if (item->width() < 24.0 || item->height() < 24.0) {
                    std::printf("  small target: %s %.0fx%.0f\n", qPrintable(cls), item->width(),
                                item->height());
                    ++g_failures;
                }
            }
            std::printf("  %s: %d interactive targets checked (walked %d items, screen=%s)\n", where,
                        checked, walked, qPrintable(eval(win, QStringLiteral(
                            "(() => { const l = win.findChild ? null : null; return studio.editing ? 'editor' : 'dashboard'; })()"))));
            check(checked > 0, "found interactive targets to measure");
        };
        QTest::qWait(300);
        audit("dashboard");
        studio.newFromTemplate(0);
        QTest::qWait(400);
        studio.selectAll();
        QTest::qWait(200);
        {
            const QList<QQuickItem *> all = win->contentItem()->findChildren<QQuickItem *>();
            for (QQuickItem *i : all) {
                if (QString::fromLatin1(i->metaObject()->className()).contains(QLatin1String("Loader")))
                    std::printf("  [loader] status=%d item=%p\n", i->property("status").toInt(),
                                (void *)i->property("item").value<QObject *>());
            }
        }
        audit("editor with a selection");
        std::printf("scenario f19-targets: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------------------------ F06 tab walk
    if (scenario == QLatin1String("f06-tab-walk")) {
        // One saved design so the Recent list (and its Open button) exists.
        studio.newFromTemplate(0);
        studio.saveWithFeedback();
        studio.closeDoc();
        QTest::qWait(300);
        QStringList seen;
        for (int i = 0; i < 30; ++i) {
            QTest::keyClick(win, Qt::Key_Tab);
            QTest::qWait(5);
            const QString n = eval(win, QStringLiteral(
                "(() => { const i = win.activeFocusItem; if (!i) return '<none>';"
                " const a = (i.Accessible && i.Accessible.name) ? i.Accessible.name : '';"
                " return (i.objectName || '(anon)') + '|' + a; })()"));
            seen << n;
        }
        std::printf("  focus order: %s\n", qPrintable(seen.join(QStringLiteral(" > "))));
        for (const QString &w : {QStringLiteral("New design"), QStringLiteral("Open"),
                                 QStringLiteral("Delete")}) {
            check(seen.filter(w).size() > 0,
                  qPrintable(QStringLiteral("Tab reaches \"%1\"").arg(w)));
        }
        std::printf("scenario f06-tab-walk: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ---------------------------------------------------------- persistent trash
    if (scenario == QLatin1String("trash-dashboard")) {
        studio.newDocument(320, 320);
        studio.setTitle(QStringLiteral("Recover me"));
        const QString id = studio.docId();
        check(studio.closeDoc(), "design returns to dashboard");
        studio.deleteDoc(id);
        QTest::qWait(150);
        QObject *restore = findItemByObjectName(win, QStringLiteral("restoreTrashButton"));
        check(restore && restore->property("visible").toBool(),
              "trashed design has a persistent Restore button");
        const QVariantList trash = studio.listTrash();
        check(trash.size() == 1, "trashed design appears in the dashboard model");
        if (!trash.isEmpty()) {
            check(studio.restoreDoc(trash.first().toMap().value(QStringLiteral("id")).toString()),
                  "Restore returns the design to Recent");
            QTest::qWait(100);
            check(findItemByObjectName(win, QStringLiteral("restoreTrashButton")) == nullptr,
                  "restored design leaves the Trash section");
            check(studio.listDocs().size() == 1, "restored design appears in Recent");
        }
        std::printf("scenario trash-dashboard: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------------------- mixed Inspector values
    if (scenario == QLatin1String("mixed-inspector")) {
        studio.newDocument(600, 600);
        const QString first = studio.addElement(QStringLiteral("rect"));
        studio.addElement(QStringLiteral("ellipse"));
        studio.selectElement(first, false);
        studio.setSelProp(QStringLiteral("opacity"), 0.25);
        studio.selectAll();
        QTest::qWait(200);

        QObject *num = findByObjectName(win, QStringLiteral("opacityNumField"));
        QObject *field = findByObjectName(num, QStringLiteral("numericInput"));
        check(num && field, "opacity field exists in the Inspector");
        if (!num || !field)
            return 1;
        check(num->property("mixed").toBool(), "mixed selection shows a mixed value");
        check(field->property("text").toString().isEmpty(), "mixed value does not show the primary value");

        int writes = 0;
        QObject::connect(&studio, &Studio::contentsChanged, &studio, [&writes]() { ++writes; });
        check(QMetaObject::invokeMethod(num, "commit"), "numeric field commits on focus loss");
        check(writes == 0 && field->property("text").toString().isEmpty(),
              "leaving an untouched mixed field changes nothing");
        field->setProperty("text", QStringLiteral("50"));
        check(QMetaObject::invokeMethod(num, "commit"), "mixed numeric entry commits");
        check(QMetaObject::invokeMethod(num, "commit"), "second focus event commits safely");
        check(writes == 1, "editingFinished plus focus loss produce one edit");
        for (const Element &element : studio.elements())
            check(qAbs(element.opacity - 0.5) < 0.001, "mixed edit sets each element's opacity");

        std::printf("scenario mixed-inspector: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ------------------------------------------------------- portal result routing
    if (scenario == QLatin1String("portal-routing")) {
        studio.newDocument(192, 192);
        QTest::qWait(200);
        QQuickItem *editor = findItemByObjectName(win, QStringLiteral("canvasStage"));
        while (editor && !editor->property("imagePickerPending").isValid())
            editor = editor->parentItem();
        check(editor != nullptr, "editor receives image picker results");
        if (!editor)
            return 1;

        QTemporaryDir files;
        check(files.isValid(), "temporary image folder exists");
        if (!files.isValid())
            return 1;
        const QString source = files.path() + QStringLiteral("/source.png");
        QImage image(7, 9, QImage::Format_ARGB32);
        image.fill(Qt::red);
        check(image.save(source), "source image was written");

        editor->setProperty("imagePickerPending", true);
        editor->setProperty("imageMode", QStringLiteral("place"));
        editor->setProperty("imageDocId", studio.docId());
        editor->setProperty("imagePageId", studio.currentPageId());
        portal.openFinished(QUrl::fromLocalFile(source), false);
        QTest::qWait(200);

        check(studio.elements().size() == 1, "image chooser adds exactly one image layer");
        check(QImage(source).size() == QSize(7, 9), "image chooser leaves source pixels intact");
        check(!studio.exporting(), "image chooser does not start an export");

        QObject *exportDialog = findByObjectName(win, QStringLiteral("exportDialog"));
        check(exportDialog != nullptr, "export dialog receives save picker results");
        if (!exportDialog)
            return 1;
        check(QMetaObject::invokeMethod(exportDialog, "open"), "export dialog opens");
        QTest::qWait(100);
        exportDialog->setProperty("exportDocId", studio.docId());
        exportDialog->setProperty("exportPageId", studio.currentPageId());
        exportDialog->setProperty("portalPending", true);
        const QString destination = files.path() + QStringLiteral("/export.png");
        portal.saveFinished(QUrl::fromLocalFile(destination), false);
        for (int i = 0; i < 50 && studio.exporting(); ++i)
            QTest::qWait(20);
        check(QFileInfo::exists(destination) && !QImage(destination).isNull(),
              "save chooser writes the requested export");
        check(studio.elements().size() == 1, "save chooser does not import another image");
        check(QImage(source).size() == QSize(7, 9), "export leaves the imported source intact");
        std::printf("scenario portal-routing: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // -------------------------------------------------------- F17 small window
    if (scenario == QLatin1String("f17-small-window")) {
        win->resize(800, 600);
        studio.newDocument(1080, 1080);
        QTest::qWait(400);
        QObject *canvas = findByObjectName(win, QStringLiteral("canvasStage"));
        QObject *inspectorPanel = findByObjectName(win, QStringLiteral("inspectorPanel"));
        check(canvas != nullptr, "canvas objectName found");
        check(inspectorPanel && !inspectorPanel->property("visible").toBool(),
              "compact inspector starts closed so the page stays visible");
        if (canvas) {
            const qreal w = canvas->property("width").toReal();
            std::printf("  canvas width at 800x600: %.0f\n", w);
            check(w >= 400.0, "canvas width >= 400 px at 800x600");
            auto *item = qobject_cast<QQuickItem *>(canvas);
            const qreal zoom = canvas->property("zoom").toReal();
            const QRectF page(item->mapToScene(QPointF(canvas->property("panX").toReal(),
                                                     canvas->property("panY").toReal())),
                              QSizeF(studio.docWidth() * zoom, studio.docHeight() * zoom));
            const QRectF viewport(item->mapToScene(QPointF()), item->size());
            check(viewport.contains(page), "fitted page lies inside the visible canvas");
        }
        const QImage grab = win->grabWindow();
        check(!grab.isNull(), "800x600 grab produced");
        check(win->width() == 800 && win->height() == 600, "window accepted 800x600");
        grab.save(QStringLiteral("/tmp/levele-harness-800x600.png"));
        std::printf("  grab: %dx%d -> /tmp/levele-harness-800x600.png\n", grab.width(), grab.height());
        win->resize(1200, 600);
        QTest::qWait(80);
        check(inspectorPanel && inspectorPanel->property("visible").toBool(),
              "wide layout restores the Inspector beside the canvas");
        win->resize(800, 600);
        QTest::qWait(80);
        check(inspectorPanel && !inspectorPanel->property("visible").toBool(),
              "returning to compact layout closes the overlay");
        QQuickItem *editor = findItemByObjectName(win, QStringLiteral("canvasStage"));
        while (editor && !editor->property("rightOpen").isValid())
            editor = editor->parentItem();
        check(editor && QMetaObject::invokeMethod(editor, "toggleRight")
                  && inspectorPanel->property("visible").toBool(),
              "Inspector toggle opens the compact overlay on demand");
        std::printf("scenario f17-small-window: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    // ----------------------------------------- Phase 2 C++ & QML regressions
    if (scenario == QLatin1String("phase2-regressions")) {
        studio.newDocument(800, 800);
        QTest::qWait(300);
        auto *canvas = findItemByObjectName(win, QStringLiteral("canvasStage"));
        check(canvas != nullptr, "canvas item found");
        if (!canvas)
            return 1;

        const QPointF origin = canvas->mapToScene(QPointF(0, 0));
        const qreal zoom = canvas->property("zoom").toReal();
        const qreal panX = canvas->property("panX").toReal();
        const qreal panY = canvas->property("panY").toReal();
        auto toWindow = [&](const QPointF &doc) {
            return QPointF(origin.x() + panX + doc.x() * zoom, origin.y() + panY + doc.y() * zoom);
        };

        // 1. Line Stroke section visible in Inspector + handle resize keeps zero/signed vector.
        const QString lineId = studio.addElement(QStringLiteral("line"));
        studio.setSelProp(QStringLiteral("x"), 100.0);
        studio.setSelProp(QStringLiteral("y"), 300.0);
        studio.setSelProp(QStringLiteral("w"), 300.0);
        studio.setSelProp(QStringLiteral("h"), 0.0);
        QTest::qWait(50);

        QStringList sections;
        {
            QList<QQuickItem *> stack;
            stack << win->contentItem();
            while (!stack.isEmpty()) {
                QQuickItem *item = stack.takeLast();
                stack << item->childItems();
                if (item->metaObject()->indexOfProperty("title") >= 0
                    && item->metaObject()->indexOfProperty("expanded") >= 0) {
                    sections << QStringLiteral("%1=%2").arg(
                        item->property("title").toString(),
                        item->property("visible").toBool() ? "shown" : "hidden");
                }
            }
        }
        check(sections.contains(QStringLiteral("Fill=hidden")), "line hides Fill section");
        check(sections.contains(QStringLiteral("Stroke=shown")), "line shows Stroke section");

        int selSignals = 0;
        QObject::connect(&studio, &Studio::selectionChanged, &studio, [&selSignals]() { ++selSignals; });
        sendMouse(win, QEvent::MouseButtonPress, toWindow(QPointF(400, 300)), Qt::LeftButton, Qt::LeftButton);
        sendMouse(win, QEvent::MouseMove, toWindow(QPointF(450, 300)), Qt::NoButton, Qt::LeftButton);
        sendMouse(win, QEvent::MouseButtonRelease, toWindow(QPointF(450, 300)), Qt::LeftButton, Qt::NoButton);
        QTest::qWait(30);
        const Element *resized = studio.elementById(lineId);
        check(resized && qAbs(resized->w - 350.0) < 2.0 && qAbs(resized->h) < 0.01,
              "resizing flat line preserves h=0 and expands w to 350");
        check(selSignals >= 1 && qAbs(studio.sel().value(QStringLiteral("w")).toDouble() - resized->w) < 0.5,
              "selectionChanged fires on resize release so Inspector syncs");

        // Negative-vector line resize preserves negative w.
        const QString negLine = studio.addElement(QStringLiteral("line"));
        studio.setSelProp(QStringLiteral("x"), 400.0);
        studio.setSelProp(QStringLiteral("y"), 450.0);
        studio.setSelProp(QStringLiteral("w"), -100.0);
        studio.setSelProp(QStringLiteral("h"), 0.0);
        QTest::qWait(30);
        sendMouse(win, QEvent::MouseButtonPress, toWindow(QPointF(300, 450)), Qt::LeftButton, Qt::LeftButton);
        sendMouse(win, QEvent::MouseMove, toWindow(QPointF(260, 450)), Qt::NoButton, Qt::LeftButton);
        sendMouse(win, QEvent::MouseButtonRelease, toWindow(QPointF(260, 450)), Qt::LeftButton, Qt::NoButton);
        QTest::qWait(30);
        const Element *negResized = studio.elementById(negLine);
        check(negResized && negResized->w < -120.0 && qAbs(negResized->h) < 0.01,
              "resizing negative-vector line preserves negative w and h=0");

        // 2. Return/Enter opens inline text editor; page switch clears editingId; locked blocks edit.
        const QString textId = studio.addElementAt(QStringLiteral("text"), QPointF(120, 120));
        QTest::qWait(50);
        QMetaObject::invokeMethod(canvas, "forceActiveFocus");
        QTest::qWait(20);
        QTest::keyClick(win, Qt::Key_Return);
        QTest::qWait(100);
        check(canvas->property("editingId").toString() == textId,
              "Return starts inline text edit on selected text element");
        studio.addPage();
        QTest::qWait(50);
        check(canvas->property("editingId").toString().isEmpty(),
              "adding/switching page clears active inline editingId");

        studio.setCurrentPage(0);
        QTest::qWait(50);
        studio.setElementLocked(textId, true);
        studio.selectElement(textId, false);
        QMetaObject::invokeMethod(canvas, "forceActiveFocus");
        QTest::keyClick(win, Qt::Key_Return);
        QTest::qWait(50);
        check(canvas->property("editingId").toString().isEmpty(),
              "Return does not open inline editor on a locked text element");
        check(studio.sel().value(QStringLiteral("locked")).toBool(),
              "sel exposes locked=true when locked layer is selected");

        // 3. Modal Escape closes overlay without clearing canvas selection.
        const QString rectId = studio.addElementAt(QStringLiteral("rect"), QPointF(200, 200));
        studio.selectElement(rectId, false);
        QMetaObject::invokeMethod(canvas, "forceActiveFocus");
        QTest::qWait(30);
        QTest::keyClick(win, Qt::Key_Question);
        QTest::qWait(80);
        QObject *shortcuts = nullptr;
        for (QObject *obj : win->findChildren<QObject *>()) {
            if (obj->property("shortcuts").isValid()) {
                shortcuts = obj;
                break;
            }
        }
        check(shortcuts && shortcuts->property("visible").toBool(),
              "Question shortcut opens ShortcutsOverlay");
        QTest::keyClick(win, Qt::Key_Escape);
        QTest::qWait(80);
        check(shortcuts && !shortcuts->property("visible").toBool(),
              "Escape closes ShortcutsOverlay");
        check(studio.selectionCount() == 1 && studio.isSelected(rectId),
              "Escape closing modal does not clear canvas selection");

        // 4. Compact 640x480 topBar spacing and LayersPanel top alignment.
        win->resize(640, 480);
        QTest::qWait(150);
        auto *titleField = findItemByObjectName(win, QStringLiteral("titleField"));
        auto *undoBtn = findItemByObjectName(win, QStringLiteral("undoButton"));
        auto *layers = findItemByObjectName(win, QStringLiteral("layerList"));
        if (titleField && undoBtn) {
            const qreal titleRight = titleField->mapToScene(QPointF(titleField->width(), 0)).x();
            const qreal undoLeft = undoBtn->mapToScene(QPointF(0, 0)).x();
            std::printf("  640x480 topBar: titleRight=%.1f undoLeft=%.1f\n", titleRight, undoLeft);
            check(titleRight <= undoLeft + 0.5, "titleField does not overlap undoButton at 640x480");
        } else {
            check(false, "titleField and undoButton found at 640x480");
        }
        if (layers) {
            std::printf("  layers list y=%.1f height=%.1f\n", layers->y(), layers->height());
            check(layers->y() <= 48.0, "layerList stays anchored near top of LayersPanel");
        } else {
            check(false, "layerList found");
        }

        std::printf("scenario phase2-regressions: %d failing\n", g_failures);
        return g_failures ? 1 : 0;
    }

    std::printf("FAIL  unknown scenario '%s'\n", qPrintable(scenario));
    return 2;
}
