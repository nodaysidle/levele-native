#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QDBusMessage;

// Thin wrapper over org.freedesktop.portal.FileChooser, which is what the
// Omarchy app convention asks for instead of the in-process Qt dialogs (those
// turn into GTK dialogs through QT_QPA_PLATFORMTHEME=gtk3 and ignore the
// desktop's own look). `available` is false on a session without a portal, and
// callers fall back to QtQuick.Dialogs.
class PortalFileChooser : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit PortalFileChooser(QObject *parent = nullptr);

    bool available() const;
    bool busy() const { return !m_handlePath.isEmpty(); }

    Q_INVOKABLE bool saveFile(const QString &title, const QString &suggestedName);
    Q_INVOKABLE bool openFile(const QString &title);

signals:
    void busyChanged();
    // An empty file with useFallback false means the user canceled.
    void openFinished(const QUrl &file, bool useFallback);
    void saveFinished(const QUrl &file, bool useFallback);

private slots:
    void onResponse(uint response, const QVariantMap &results, const QDBusMessage &message);

private:
    bool request(bool saving, const QString &title, const QString &suggestedName);
    bool subscribe();
    void unsubscribe();
    void finish(const QUrl &file, bool useFallback);

    bool m_saving = false;
    QString m_handlePath;
};
