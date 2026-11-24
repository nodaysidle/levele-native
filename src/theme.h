#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

// Reads the live Omarchy theme (colors.toml) and re-reads it whenever the
// desktop theme changes. Omarchy replaces current/theme wholesale on a switch,
// so file watches alone go stale: the directory is watched too.
class QProcess;

class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor bg MEMBER m_bg NOTIFY changed)
    Q_PROPERTY(QColor fg MEMBER m_fg NOTIFY changed)
    Q_PROPERTY(QColor textStrong MEMBER m_textStrong NOTIFY changed)
    Q_PROPERTY(QColor surface MEMBER m_surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceAlt MEMBER m_surfaceAlt NOTIFY changed)
    Q_PROPERTY(QColor chrome MEMBER m_chrome NOTIFY changed)
    Q_PROPERTY(QColor border MEMBER m_border NOTIFY changed)
    Q_PROPERTY(QColor accent MEMBER m_accent NOTIFY changed)
    Q_PROPERTY(QColor accentInk MEMBER m_accentInk NOTIFY changed)
    Q_PROPERTY(QColor accentSoft MEMBER m_accentSoft NOTIFY changed)
    Q_PROPERTY(QColor muted MEMBER m_muted NOTIFY changed)
    Q_PROPERTY(QColor red MEMBER m_red NOTIFY changed)
    Q_PROPERTY(QColor green MEMBER m_green NOTIFY changed)
    Q_PROPERTY(QColor yellow MEMBER m_yellow NOTIFY changed)
    Q_PROPERTY(bool dark MEMBER m_dark NOTIFY changed)
    Q_PROPERTY(int radius MEMBER m_radius NOTIFY changed)
    Q_PROPERTY(QString themeName READ themeName NOTIFY changed)

public:
    // One pair of tokens the QML actually paints together. The theme sweep and
    // the derivation in loadFrom() both read this table, so a pair can never be
    // enforced in one place and forgotten in the other.
    struct ContrastPair {
        QString label;   // "fg/surfaceAlt"
        QString ink;     // token name, as used by token()
        QString paper;
        double ratio = 4.5; // WCAG minimum
        QString where;   // what paints it
    };
    static QVector<ContrastPair> contrastPairs();

    explicit Theme(QObject *parent = nullptr);
    ~Theme() override;

    QString themeName() const { return m_themeName; }

    // Token by Q_PROPERTY name, for the sweep and tests.
    QColor token(const QString &name) const;
    // Whichever of black/white reads better on `paper`. The better of the two
    // is always >= 4.58:1, so accent text can never fall below 4.5:1.
    static QColor bestInkOn(const QColor &paper);

    // Directory holding the live theme (…/omarchy/current), empty if not found.
    static QString currentRoot();

    static QColor mix(const QColor &a, const QColor &b, double t);
    static double contrast(const QColor &a, const QColor &b);
    // Walk ink towards black or white until it clears `ratio` against bg.
    static QColor ensureContrast(QColor ink, const QColor &bg, double ratio);

    void reload();

    // Load tokens from an explicit theme directory (used by the theme sweep
    // and by tests). Accepts both the flat layout and a `theme/` subdir.
    void loadFromRoot(const QString &root);

signals:
    void changed();

private slots:
    void schedule();

private:
    void rewireWatcher();
    void loadFrom(const QString &rootDir);
    // hyprctl runs off the GUI thread: the answer only sets the corner radius.
    void queryRadius();
    // Hyprland's event socket tells us when its config was reloaded.
    void watchHyprlandEvents();

    QPointer<QProcess> m_radiusQuery;
    class QLocalSocket *m_eventSocket = nullptr;
    QByteArray m_eventBuffer;
    QString m_eventSocketName;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_themeName = QStringLiteral("omarchy");

    QColor m_bg{0x0c, 0x0d, 0x0b};
    QColor m_fg{0xe8, 0xe4, 0xdc};
    QColor m_textStrong{0xff, 0xff, 0xff};
    QColor m_surface{0x18, 0x19, 0x17};
    QColor m_surfaceAlt{0x1f, 0x20, 0x1e};
    QColor m_chrome{0x09, 0x0a, 0x08};
    QColor m_border{0x2b, 0x2a, 0x28};
    QColor m_accent{0xf5, 0x45, 0xc9};
    QColor m_accentInk{0x00, 0x00, 0x00};
    QColor m_accentSoft{0x3d, 0x14, 0x32};
    QColor m_muted{0x9a, 0x97, 0x91};
    QColor m_red{0xff, 0x64, 0x67};
    QColor m_green{0x8e, 0xe0, 0x7a};
    QColor m_yellow{0xf3, 0xca, 0x3e};
    bool m_dark = true;
    int m_radius = 0;
};
