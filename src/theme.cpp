#include "theme.h"

#include <utility>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>
#include <unistd.h>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTextStream>
#include <cmath>

namespace {

double linearise(double v)
{
    v /= 255.0;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

double luminance(const QColor &c)
{
    return 0.2126 * linearise(c.red()) + 0.7152 * linearise(c.green()) + 0.0722 * linearise(c.blue());
}

bool looksLikeHex(const QString &s)
{
    if (s.size() != 7 || s.at(0) != QLatin1Char('#'))
        return false;
    for (int i = 1; i < 7; ++i) {
        const QChar ch = s.at(i);
        if (!ch.isDigit() && !(ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))
            && !(ch >= QLatin1Char('A') && ch <= QLatin1Char('F')))
            return false;
    }
    return true;
}

// colors.toml is `key = "value"` lines with `#` comments. Values are quoted hex,
// so a naive "strip everything after #" would destroy every colour.
QHash<QString, QString> parseColors(const QString &path)
{
    QHash<QString, QString> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')) && value.size() >= 2)
            value = value.mid(1, value.size() - 2);
        if (!key.isEmpty() && !value.isEmpty())
            out.insert(key, value);
    }
    return out;
}

QString pick(const QHash<QString, QString> &map, const QStringList &keys)
{
    for (const QString &k : keys) {
        const auto it = map.constFind(k);
        if (it != map.constEnd() && looksLikeHex(it.value()))
            return it.value();
    }
    return QString();
}

QColor colour(const QHash<QString, QString> &map, const QStringList &keys, const QColor &fallback)
{
    const QString s = pick(map, keys);
    if (s.isEmpty())
        return fallback;
    const QColor c = QColor::fromString(s);
    return c.isValid() ? c : fallback;
}

// Keep ink readable on every panel it is painted on, not just two of them.
bool readableOn(const QColor &ink, const QVector<QColor> &panels, double ratio)
{
    for (const QColor &panel : panels) {
        if (Theme::contrast(ink, panel) < ratio)
            return false;
    }
    return true;
}

QColor readable(QColor ink, const QVector<QColor> &panels, double ratio)
{
    if (readableOn(ink, panels, ratio))
        return ink;
    double panelLum = 0;
    for (const QColor &panel : panels)
        panelLum += luminance(panel);
    panelLum /= double(qMax(1, panels.size()));
    const QColor target = panelLum < 0.35 ? QColor(Qt::white) : QColor(Qt::black);
    for (int i = 1; i <= 200; ++i) {
        const QColor c = Theme::mix(ink, target, i / 200.0);
        if (readableOn(c, panels, ratio))
            return c;
    }
    // Panels of very different lightness cannot all be satisfied: take the
    // black/white extreme, which is the best a single ink can do.
    return target;
}

QColor readable(QColor ink, const QColor &panelA, const QColor &panelB, double ratio)
{
    return readable(ink, QVector<QColor>{panelA, panelB}, ratio);
}

QString hyprlandRuntimeDir()
{
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty())
        return runtime;
    return QStringLiteral("/run/user/%1").arg(int(::getuid()));
}

} // namespace

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, [this]() {
        rewireWatcher();
        reload();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Theme::schedule);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &Theme::schedule);

    rewireWatcher();
    reload();
    watchHyprlandEvents();
}

Theme::~Theme()
{
    if (m_radiusQuery && m_radiusQuery->state() != QProcess::NotRunning) {
        m_radiusQuery->disconnect(this);
        m_radiusQuery->kill();
        m_radiusQuery->waitForFinished(200);
    }
}

QString Theme::currentRoot()
{
    QStringList candidates;
    const QString stateHome = qEnvironmentVariable("XDG_STATE_HOME");
    if (!stateHome.isEmpty())
        candidates << stateHome + QStringLiteral("/omarchy/current");
    candidates << QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
    const QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!configHome.isEmpty())
        candidates << configHome + QStringLiteral("/omarchy/current");
    candidates << QDir::homePath() + QStringLiteral("/.config/omarchy/current");

    for (const QString &c : std::as_const(candidates)) {
        const QFileInfo fi(c);
        if (fi.exists() && fi.isDir())
            return c;
    }
    return QString();
}

QColor Theme::mix(const QColor &a, const QColor &b, double t)
{
    t = qBound(0.0, t, 1.0);
    auto chan = [t](int x, int y) {
        return int(qBound(0, qRound(double(x) + (double(y) - double(x)) * t), 255));
    };
    QColor out;
    out.setRgb(chan(a.red(), b.red()), chan(a.green(), b.green()), chan(a.blue(), b.blue()), a.alpha());
    return out;
}

double Theme::contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    const double hi = qMax(la, lb);
    const double lo = qMin(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

QColor Theme::ensureContrast(QColor ink, const QColor &bg, double ratio)
{
    if (contrast(ink, bg) >= ratio)
        return ink;
    return readable(ink, bg, bg, ratio);
}

QColor Theme::bestInkOn(const QColor &paper)
{
    const QColor black(Qt::black);
    const QColor white(Qt::white);
    return contrast(black, paper) >= contrast(white, paper) ? black : white;
}

QVector<Theme::ContrastPair> Theme::contrastPairs()
{
    // Every ink/paper combination the QML paints. Mirrored by the contrast
    // script in the handoff appendix; keep the two in step.
    return {
        {QStringLiteral("fg/bg"), QStringLiteral("fg"), QStringLiteral("bg"), 4.5,
         QStringLiteral("NumField/Combo/TextArea/tool tiles/shortcut keys")},
        {QStringLiteral("fg/surface"), QStringLiteral("fg"), QStringLiteral("surface"), 4.5,
         QStringLiteral("panels, rows, chips")},
        {QStringLiteral("fg/chrome"), QStringLiteral("fg"), QStringLiteral("chrome"), 4.5,
         QStringLiteral("window, top bar")},
        {QStringLiteral("fg/surfaceAlt"), QStringLiteral("fg"), QStringLiteral("surfaceAlt"), 4.5,
         QStringLiteral("hovered rows, chips")},
        {QStringLiteral("muted/surfaceAlt"), QStringLiteral("muted"), QStringLiteral("surfaceAlt"), 4.5,
         QStringLiteral("recent row meta on hover")},
        {QStringLiteral("fg/accentSoft"), QStringLiteral("fg"), QStringLiteral("accentSoft"), 4.5,
         QStringLiteral("selected layer name")},
        {QStringLiteral("accentInk/accent"), QStringLiteral("accentInk"), QStringLiteral("accent"), 4.5,
         QStringLiteral("accent Btn, active tool, align, combo highlight")},
        {QStringLiteral("accent/surface"), QStringLiteral("accent"), QStringLiteral("surface"), 3.0,
         QStringLiteral("focus ring, selected layer glyph, lock icon (1.4.11)")},
        {QStringLiteral("accent/chrome"), QStringLiteral("accent"), QStringLiteral("chrome"), 3.0,
         QStringLiteral("canvas selection outline + handles")},
        {QStringLiteral("border/surface"), QStringLiteral("border"), QStringLiteral("surface"), 3.0,
         QStringLiteral("NumField/Combo/ColorChip boundary (1.4.11)")},
        {QStringLiteral("border/bg"), QStringLiteral("border"), QStringLiteral("bg"), 3.0,
         QStringLiteral("input boundary on bg-filled fields")},
        {QStringLiteral("red/surfaceAlt"), QStringLiteral("red"), QStringLiteral("surfaceAlt"), 4.5,
         QStringLiteral("danger Btn hovered")},
    };
}

QColor Theme::token(const QString &name) const
{
    if (name == QLatin1String("bg")) return m_bg;
    if (name == QLatin1String("fg")) return m_fg;
    if (name == QLatin1String("textStrong")) return m_textStrong;
    if (name == QLatin1String("surface")) return m_surface;
    if (name == QLatin1String("surfaceAlt")) return m_surfaceAlt;
    if (name == QLatin1String("chrome")) return m_chrome;
    if (name == QLatin1String("border")) return m_border;
    if (name == QLatin1String("accent")) return m_accent;
    if (name == QLatin1String("accentInk")) return m_accentInk;
    if (name == QLatin1String("accentSoft")) return m_accentSoft;
    if (name == QLatin1String("muted")) return m_muted;
    if (name == QLatin1String("red")) return m_red;
    if (name == QLatin1String("green")) return m_green;
    if (name == QLatin1String("yellow")) return m_yellow;
    return QColor();
}

void Theme::schedule()
{
    m_debounce.start();
}

void Theme::rewireWatcher()
{
    const QStringList dirs = m_watcher.directories();
    const QStringList files = m_watcher.files();
    if (!dirs.isEmpty())
        m_watcher.removePaths(dirs);
    if (!files.isEmpty())
        m_watcher.removePaths(files);

    const QString root = currentRoot();
    if (!root.isEmpty()) {
        m_watcher.addPath(root);
        const QStringList watched = {root + QStringLiteral("/theme.name"),
                                     root + QStringLiteral("/theme/colors.toml"),
                                     root + QStringLiteral("/colors.toml")};
        for (const QString &f : watched) {
            if (QFileInfo::exists(f) && !m_watcher.files().contains(f))
                m_watcher.addPath(f);
        }
        // Omarchy may not have written `current` yet on a very first run.
        const QString parentDir = QFileInfo(root).absolutePath();
        if (QFileInfo::exists(parentDir))
            m_watcher.addPath(parentDir);
    } else {
        QStringList watchDirs;
        const QString stateHome = qEnvironmentVariable("XDG_STATE_HOME");
        if (!stateHome.isEmpty())
            watchDirs << stateHome + QStringLiteral("/omarchy");
        watchDirs << QDir::homePath() + QStringLiteral("/.local/state/omarchy");
        const QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
        if (!configHome.isEmpty())
            watchDirs << configHome + QStringLiteral("/omarchy");
        watchDirs << QDir::homePath() + QStringLiteral("/.config/omarchy");
        for (const QString &d : std::as_const(watchDirs)) {
            if (QFileInfo::exists(d) && !m_watcher.directories().contains(d))
                m_watcher.addPath(d);
        }
    }
}

void Theme::reload()
{
    const QString root = currentRoot();
    loadFrom(root);
    emit changed();
    queryRadius();
}

// hyprctl used to block the GUI thread for up to 300 ms at startup and on every
// theme switch. It runs asynchronously now and only the radius changes.
void Theme::queryRadius()
{
    if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
        if (m_radius != 0) {
            m_radius = 0;
            emit changed();
        }
        return;
    }
    if (m_radiusQuery)
        return; // one in flight is enough

    auto *proc = new QProcess(this);
    m_radiusQuery = proc;
    connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus status) {
        int radius = 0;
        if (status == QProcess::NormalExit && code == 0) {
            const QJsonObject obj = QJsonDocument::fromJson(proc->readAllStandardOutput()).object();
            radius = qBound(0, obj.value(QStringLiteral("int")).toInt(0), 24);
        }
        if (m_radiusQuery == proc)
            m_radiusQuery = nullptr;
        proc->deleteLater();
        if (radius != m_radius) {
            m_radius = radius;
            emit changed();
        }
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
        // hyprctl missing or Hyprland gone: radius 0 is a fine default.
        if (m_radiusQuery == proc)
            m_radiusQuery = nullptr;
        proc->deleteLater();
        if (m_radius != 0) {
            m_radius = 0;
            emit changed();
        }
    });
    proc->start(QStringLiteral("hyprctl"),
                {QStringLiteral("-j"), QStringLiteral("getoption"), QStringLiteral("decoration:rounding")});
}

// Hyprland's socket2 emits one line per event; `configreloaded` means the
// rounding option may have changed under us.
void Theme::watchHyprlandEvents()
{
    const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (signature.isEmpty())
        return;
    const QString path = hyprlandRuntimeDir() + QStringLiteral("/hypr/") + signature
        + QStringLiteral("/.socket2.sock");
    m_eventSocketName = path;

    if (!m_eventSocket) {
        m_eventSocket = new QLocalSocket(this);
        connect(m_eventSocket, &QLocalSocket::readyRead, this, [this]() {
            constexpr int kMaxEventBuffer = 65536;
            while (m_eventSocket->bytesAvailable() > 0) {
                const QByteArray chunk = m_eventSocket->read(kMaxEventBuffer);
                if (chunk.isEmpty())
                    break;
                if (m_eventBuffer.size() + chunk.size() > kMaxEventBuffer)
                    m_eventBuffer.clear();
                m_eventBuffer += chunk;
                int nl = m_eventBuffer.indexOf('\n');
                while (nl >= 0) {
                    const QByteArray line = m_eventBuffer.left(nl);
                    m_eventBuffer.remove(0, nl + 1);
                    if (line.startsWith("configreloaded"))
                        queryRadius();
                    nl = m_eventBuffer.indexOf('\n');
                }
            }
        });
        connect(m_eventSocket, &QLocalSocket::disconnected, this, [this]() {
            m_eventBuffer.clear();
            // Hyprland restarted: try again in a moment rather than spin.
            QTimer::singleShot(2000, this, [this]() {
                if (m_eventSocket && !m_eventSocketName.isEmpty()
                    && m_eventSocket->state() == QLocalSocket::UnconnectedState)
                    m_eventSocket->connectToServer(m_eventSocketName);
            });
        });
    }
    if (m_eventSocket->state() == QLocalSocket::UnconnectedState)
        m_eventSocket->connectToServer(path);
}

void Theme::loadFromRoot(const QString &root)
{
    loadFrom(root);
}

void Theme::loadFrom(const QString &root)
{
    QHash<QString, QString> map;
    QString name = QStringLiteral("omarchy");
    if (!root.isEmpty()) {
        QString themeDir = root + QStringLiteral("/theme");
        if (!QFileInfo::exists(themeDir + QStringLiteral("/colors.toml")))
            themeDir = root;
        map = parseColors(themeDir + QStringLiteral("/colors.toml"));

        QFile nf(root + QStringLiteral("/theme.name"));
        if (nf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString first = QString::fromUtf8(nf.readLine()).trimmed();
            if (!first.isEmpty())
                name = first;
        }
    }
    m_themeName = name;

    const QString mode = map.value(QStringLiteral("mode"));
    const bool dark = mode.compare(QLatin1String("light"), Qt::CaseInsensitive) != 0;
    m_dark = dark;

    const QColor bg = colour(map, {"background", "bg"}, dark ? QColor(0x0c, 0x0d, 0x0b) : QColor(0xf7, 0xf5, 0xf0));
    const QColor fg = colour(map, {"foreground", "fg", "light_foreground"},
                             dark ? QColor(0xe8, 0xe4, 0xdc) : QColor(0x1d, 0x1c, 0x1a));

    // surface / chrome are the two panels every piece of ink lands on.
    QColor surface = colour(map, {"lighter_background", "lighter_bg"}, QColor());
    if (!surface.isValid())
        surface = mix(bg, fg, dark ? 0.09 : 0.06);
    QColor surfaceAlt = mix(surface, fg, dark ? 0.07 : 0.05);

    QColor chrome = colour(map, {"dark_background", "dark_bg", "darker_bg"}, QColor());
    if (!chrome.isValid())
        chrome = dark ? mix(bg, QColor(Qt::black), 0.18) : mix(bg, fg, 0.05);

    QColor border = colour(map, {"line", "border"}, QColor());
    if (!border.isValid())
        border = mix(bg, fg, dark ? 0.22 : 0.28);

    const QColor accent = colour(map, {"accent", "blue", "magenta"},
                                 colour(map, {"bright_blue"}, fg));

    m_bg = bg;
    m_surface = surface;
    m_surfaceAlt = surfaceAlt;
    m_chrome = chrome;

    // The accent doubles as a stroke on the panels and as a fill behind
    // accentInk, so it has to clear 3:1 on both panels before the ink is picked.
    m_accent = readable(accent, QVector<QColor>{surface, chrome}, 3.0);
    m_accentSoft = mix(surface, m_accent, dark ? 0.24 : 0.16);
    // Black or white, whichever reads better: the better of the two is always
    // at least 4.58:1, so accent buttons can never fall below 4.5:1.
    m_accentInk = bestInkOn(m_accent);

    // Inks are derived against every panel they can land on. The selected-layer
    // tint is tuned after the foreground so the text is never the thing that
    // gives way.
    const QVector<QColor> panels{surface, chrome, surfaceAlt, bg};
    m_border = readable(border, QVector<QColor>{surface, chrome, bg}, 3.0);
    m_fg = readable(fg, panels, 4.5);
    for (double tint = dark ? 0.24 : 0.16; tint > 0.0 && contrast(m_fg, m_accentSoft) < 4.5;
         tint -= 0.02) {
        m_accentSoft = mix(surface, m_accent, tint);
    }
    m_textStrong = readable(colour(map, {"bright_foreground"}, fg), panels, 4.5);
    m_muted = readable(mix(fg, bg, 0.42), panels, 4.5);
    m_red = readable(colour(map, {"red"}, QColor(0xff, 0x64, 0x67)), panels, 4.5);
    m_green = readable(colour(map, {"green"}, QColor(0x8e, 0xe0, 0x7a)), panels, 4.5);
    m_yellow = readable(colour(map, {"yellow"}, QColor(0xf3, 0xca, 0x3e)), panels, 4.5);

    if (QGuiApplication::instance()) {
        if (auto *hints = QGuiApplication::styleHints())
            hints->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    }
}
