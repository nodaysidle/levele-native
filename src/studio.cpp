#include "studio.h"

#include "exporter.h"
#include "templates.h"

#include <utility>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kMaxUndo = 100;
// Trashed designs stay restorable for a week.
constexpr int kTrashDays = 7;

QVariant colourVariant(const QColor &c) { return QVariant::fromValue(c); }

QString dataRoot()
{
    QString base = qEnvironmentVariable("XDG_DATA_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/share");
    return base + QStringLiteral("/levele");
}

struct TrashEntry
{
    QString documentId;
    QDateTime deletedAt;
};

TrashEntry trashEntry(const QString &key)
{
    // shortcut: legacy entries have no deletion timestamp, keep them until restored.
    if (Doc::isValidId(key))
        return {key, {}};

    const QStringList parts = key.split(QLatin1Char('.'));
    if (parts.size() != 3 || !Doc::isValidId(parts.at(0)) || !Doc::isValidId(parts.at(2)))
        return {};
    bool validTime = false;
    const qint64 timestamp = parts.at(1).toLongLong(&validTime);
    if (!validTime || timestamp <= 0)
        return {};
    return {parts.at(0), QDateTime::fromMSecsSinceEpoch(timestamp)};
}

QString localExportPath(const QUrl &url)
{
    if (url.isLocalFile())
        return url.toLocalFile();
    if (url.scheme().isEmpty())
        return url.toString();
    return QString();
}

QString safeImageExt(const QString &localPath, const QImageReader &reader)
{
    QString raw = QFileInfo(localPath).suffix().toLower();
    QString clean;
    for (const QChar ch : raw) {
        if ((ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
            || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))) {
            clean.append(ch);
            if (clean.size() >= 8)
                break;
        }
    }
    if (!clean.isEmpty())
        return clean;
    const QString fmt = QString::fromLatin1(reader.format()).toLower();
    return fmt.isEmpty() ? QStringLiteral("png") : fmt;
}

} // namespace

// ---------------------------------------------------------------- list model

ElementListModel::ElementListModel(Studio *studio)
    : QAbstractListModel(studio)
    , m_studio(studio)
{
}

int ElementListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_studio->elements().size();
}

QVariant ElementListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_studio->elements().size())
        return {};
    const Element &e = m_studio->elements().at(index.row());
    switch (role) {
    case IdRole:
        return e.id;
    case NameRole:
        return e.name.isEmpty() ? elementTypeLabel(e.type) : e.name;
    case TypeRole:
        return e.type;
    case GlyphRole:
        return elementTypeGlyph(e.type);
    case VisibleRole:
        return e.visible;
    case LockedRole:
        return e.locked;
    case SelectedRole:
        return m_studio->isSelected(e.id);
    case TextRole:
        return e.text;
    default:
        return {};
    }
}

bool ElementListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_studio->elements().size())
        return false;
    Element &e = m_studio->elements()[index.row()];
    if (role == NameRole) {
        m_studio->renameElement(e.id, value.toString());
        return true;
    }
    if (role == VisibleRole) {
        m_studio->setElementVisible(e.id, value.toBool());
        return true;
    }
    if (role == LockedRole) {
        m_studio->setElementLocked(e.id, value.toBool());
        return true;
    }
    return false;
}

QHash<int, QByteArray> ElementListModel::roleNames() const
{
    return {{IdRole, "elemId"},   {NameRole, "elemName"}, {TypeRole, "elemType"},
            {GlyphRole, "elemGlyph"}, {VisibleRole, "elemVisible"},
            {LockedRole, "elemLocked"}, {SelectedRole, "elemSelected"},
            {TextRole, "elemText"}};
}

void ElementListModel::refresh()
{
    beginResetModel();
    endResetModel();
}

void ElementListModel::refreshSelection()
{
    if (rowCount() == 0)
        return;
    emit dataChanged(index(0), index(rowCount() - 1), {SelectedRole});
}

// -------------------------------------------------------------------- studio

Studio::Studio(QObject *parent)
    : QObject(parent)
    , m_model(this)
{
    m_doc = makeBlankDoc(1080, 1080, QStringLiteral("Untitled"));
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(800);
    connect(&m_saveTimer, &QTimer::timeout, this, &Studio::saveNow);
    QDir().mkpath(docsDir());
    QDir().mkpath(assetsDir());
    purgeTrash();
}

Studio::~Studio()
{
    if (m_exportFuture.isRunning())
        m_exportFuture.waitForFinished();
}

Page &Studio::page()
{
    if (m_doc.pages.isEmpty())
        m_doc.pages.append(Page{Doc::newId(), QStringLiteral("#ffffff"), {}});
    m_pageIndex = qBound(0, m_pageIndex, m_doc.pages.size() - 1);
    return m_doc.pages[m_pageIndex];
}

const Page &Studio::page() const
{
    static const Page empty{Doc::newId(), QStringLiteral("#ffffff"), {}};
    if (m_doc.pages.isEmpty())
        return empty;
    return m_doc.pages.at(qBound(0, m_pageIndex, m_doc.pages.size() - 1));
}

QVector<Element> &Studio::elements() { return page().elements; }
const QVector<Element> &Studio::elements() const { return page().elements; }

Element *Studio::elementById(const QString &id)
{
    for (Element &e : elements()) {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

void Studio::setTitle(const QString &t)
{
    if (m_doc.title == t)
        return;
    pushUndo();
    m_doc.title = t;
    emit titleChanged();
    touch();
}

QVariantMap Studio::sel() const
{
    QVariantMap m;
    m["count"] = m_selection.size();
    m["multi"] = m_selection.size() > 1;
    if (m_selection.isEmpty())
        return m;

    const QVector<Element> &els = elements();
    const Element *primary = nullptr;
    for (const Element &e : els) {
        if (e.id == m_selection.first()) {
            primary = &e;
            break;
        }
    }
    if (!primary)
        return m;

    const Element &e = *primary;
    m["id"] = e.id;
    m["type"] = e.type;
    m["name"] = e.name;
    m["x"] = e.x;
    m["y"] = e.y;
    m["w"] = e.w;
    m["h"] = e.h;
    m["rotation"] = e.rotation;
    m["opacity"] = e.opacity;
    m["visible"] = e.visible;
    m["locked"] = e.locked;
    m["fill"] = colourVariant(e.fill);
    m["gradient"] = e.gradient;
    m["gradFrom"] = colourVariant(e.gradFrom);
    m["gradTo"] = colourVariant(e.gradTo);
    m["gradAngle"] = e.gradAngle;
    m["stroke"] = colourVariant(e.stroke);
    m["strokeWidth"] = e.strokeWidth;
    m["radius"] = e.radius;
    m["text"] = e.text;
    m["fontSize"] = e.fontSize;
    m["fontWeight"] = e.fontWeight;
    m["textAlign"] = e.textAlign;
    m["textVAlign"] = e.textVAlign;
    m["textColor"] = colourVariant(e.textColor);
    m["letterSpacing"] = e.letterSpacing;
    m["lineHeight"] = e.lineHeight;
    m["imagePath"] = e.imagePath;
    m["imageFit"] = e.imageFit;
    m["isText"] = e.isText();
    m["isImage"] = e.isImage();
    m["hasFill"] = e.hasFill();
    return m;
}

void Studio::pushUndo()
{
    pushUndoSnapshot(m_doc.toJson());
}

void Studio::pushUndoSnapshot(const QJsonObject &before)
{
    m_undo.append(before);
    while (m_undo.size() > kMaxUndo)
        m_undo.removeFirst();
    m_redo.clear();
    m_nudgeOpen = false;
    emit historyChanged();
    scheduleSave();
}

// A drag/resize/rotate takes the snapshot on press and only keeps it if the
// pointer actually moved: ten clicks on an element must not fill the history.
void Studio::takeGestureSnapshot()
{
    m_pending = m_doc.toJson();
}

void Studio::commitGestureSnapshot()
{
    if (m_pending.isEmpty())
        return;
    const QJsonObject before = m_pending;
    m_pending = QJsonObject();
    pushUndoSnapshot(before);
}

void Studio::discardGestureSnapshot()
{
    m_pending = QJsonObject();
}

void Studio::beginNudge()
{
    // One undo step per burst of arrow-key nudges.
    if (!m_nudgeOpen || m_nudgeTimer.elapsed() > 500) {
        m_undo.append(m_doc.toJson());
        while (m_undo.size() > kMaxUndo)
            m_undo.removeFirst();
        m_redo.clear();
        m_nudgeOpen = true;
        emit historyChanged();
    }
    m_nudgeTimer.restart();
    scheduleSave();
}

void Studio::touch()
{
    m_model.refresh();
    emit contentsChanged();
    scheduleSave();
}

void Studio::touchGeometry()
{
    emit geometryChanged();
    m_dirty = true;
    m_saveTimer.start();
}

const Element *Studio::primaryElement() const
{
    const QString id = primaryId();
    if (id.isEmpty())
        return nullptr;
    for (const Element &e : elements()) {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

void Studio::touchSelection()
{
    m_model.refreshSelection();
    emit selectionChanged();
    emit contentsChanged();
    scheduleSave();
}

// Selection is view state, not document state: repaint the layer rows and the
// Inspector, but do not dirty the document or repaint page thumbnails.
void Studio::touchSelectionOnly()
{
    m_model.refreshSelection();
    emit selectionChanged();
}

void Studio::scheduleSave()
{
    m_dirty = true;
    m_saveTimer.start();
}

void Studio::setStatus(const QString &s)
{
    m_status = s;
    emit statusChanged();
}

void Studio::clearStatus()
{
    if (m_status.isEmpty())
        return;
    m_status.clear();
    emit statusChanged();
}

void Studio::setTyping(bool t)
{
    if (m_typing == t)
        return;
    m_typing = t;
    emit typingChanged();
}

QString Studio::docsDir() const { return dataRoot() + QStringLiteral("/docs"); }
QString Studio::assetsDir() const { return dataRoot() + QStringLiteral("/assets"); }
QString Studio::docPath(const QString &id) const { return docsDir() + QLatin1Char('/') + id + QStringLiteral(".json"); }
QString Studio::trashDir() const { return docsDir() + QStringLiteral("/.trash"); }
QString Studio::dataRoot() const { return ::dataRoot(); }

QString Studio::suggestedExportName(const QString &extension) const
{
    QString base = m_doc.title.simplified();
    if (base.isEmpty() || base == QLatin1String("Untitled"))
        base = QStringLiteral("Levele design");
    static const QRegularExpression unsafeFileChars(QStringLiteral("[\\\\/:*?\"<>|]"));
    base.replace(unsafeFileChars, QStringLiteral("-"));
    return base + QLatin1Char('.') + (extension.isEmpty() ? QStringLiteral("png") : extension);
}

void Studio::newDocument(double w, double h)
{
    if (m_editing && !saveOnQuit())
        return;
    const double cw = std::isfinite(w) ? qBound(16.0, w, 16384.0) : 1080.0;
    const double ch = std::isfinite(h) ? qBound(16.0, h, 16384.0) : 1080.0;
    m_doc = makeBlankDoc(cw, ch, QStringLiteral("Untitled"));
    m_pageIndex = 0;
    m_selection.clear();
    resetHistory();
    m_editing = true;
    emit editingChanged();
    emit docChanged();
    emit documentReplaced();
    emit titleChanged();
    emit pagesChanged();
    emit selectionChanged();
    touch();
    saveNowInternal(false);
}

void Studio::newFromTemplate(int index)
{
    const QVector<TemplateInfo> all = buildTemplates();
    if (index < 0 || index >= all.size())
        return;
    if (m_editing && !saveOnQuit())
        return;
    m_doc = all.at(index).doc;
    m_pageIndex = 0;
    m_selection.clear();
    resetHistory();
    m_editing = true;
    emit editingChanged();
    emit docChanged();
    emit documentReplaced();
    emit titleChanged();
    emit pagesChanged();
    emit selectionChanged();
    touch();
    saveNowInternal(false);
}

bool Studio::openDoc(const QString &id)
{
    if (!Doc::isValidId(id)) {
        setStatus(tr("Couldn't open: that is not a document id"));
        return false;
    }
    if (m_editing && !saveOnQuit())
        return false;
    QFile f(docPath(id));
    if (!f.open(QIODevice::ReadOnly)) {
        setStatus(tr("Couldn't open: ") + f.errorString());
        return false;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.isEmpty()) {
        setStatus(tr("Couldn't open: not a Levele document"));
        return false;
    }
    m_doc = Doc::fromJson(o);
    m_doc.id = id;
    discardChanges();
    m_pageIndex = 0;
    m_selection.clear();
    resetHistory();
    m_editing = true;
    emit editingChanged();
    emit docChanged();
    emit documentReplaced();
    emit titleChanged();
    emit pagesChanged();
    emit selectionChanged();
    m_model.refresh();
    emit contentsChanged();
    return true;
}

bool Studio::closeDoc()
{
    if (!prepareToLeave(false))
        return false;
    m_editing = false;
    m_selection.clear();
    emit editingChanged();
    emit selectionChanged();
    return true;
}

void Studio::discardChanges()
{
    m_saveTimer.stop();
    m_dirty = false;
}

bool Studio::prepareToLeave(bool quitting)
{
    if (m_editing)
        emit closingSoon();
    if (saveOnQuit())
        return true;
    emit leaveBlocked(quitting);
    return false;
}

bool Studio::saveNow()
{
    return saveNowInternal(true);
}

// The single write path. `announce` puts "Saved" in the toast; failures are
// always announced, because a silent failed save is a lost design.
bool Studio::saveNowInternal(bool announce)
{
    m_saveTimer.stop();
    if (m_doc.id.isEmpty())
        return true;

    if (!QDir().mkpath(docsDir())) {
        setStatus(tr("Couldn't save: cannot create %1").arg(docsDir()));
        return false;
    }

    QString error;
    if (!writeDoc(docPath(m_doc.id), m_doc, &error)) {
        setStatus(tr("Couldn't save: ") + error);
        return false;
    }
    m_dirty = false;
    if (announce)
        setStatus(tr("Saved"));
    return true;
}

bool Studio::saveWithFeedback()
{
    return saveNowInternal(true);
}

bool Studio::saveOnQuit()
{
    return !m_dirty || saveNowInternal(false);
}

// QSaveFile writes a temporary file and renames it into place, so a crash, a
// full disk or a power loss can never leave a half-written document behind.
bool Studio::writeDoc(const QString &path, const Doc &doc, QString *error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    const QByteArray data = QJsonDocument(doc.toJson()).toJson(QJsonDocument::Indented);
    if (f.write(data) != data.size()) {
        if (error)
            *error = f.errorString();
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

QVariantList Studio::listDocs() const
{
    QVariantList out;
    QDir dir(docsDir());
    // Only real documents are listed: docs/.trash is a directory, so trashed
    // designs never show up here and can still be restored.
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (const QFileInfo &fi : files) {
        if (fi.absolutePath() == trashDir())
            continue;
        const QString id = fi.completeBaseName();
        if (!Doc::isValidId(id))
            continue;
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.isEmpty())
            continue;
        QVariantMap m;
        // The file name is the truth: a hand-copied file whose JSON id says
        // something else must still be openable and deletable.
        m["id"] = id;
        m["title"] = o.value("title").toString(QStringLiteral("Untitled"));
        m["w"] = o.value("width").toDouble(1080);
        m["h"] = o.value("height").toDouble(1080);
        m["pages"] = o.value("pages").toArray().size();
        m["modified"] = fi.lastModified().toString(QStringLiteral("d MMM yyyy · HH:mm"));
        m["modifiedMs"] = double(fi.lastModified().toMSecsSinceEpoch());
        out.append(m);
    }
    return out;
}

QVariantList Studio::listTrash() const
{
    QVariantList out;
    const QFileInfoList files = QDir(trashDir()).entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo &file : files) {
        const QString key = file.completeBaseName();
        const TrashEntry entry = trashEntry(key);
        if (entry.documentId.isEmpty())
            continue;

        QFile f(file.absoluteFilePath());
        QString title = entry.documentId;
        if (f.open(QIODevice::ReadOnly)) {
            const auto o = QJsonDocument::fromJson(f.readAll()).object();
            title = o.value(QStringLiteral("title")).toString(title);
        }
        QVariantMap row;
        row["id"] = key;
        row["title"] = title;
        row["deleted"] = entry.deletedAt.toString(QStringLiteral("d MMM yyyy · HH:mm"));
        row["deletedMs"] = entry.deletedAt.isValid() ? entry.deletedAt.toMSecsSinceEpoch() : 0;
        out.append(row);
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("deletedMs").toLongLong() > b.toMap().value("deletedMs").toLongLong();
    });
    return out;
}

void Studio::deleteDoc(const QString &id)
{
    if (!Doc::isValidId(id)) {
        setStatus(tr("Couldn't delete: that is not a document id"));
        return;
    }
    if (m_editing && id == m_doc.id) {
        setStatus(tr("Close this design before moving it to trash"));
        return;
    }
    const QString source = docPath(id);
    if (!QFileInfo::exists(source)) {
        setStatus(tr("Couldn't delete: the design is already gone"));
        return;
    }

    // Read the title first so the Undo toast can name what it is offering back.
    QString title = id;
    QFile f(source);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        const QString t = o.value(QStringLiteral("title")).toString();
        if (!t.isEmpty())
            title = t;
    }

    if (!QDir().mkpath(trashDir())) {
        setStatus(tr("Couldn't delete: cannot create %1").arg(trashDir()));
        return;
    }
    // The timestamp and nonce travel with the file in the same atomic move.
    const QString key = QStringLiteral("%1.%2.%3").arg(id)
                            .arg(QDateTime::currentMSecsSinceEpoch()).arg(Doc::newId());
    const QString target = trashDir() + QLatin1Char('/') + key + QStringLiteral(".json");
    if (!QFile::rename(source, target)) {
        setStatus(tr("Couldn't delete \"%1\"").arg(title));
        return;
    }

    m_trashId = key;
    emit trashChanged();
    setStatus(tr("Moved \"%1\" to trash").arg(title));
}

// Puts a trashed design back. Also what the Undo button on the toast calls.
bool Studio::restoreDoc(const QString &key)
{
    const TrashEntry entry = trashEntry(key);
    if (entry.documentId.isEmpty()) {
        setStatus(tr("Couldn't restore: that is not a document id"));
        return false;
    }
    const QString source = trashDir() + QLatin1Char('/') + key + QStringLiteral(".json");
    if (!QFileInfo::exists(source)) {
        setStatus(tr("Couldn't restore: the design is no longer in the trash"));
        return false;
    }
    if (!QDir().mkpath(docsDir())) {
        setStatus(tr("Couldn't restore: cannot create %1").arg(docsDir()));
        return false;
    }
    const bool collision = QFileInfo::exists(docPath(entry.documentId))
        || (m_editing && entry.documentId == m_doc.id);
    const QString target = docPath(collision ? Doc::newId() : entry.documentId);
    if (!QFile::rename(source, target)) {
        setStatus(tr("Couldn't restore the design"));
        return false;
    }
    if (m_trashId == key)
        m_trashId.clear();
    emit trashChanged();
    setStatus(collision ? tr("Restored as a separate design") : tr("Restored"));
    return true;
}

void Studio::undoDelete()
{
    if (trashPending())
        restoreDoc(QString(m_trashId));
}

// Trashed designs are kept for a week, then removed on the next start.
void Studio::purgeTrash()
{
    QDir trash(trashDir());
    if (!trash.exists())
        return;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-kTrashDays);
    const QFileInfoList old = trash.entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo &fi : old) {
        const QDateTime deletedAt = trashEntry(fi.completeBaseName()).deletedAt;
        if (deletedAt.isValid() && deletedAt < cutoff)
            QFile::remove(fi.absoluteFilePath());
    }
}

void Studio::duplicateDoc(const QString &id)
{
    if (!Doc::isValidId(id)) {
        setStatus(tr("Couldn't copy: that is not a document id"));
        return;
    }
    QFile src(docPath(id));
    if (!src.open(QIODevice::ReadOnly)) {
        setStatus(tr("Couldn't copy: ") + src.errorString());
        return;
    }
    Doc d = Doc::fromJson(QJsonDocument::fromJson(src.readAll()).object());
    d.id = Doc::newId();
    d.title += QStringLiteral(" copy");
    QString error;
    if (!writeDoc(docPath(d.id), d, &error)) {
        setStatus(tr("Couldn't save the copy: ") + error);
        return;
    }
    setStatus(tr("Duplicated"));
}

QVariantList Studio::presets() const
{
    QVariantList out;
    const QVector<DocPreset> presets = buildPresets();
    for (const DocPreset &p : presets) {
        QVariantMap m;
        m["name"] = p.name;
        m["w"] = p.w;
        m["h"] = p.h;
        m["label"] = QStringLiteral("%1 × %2").arg(int(p.w)).arg(int(p.h));
        out.append(m);
    }
    return out;
}

QVariantList Studio::templates() const
{
    QVariantList out;
    const QVector<TemplateInfo> all = buildTemplates();
    for (const TemplateInfo &t : all) {
        QVariantMap m;
        m["name"] = t.name;
        m["category"] = t.category;
        m["w"] = t.doc.width;
        m["h"] = t.doc.height;
        m["pages"] = t.doc.pages.size();
        QString bg = t.doc.pages.isEmpty() ? QStringLiteral("#ffffff") : t.doc.pages.first().background;
        m["bg"] = bg;
        out.append(m);
    }
    return out;
}

QString Studio::docJson(const QString &id) const
{
    if (!Doc::isValidId(id))
        return {};
    QFile f(docPath(id));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.isEmpty())
        return {};
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString Studio::templateJson(int index) const
{
    const QVector<TemplateInfo> all = buildTemplates();
    if (index < 0 || index >= all.size())
        return {};
    return QString::fromUtf8(QJsonDocument(all.at(index).doc.toJson()).toJson(QJsonDocument::Compact));
}

// ----------------------------------------------------------------- selection

void Studio::selectElement(const QString &id, bool additive)
{
    if (!additive)
        m_selection.clear();
    if (m_selection.contains(id))
        m_selection.removeAll(id);
    else
        m_selection.append(id);
    touchSelectionOnly();
}

void Studio::selectAll()
{
    m_selection.clear();
    for (const Element &e : std::as_const(elements())) {
        if (!e.locked)
            m_selection.append(e.id);
    }
    touchSelectionOnly();
}

void Studio::clearSelection()
{
    if (m_selection.isEmpty())
        return;
    m_selection.clear();
    touchSelectionOnly();
}

void Studio::setSelProp(const QString &key, const QVariant &value)
{
    if (m_selection.isEmpty())
        return;

    const Element *selRef = nullptr;
    int unlockedCount = 0;
    for (const Element &e : std::as_const(elements())) {
        if (e.id == m_selection.first())
            selRef = &e;
        if (m_selection.contains(e.id) && !e.locked)
            ++unlockedCount;
    }
    if (unlockedCount == 0)
        return;
    if (!selRef) {
        for (const Element &e : std::as_const(elements())) {
            if (m_selection.contains(e.id) && !e.locked) {
                selRef = &e;
                break;
            }
        }
    }
    pushUndo();

    const bool multi = m_selection.size() > 1;
    // With several elements selected, X/Y move the whole selection by the delta
    // the primary sees and W/H scale each element — an absolute value written to
    // every element would stack them all on top of each other.
    double deltaX = 0;
    double deltaY = 0;
    double deltaW = 0;
    double deltaH = 0;
    double scaleW = 1;
    double scaleH = 1;
    bool useScaleW = false;
    bool useScaleH = false;
    if (multi && selRef) {
        if (key == QLatin1String("x"))
            deltaX = value.toDouble() - selRef->x;
        else if (key == QLatin1String("y"))
            deltaY = value.toDouble() - selRef->y;
        else if (key == QLatin1String("w")) {
            if (qAbs(selRef->w) > 0.5) {
                scaleW = value.toDouble() / selRef->w;
                useScaleW = true;
            } else {
                deltaW = value.toDouble() - selRef->w;
            }
        } else if (key == QLatin1String("h")) {
            if (qAbs(selRef->h) > 0.5) {
                scaleH = value.toDouble() / selRef->h;
                useScaleH = true;
            } else {
                deltaH = value.toDouble() - selRef->h;
            }
        }
    }

    for (Element &e : elements()) {
        if (!m_selection.contains(e.id) || e.locked)
            continue;
        if (key == QLatin1String("x")) e.x = multi ? e.x + deltaX : value.toDouble();
        else if (key == QLatin1String("y")) e.y = multi ? e.y + deltaY : value.toDouble();
        else if (key == QLatin1String("w")) {
            if (multi) {
                if (e.isLineLike())
                    e.w = useScaleW ? (e.w * scaleW) : (e.w + deltaW);
                else
                    e.w = useScaleW ? qMax(1.0, e.w * qMax(0.01, qAbs(scaleW)))
                                    : qMax(1.0, e.w + deltaW);
            } else {
                e.w = e.isLineLike() ? value.toDouble() : qMax(1.0, value.toDouble());
            }
        } else if (key == QLatin1String("h")) {
            if (multi) {
                if (e.isLineLike())
                    e.h = useScaleH ? (e.h * scaleH) : (e.h + deltaH);
                else
                    e.h = useScaleH ? qMax(1.0, e.h * qMax(0.01, qAbs(scaleH)))
                                    : qMax(1.0, e.h + deltaH);
            } else {
                e.h = e.isLineLike() ? value.toDouble() : qMax(1.0, value.toDouble());
            }
        } else if (key == QLatin1String("rotation")) e.rotation = value.toDouble();
        else if (key == QLatin1String("opacity")) e.opacity = qBound(0.0, value.toDouble(), 1.0);
        else if (key == QLatin1String("fill")) e.fill = value.value<QColor>();
        else if (key == QLatin1String("gradient")) e.gradient = value.toBool();
        else if (key == QLatin1String("gradFrom")) e.gradFrom = value.value<QColor>();
        else if (key == QLatin1String("gradTo")) e.gradTo = value.value<QColor>();
        else if (key == QLatin1String("gradAngle")) e.gradAngle = value.toDouble();
        else if (key == QLatin1String("stroke")) e.stroke = value.value<QColor>();
        else if (key == QLatin1String("strokeWidth")) e.strokeWidth = qMax(0.0, value.toDouble());
        else if (key == QLatin1String("radius")) e.radius = qMax(0.0, value.toDouble());
        else if (key == QLatin1String("text")) e.text = value.toString();
        else if (key == QLatin1String("fontSize")) e.fontSize = qBound(4.0, value.toDouble(), 800.0);
        else if (key == QLatin1String("fontWeight")) e.fontWeight = qBound(1, value.toInt(), 1000);
        else if (key == QLatin1String("textAlign")) e.textAlign = value.toString();
        else if (key == QLatin1String("textVAlign")) e.textVAlign = value.toString();
        else if (key == QLatin1String("textColor")) e.textColor = value.value<QColor>();
        else if (key == QLatin1String("letterSpacing")) e.letterSpacing = value.toDouble();
        else if (key == QLatin1String("lineHeight")) e.lineHeight = qBound(0.6, value.toDouble(), 4.0);
        else if (key == QLatin1String("imageFit")) e.imageFit = value.toString();
    }
    touch();
    emit selectionChanged();
}

QString Studio::addElement(const QString &type)
{
    return addElementInternal(type, QPointF(), false);
}

QString Studio::addElementAt(const QString &type, const QPointF &docPos)
{
    return addElementInternal(type, docPos, true);
}

QString Studio::addElementInternal(const QString &type, const QPointF &docPos, bool atPoint)
{
    pushUndo();
    Element e = Element::make(type);
    if (atPoint) {
        // Under the pointer, exactly where it was clicked.
        e.x = docPos.x();
        e.y = docPos.y();
    } else {
        const int n = elements().size();
        e.x = qRound((m_doc.width - e.w) / 2.0) + (n % 5) * 24;
        e.y = qRound((m_doc.height - e.h) / 2.0) + (n % 5) * 24;
    }
    e.fill = e.fill.isValid() ? e.fill : QColor(QStringLiteral("#8a8a8a"));
    elements().append(e);
    m_selection = {e.id};
    touch();
    emit selectionChanged();
    return e.id;
}

QString Studio::addImage(const QUrl &source, double naturalW, double naturalH)
{
    const QString local = source.isLocalFile() ? source.toLocalFile() : QString();
    const QFileInfo fi(local);
    if (local.isEmpty() || !fi.exists() || !fi.isFile()) {
        setStatus(tr("Image not found"));
        return {};
    }

    // Size comes from the header, so a 40 MP photo is not decoded just to
    // learn how big it is. Anything the reader cannot parse is refused loudly
    // instead of becoming an 800x800 element pointing at junk.
    const QImageReader reader(local);
    if (!reader.canRead()) {
        setStatus(tr("Not an image: %1").arg(QFileInfo(local).fileName()));
        return {};
    }
    const QSize natural = reader.size();
    double nw = naturalW > 0 ? naturalW : natural.width();
    double nh = naturalH > 0 ? naturalH : natural.height();
    if (nw <= 0 || nh <= 0) {
        nw = 800;
        nh = 800;
    }

    // Import into the app's own asset store so the document survives the
    // original file moving away.
    if (!QDir().mkpath(assetsDir())) {
        setStatus(tr("Couldn't create the asset folder"));
        return {};
    }
    const QString ext = safeImageExt(local, reader);
    const QString dest = assetsDir() + QLatin1Char('/') + Doc::newId() + QLatin1Char('.') + ext;
    if (!QFile::copy(local, dest)) {
        setStatus(tr("Couldn't import %1").arg(QFileInfo(local).fileName()));
        return {};
    }

    pushUndo();
    Element e = Element::make(QStringLiteral("image"));
    e.imagePath = dest;
    const double maxW = m_doc.width * 0.6;
    const double maxH = m_doc.height * 0.6;
    const double s = qMin(1.0, qMin(maxW / nw, maxH / nh));
    e.w = qRound(nw * s);
    e.h = qRound(nh * s);
    if (m_pointerInside) {
        // Place it under the pointer that asked for it.
        e.x = qRound(m_pointerPos.x() - e.w / 2.0);
        e.y = qRound(m_pointerPos.y() - e.h / 2.0);
    } else {
        e.x = qRound((m_doc.width - e.w) / 2.0);
        e.y = qRound((m_doc.height - e.h) / 2.0);
    }
    e.name = QFileInfo(local).completeBaseName();
    elements().append(e);
    m_selection = {e.id};
    touch();
    emit selectionChanged();
    return e.id;
}

// Swaps the picture but keeps the frame: geometry, rotation and radius belong
// to the design, not to the file.
bool Studio::replaceImage(const QString &id, const QUrl &source)
{
    Element *e = elementById(id);
    if (!e || !e->isImage()) {
        setStatus(tr("Select an image to replace"));
        return false;
    }
    if (e->locked) {
        setStatus(tr("Unlock the layer before replacing its image"));
        return false;
    }
    const QString local = source.isLocalFile() ? source.toLocalFile() : QString();
    const QFileInfo fi(local);
    if (local.isEmpty() || !fi.exists() || !fi.isFile()) {
        setStatus(tr("Image not found"));
        return false;
    }
    const QImageReader reader(local);
    if (!reader.canRead()) {
        setStatus(tr("Not an image: %1").arg(QFileInfo(local).fileName()));
        return false;
    }

    QDir().mkpath(assetsDir());
    const QString ext = safeImageExt(local, reader);
    const QString dest = assetsDir() + QLatin1Char('/') + Doc::newId() + QLatin1Char('.') + ext;
    if (!QFile::copy(local, dest)) {
        setStatus(tr("Couldn't import %1").arg(QFileInfo(local).fileName()));
        return false;
    }

    pushUndo();
    e->imagePath = dest;
    if (e->name.isEmpty() || e->name == QLatin1String("Image"))
        e->name = QFileInfo(local).completeBaseName();
    touch();
    emit selectionChanged();
    setStatus(tr("Replaced the image"));
    return true;
}

void Studio::setPointerPosition(const QPointF &docPt, bool inside)
{
    if (inside)
        m_pointerPos = docPt;
    m_pointerInside = inside;
}

namespace {
QVariant elementValue(const Element &e, const QString &key)
{
    if (key == QLatin1String("x")) return e.x;
    if (key == QLatin1String("y")) return e.y;
    if (key == QLatin1String("w")) return e.w;
    if (key == QLatin1String("h")) return e.h;
    if (key == QLatin1String("rotation")) return e.rotation;
    if (key == QLatin1String("opacity")) return e.opacity;
    if (key == QLatin1String("fontSize")) return e.fontSize;
    if (key == QLatin1String("textAlign")) return e.textAlign;
    if (key == QLatin1String("textVAlign")) return e.textVAlign;
    return QVariant();
}
} // namespace

// "Mixed" in the Inspector: the selected elements do not agree on a value.
bool Studio::selMixed(const QString &key) const
{
    if (m_selection.size() < 2)
        return false;
    QVariant first;
    bool haveFirst = false;
    for (const Element &e : std::as_const(elements())) {
        if (!m_selection.contains(e.id))
            continue;
        const QVariant v = elementValue(e, key);
        if (!v.isValid())
            return false;
        if (!haveFirst) {
            first = v;
            haveFirst = true;
        } else if (v != first) {
            return true;
        }
    }
    return false;
}

void Studio::deleteSelected()
{
    if (m_selection.isEmpty())
        return;
    QVector<Element> kept;
    kept.reserve(elements().size());
    for (const Element &e : std::as_const(elements())) {
        if (e.locked || !m_selection.contains(e.id))
            kept.append(e);
    }
    if (kept.size() == elements().size())
        return;
    pushUndo();
    elements() = kept;
    m_selection.clear();
    touch();
    emit selectionChanged();
}

void Studio::duplicateSelected()
{
    if (m_selection.isEmpty())
        return;
    pushUndo();
    QStringList fresh;
    const QVector<Element> source = elements();
    for (const Element &e : source) {
        if (!m_selection.contains(e.id))
            continue;
        Element copy = e;
        copy.id = Doc::newId();
        copy.x += 28;
        copy.y += 28;
        copy.name = e.name + QStringLiteral(" copy");
        elements().append(copy);
        fresh.append(copy.id);
    }
    m_selection = fresh;
    touch();
    emit selectionChanged();
}

void Studio::reorderElement(int from, int to)
{
    QVector<Element> &els = elements();
    if (from < 0 || from >= els.size() || to < 0 || to >= els.size() || from == to)
        return;
    pushUndo();
    els.move(from, to);
    touch();
}

void Studio::raiseSelected()
{
    if (m_selection.isEmpty())
        return;
    QVector<Element> &els = elements();
    bool any = false;
    for (int i = els.size() - 2; i >= 0; --i) {
        if (m_selection.contains(els[i].id) && !m_selection.contains(els[i + 1].id)) {
            if (!any) {
                pushUndo();
                any = true;
            }
            els.swapItemsAt(i, i + 1);
        }
    }
    if (any)
        touch();
}

void Studio::lowerSelected()
{
    if (m_selection.isEmpty())
        return;
    QVector<Element> &els = elements();
    bool any = false;
    for (int i = 1; i < els.size(); ++i) {
        if (m_selection.contains(els[i].id) && !m_selection.contains(els[i - 1].id)) {
            if (!any) {
                pushUndo();
                any = true;
            }
            els.swapItemsAt(i, i - 1);
        }
    }
    if (any)
        touch();
}

void Studio::frontSelected()
{
    if (m_selection.isEmpty())
        return;
    QVector<Element> moved, kept;
    for (const Element &e : std::as_const(elements())) {
        if (m_selection.contains(e.id))
            moved.append(e);
        else
            kept.append(e);
    }
    const QVector<Element> next = kept + moved;
    bool changed = false;
    const QVector<Element> &cur = elements();
    for (int i = 0; i < cur.size(); ++i) {
        if (cur.at(i).id != next.at(i).id) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return;
    pushUndo();
    elements() = next;
    touch();
}

void Studio::backSelected()
{
    if (m_selection.isEmpty())
        return;
    QVector<Element> moved, kept;
    for (const Element &e : std::as_const(elements())) {
        if (m_selection.contains(e.id))
            moved.append(e);
        else
            kept.append(e);
    }
    const QVector<Element> next = moved + kept;
    bool changed = false;
    const QVector<Element> &cur = elements();
    for (int i = 0; i < cur.size(); ++i) {
        if (cur.at(i).id != next.at(i).id) {
            changed = true;
            break;
        }
    }
    if (!changed)
        return;
    pushUndo();
    elements() = next;
    touch();
}

void Studio::setElementVisible(const QString &id, bool visible)
{
    Element *e = elementById(id);
    if (!e || e->visible == visible)
        return;
    pushUndo();
    e->visible = visible;
    touch();
}

void Studio::setElementLocked(const QString &id, bool locked)
{
    Element *e = elementById(id);
    if (!e || e->locked == locked)
        return;
    pushUndo();
    e->locked = locked;
    if (locked)
        m_selection.removeAll(id);
    touch();
    emit selectionChanged();
}

void Studio::renameElement(const QString &id, const QString &name)
{
    Element *e = elementById(id);
    if (!e || e->name == name)
        return;
    pushUndo();
    e->name = name;
    touch();
    if (m_selection.contains(id))
        emit selectionChanged();
}

void Studio::alignSelected(const QString &mode)
{
    if (m_selection.isEmpty())
        return;

    QVector<Element *> picked;
    for (Element &e : elements()) {
        if (m_selection.contains(e.id) && !e.locked)
            picked.append(&e);
    }
    if (picked.isEmpty())
        return;
    pushUndo();

    // One element aligns to the page, several align to their shared bounds.
    QRectF bounds(0, 0, m_doc.width, m_doc.height);
    if (picked.size() > 1) {
        bounds = picked.first()->rect();
        for (Element *e : picked)
            bounds = bounds.united(e->rect());
    }

    for (Element *e : picked) {
        const QRectF r = e->rect();
        if (mode == QLatin1String("left"))
            e->x += bounds.left() - r.left();
        else if (mode == QLatin1String("hcenter"))
            e->x += bounds.center().x() - r.center().x();
        else if (mode == QLatin1String("right"))
            e->x += bounds.right() - r.right();
        else if (mode == QLatin1String("top"))
            e->y += bounds.top() - r.top();
        else if (mode == QLatin1String("vcenter"))
            e->y += bounds.center().y() - r.center().y();
        else if (mode == QLatin1String("bottom"))
            e->y += bounds.bottom() - r.bottom();
    }
    touch();
    emit selectionChanged();
}

void Studio::distributeSelected(const QString &axis)
{
    QVector<Element *> picked;
    for (Element &e : elements()) {
        if (m_selection.contains(e.id) && !e.locked)
            picked.append(&e);
    }
    if (picked.size() < 3)
        return;
    pushUndo();

    const bool horizontal = axis == QLatin1String("h");
    std::sort(picked.begin(), picked.end(), [horizontal](Element *a, Element *b) {
        return horizontal ? a->rect().left() < b->rect().left() : a->rect().top() < b->rect().top();
    });

    const QRectF firstRect = picked.first()->rect();
    const QRectF lastRect = picked.last()->rect();
    double start = horizontal ? firstRect.left() : firstRect.top();
    double end = horizontal ? lastRect.right() : lastRect.bottom();
    double used = 0;
    for (Element *e : picked)
        used += horizontal ? e->rect().width() : e->rect().height();
    const double gap = (end - start - used) / double(picked.size() - 1);

    double cursor = start;
    for (Element *e : picked) {
        const QRectF r = e->rect();
        if (horizontal) {
            e->x += qRound(cursor) - r.left();
            cursor += r.width() + gap;
        } else {
            e->y += qRound(cursor) - r.top();
            cursor += r.height() + gap;
        }
    }
    touch();
    emit selectionChanged();
}

// --------------------------------------------------------------------- pages

void Studio::addPage()
{
    pushUndo();
    Page p;
    p.id = Doc::newId();
    p.background = page().background;
    m_doc.pages.append(p);
    m_pageIndex = m_doc.pages.size() - 1;
    m_selection.clear();
    emit pagesChanged();
    emit selectionChanged();
    touch();
}

void Studio::duplicatePage(int index)
{
    if (index < 0 || index >= m_doc.pages.size())
        return;
    pushUndo();
    Page copy = m_doc.pages.at(index);
    copy.id = Doc::newId();
    for (Element &e : copy.elements)
        e.id = Doc::newId();
    m_doc.pages.insert(index + 1, copy);
    m_pageIndex = index + 1;
    m_selection.clear();
    emit pagesChanged();
    emit selectionChanged();
    touch();
}

void Studio::deletePage(int index)
{
    if (m_doc.pages.size() <= 1 || index < 0 || index >= m_doc.pages.size())
        return;
    pushUndo();
    m_doc.pages.removeAt(index);
    m_pageIndex = qBound(0, m_pageIndex, m_doc.pages.size() - 1);
    m_selection.clear();
    emit pagesChanged();
    emit selectionChanged();
    touch();
}

void Studio::setCurrentPage(int index)
{
    if (index < 0 || index >= m_doc.pages.size() || index == m_pageIndex)
        return;
    m_pageIndex = index;
    m_selection.clear();
    emit pagesChanged();
    emit selectionChanged();
    m_model.refresh();
    emit contentsChanged();
}

void Studio::movePage(int from, int to)
{
    if (from < 0 || from >= m_doc.pages.size() || to < 0 || to >= m_doc.pages.size() || from == to)
        return;
    pushUndo();
    m_doc.pages.move(from, to);
    if (m_pageIndex == from)
        m_pageIndex = to;
    else if (from < m_pageIndex && m_pageIndex <= to)
        --m_pageIndex;
    else if (to <= m_pageIndex && m_pageIndex < from)
        ++m_pageIndex;
    emit pagesChanged();
    touch();
}

void Studio::setPageBackground(const QString &colour)
{
    if (!QColor::fromString(colour).isValid() || page().background == colour)
        return;
    pushUndo();
    page().background = colour;
    touch();
}

// ------------------------------------------------------------------- history

void Studio::resetHistory()
{
    m_undo.clear();
    m_redo.clear();
    m_nudgeOpen = false;
    emit historyChanged();
}

void Studio::restore(const QJsonObject &snapshot)
{
    m_doc = Doc::fromJson(snapshot);
    m_pageIndex = qBound(0, m_pageIndex, m_doc.pages.size() - 1);
    QStringList kept;
    for (const QString &id : std::as_const(m_selection)) {
        if (elementById(id))
            kept.append(id);
    }
    m_selection = kept;
    m_model.refresh();
    emit docChanged();
    emit titleChanged();
    emit pagesChanged();
    emit selectionChanged();
    emit contentsChanged();
    scheduleSave();
}

void Studio::undo()
{
    if (m_undo.isEmpty())
        return;
    m_nudgeOpen = false;
    m_redo.append(m_doc.toJson());
    const QJsonObject snap = m_undo.takeLast();
    restore(snap);
    emit historyChanged();
}

void Studio::redo()
{
    if (m_redo.isEmpty())
        return;
    m_nudgeOpen = false;
    m_undo.append(m_doc.toJson());
    const QJsonObject snap = m_redo.takeLast();
    restore(snap);
    emit historyChanged();
}

// -------------------------------------------------------------------- export

bool Studio::beginExport(const QString &label, const QString &target)
{
    if (m_exporting) {
        setStatus(tr("An export is already running"));
        return false;
    }
    if (target.isEmpty()) {
        setStatus(tr("Couldn't export %1: no file chosen").arg(label));
        return false;
    }
    const QFileInfo fi(target);
    const QString dir = fi.absolutePath();
    if (!QDir().mkpath(dir) || !QFileInfo(dir).isWritable()) {
        setStatus(tr("Couldn't export %1: %2 is not writable").arg(label, dir));
        return false;
    }
    m_exporting = true;
    emit exportingChanged();
    setStatus(tr("Exporting %1…").arg(label));
    return true;
}

void Studio::finishExport(const QString &error, const QString &label, const QString &target)
{
    m_exporting = false;
    m_exportFuture = QFuture<QString>();
    emit exportingChanged();
    if (error.isEmpty())
        setStatus(tr("Exported %1 · %2").arg(label, QFileInfo(target).fileName()));
    else
        setStatus(tr("Couldn't export %1: %2").arg(label, error));
}

void Studio::exportPng(const QUrl &path, int pageIndex, double scale)
{
    const QString target = localExportPath(path);
    if (!beginExport(QStringLiteral("PNG"), target))
        return;
    const Doc copy = m_doc;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, target]() {
        const QString error = watcher->result();
        watcher->deleteLater();
        finishExport(error, QStringLiteral("PNG"), target);
    });
    m_exportFuture = QtConcurrent::run([copy, pageIndex, scale, target]() {
        QString error;
        Exporter::writePng(copy, pageIndex, scale, target, &error);
        return error;
    });
    watcher->setFuture(m_exportFuture);
}

void Studio::exportPdf(const QUrl &path)
{
    const QString target = localExportPath(path);
    if (!beginExport(QStringLiteral("PDF"), target))
        return;
    const Doc copy = m_doc;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, target]() {
        const QString error = watcher->result();
        watcher->deleteLater();
        finishExport(error, QStringLiteral("PDF"), target);
    });
    m_exportFuture = QtConcurrent::run([copy, target]() {
        QString error;
        Exporter::writePdf(copy, target, &error);
        return error;
    });
    watcher->setFuture(m_exportFuture);
}

void Studio::exportGif(const QUrl &path, int width, int fps)
{
    const QString target = localExportPath(path);
    if (!beginExport(QStringLiteral("GIF"), target))
        return;
    const Doc copy = m_doc;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, target]() {
        const QString error = watcher->result();
        watcher->deleteLater();
        finishExport(error, QStringLiteral("GIF"), target);
    });
    m_exportFuture = QtConcurrent::run([copy, target, width, fps]() {
        QString error;
        Exporter::writeGif(copy, target, width, fps, &error);
        return error;
    });
    watcher->setFuture(m_exportFuture);
}
