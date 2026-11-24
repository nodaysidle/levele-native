#include "portalfilechooser.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QUuid>

namespace {

const char *kService = "org.freedesktop.portal.Desktop";
const char *kPath = "/org/freedesktop/portal/desktop";
const char *kInterface = "org.freedesktop.portal.FileChooser";
const char *kRequestInterface = "org.freedesktop.portal.Request";

// The portal exports a Request object at a path built from our unique bus name
// and the handle token, so the Response signal can be hooked up *before* the
// call — otherwise a fast answer arrives before we are listening.
QString requestPathFor(const QString &token)
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    QString sender = bus.baseService();
    if (sender.startsWith(QLatin1Char(':')))
        sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

} // namespace

PortalFileChooser::PortalFileChooser(QObject *parent)
    : QObject(parent)
{
    auto *watcher = new QDBusServiceWatcher(QString::fromLatin1(kService),
        QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        if (busy())
            finish({}, true);
    });
}

bool PortalFileChooser::available() const
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;
    QDBusConnectionInterface *iface = bus.interface();
    return iface && iface->isServiceRegistered(QString::fromLatin1(kService));
}

bool PortalFileChooser::saveFile(const QString &title, const QString &suggestedName)
{
    return request(true, title, suggestedName);
}

bool PortalFileChooser::openFile(const QString &title)
{
    return request(false, title, {});
}

bool PortalFileChooser::request(bool saving, const QString &title,
                                const QString &suggestedName)
{
    if (busy() || !available())
        return false;

    const QString token = QStringLiteral("levele") + QUuid::createUuid().toString(QUuid::Id128);
    const QString requestPath = requestPathFor(token);
    m_handlePath = requestPath;
    if (!subscribe()) {
        m_handlePath.clear();
        return false;
    }
    m_saving = saving;
    emit busyChanged();

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("modal"), true);
    if (!suggestedName.isEmpty())
        options.insert(QStringLiteral("current_name"), suggestedName);

    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(kService),
                                                      QString::fromLatin1(kPath),
                                                      QString::fromLatin1(kInterface),
                                                      saving ? QStringLiteral("SaveFile")
                                                             : QStringLiteral("OpenFile"));
    // An empty parent window is allowed; the portal then shows an unparented
    // dialog rather than refusing the call.
    msg << QString() << title << options;

    // Asynchronous: the portal shows its own dialog, so blocking the GUI thread
    // here would freeze the editor for as long as the user browses.
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, requestPath](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (m_handlePath != requestPath)
            return;
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError() || reply.value().path().isEmpty()) {
            finish({}, true);
            return;
        }
        const QString path = reply.value().path();
        // If the portal ignored our token it returns its own handle path; move
        // the subscription there.
        if (path != m_handlePath) {
            unsubscribe();
            m_handlePath = path;
            if (!subscribe())
                finish({}, true);
        }
    });
    return true;
}

void PortalFileChooser::onResponse(uint response, const QVariantMap &results, const QDBusMessage &message)
{
    if (!busy() || message.path() != m_handlePath)
        return;
    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    const QUrl file = response == 0 ? QUrl(uris.value(0)) : QUrl();
    const bool valid = file.isLocalFile() && !file.toLocalFile().isEmpty();
    finish(valid ? file : QUrl(), !valid && response != 1);
}

void PortalFileChooser::finish(const QUrl &file, bool useFallback)
{
    const bool saving = m_saving;
    unsubscribe();
    m_handlePath.clear();
    emit busyChanged();
    if (saving)
        emit saveFinished(file, useFallback);
    else
        emit openFinished(file, useFallback);
}

bool PortalFileChooser::subscribe()
{
    return QDBusConnection::sessionBus().connect(QString::fromLatin1(kService), m_handlePath,
        QString::fromLatin1(kRequestInterface), QStringLiteral("Response"),
        this, SLOT(onResponse(uint,QVariantMap,QDBusMessage)));
}

void PortalFileChooser::unsubscribe()
{
    QDBusConnection::sessionBus().disconnect(QString::fromLatin1(kService), m_handlePath,
        QString::fromLatin1(kRequestInterface), QStringLiteral("Response"),
        this, SLOT(onResponse(uint,QVariantMap,QDBusMessage)));
}
