#pragma once

#include "document.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QFuture>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class Studio;

// Layer list for the current page. Row 0 is the bottom-most element, which is
// also painting order; the panel renders it reversed so the top layer is on top.
class ElementListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        GlyphRole,
        VisibleRole,
        LockedRole,
        SelectedRole,
        TextRole
    };

    explicit ElementListModel(Studio *studio);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    void refresh();
    void refreshSelection();

private:
    Studio *m_studio;
};

class Studio : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(double docWidth READ docWidth NOTIFY docChanged)
    Q_PROPERTY(double docHeight READ docHeight NOTIFY docChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pagesChanged)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY pagesChanged)
    Q_PROPERTY(QString currentPageId READ currentPageId NOTIFY pagesChanged)
    Q_PROPERTY(QAbstractItemModel *elements READ elementModel CONSTANT)
    Q_PROPERTY(QVariantMap sel READ sel NOTIFY selectionChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString docId READ docId NOTIFY docChanged)
    Q_PROPERTY(QString pageBackground READ pageBackground WRITE setPageBackground NOTIFY contentsChanged)
    Q_PROPERTY(bool typing READ typing NOTIFY typingChanged)
    // Deleting a design moves it to docs/.trash instead of unlinking it; the
    // toast offers Undo while a delete is still undoable.
    Q_PROPERTY(bool trashPending READ trashPending NOTIFY trashChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)

public:
    explicit Studio(QObject *parent = nullptr);
    ~Studio() override;

    bool editing() const { return m_editing; }
    QString title() const { return m_doc.title; }
    // Q_INVOKABLE as well: QML calls this from the title field, and a WRITE
    // accessor alone is not callable from QML.
    Q_INVOKABLE void setTitle(const QString &t);
    double docWidth() const { return m_doc.width; }
    double docHeight() const { return m_doc.height; }
    int pageCount() const { return m_doc.pages.size(); }
    int currentPage() const { return m_pageIndex; }
    QString currentPageId() const { return page().id; }
    QAbstractItemModel *elementModel() { return &m_model; }
    QVariantMap sel() const;
    int selectionCount() const { return m_selection.size(); }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString status() const { return m_status; }
    QString docId() const { return m_doc.id; }
    QString pageBackground() const { return page().background; }
    bool typing() const { return m_typing; }
    bool trashPending() const { return !m_trashId.isEmpty(); }
    bool exporting() const { return m_exporting; }
    void setTyping(bool t);
    // Short transient message in the toast (page delete, save feedback, …).
    Q_INVOKABLE void note(const QString &text) { setStatus(text); }

    // Direct access for the canvas (it paints and mutates in document space).
    Doc &doc() { return m_doc; }
    const Doc &doc() const { return m_doc; }
    Page &page();
    const Page &page() const;
    QVector<Element> &elements();
    const QVector<Element> &elements() const;
    Element *elementById(const QString &id);
    bool isSelected(const QString &id) const { return m_selection.contains(id); }
    void pushUndo();
    // Gesture history: take a snapshot on press, commit it only if the gesture
    // actually changed something.
    void takeGestureSnapshot();
    void commitGestureSnapshot();
    void discardGestureSnapshot();
    // Consecutive nudges within 500 ms count as one undo step.
    void beginNudge();
    // Where the pointer last was, in document space, so inserted images land
    // under the cursor instead of the page centre.
    void setPointerPosition(const QPointF &docPt, bool inside);
    void touch();
    // Mid-gesture: repaint the canvas only. Layers, Inspector and the page
    // thumbnails refresh once, on release.
    void touchGeometry();
    void touchSelection();
    // Cheap accessors for the paint path: sel() builds a fresh QVariantMap.
    QString primaryId() const { return m_selection.value(0); }
    const Element *primaryElement() const;
    // Selection-only changes: no autosave, no thumbnail repaint.
    void touchSelectionOnly();

    Q_INVOKABLE void newDocument(double w, double h);
    Q_INVOKABLE void newFromTemplate(int index);
    Q_INVOKABLE bool openDoc(const QString &id);
    Q_INVOKABLE bool closeDoc();
    Q_INVOKABLE void discardChanges();
    // Returns false when the document could not be written; the reason is in
    // `status` so the toast can show it. Shows a "Saved" toast when `feedback`.
    Q_INVOKABLE bool saveNow();
    Q_INVOKABLE bool saveWithFeedback();
    // Saves only when there is unsaved work.
    Q_INVOKABLE bool saveOnQuit();
    bool prepareToLeave(bool quitting);
    Q_INVOKABLE QVariantList listDocs() const;
    Q_INVOKABLE QVariantList listTrash() const;
    Q_INVOKABLE void deleteDoc(const QString &id);
    Q_INVOKABLE bool restoreDoc(const QString &id);
    Q_INVOKABLE void undoDelete();
    Q_INVOKABLE void duplicateDoc(const QString &id);
    Q_INVOKABLE QVariantList presets() const;
    Q_INVOKABLE QVariantList templates() const;
    Q_INVOKABLE QString templateJson(int index) const;
    // A saved document's JSON, for the dashboard thumbnail.
    Q_INVOKABLE QString docJson(const QString &id) const;

    Q_INVOKABLE void selectElement(const QString &id, bool additive);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setSelProp(const QString &key, const QVariant &value);
    Q_INVOKABLE QString addElement(const QString &type);
    // Insert with its top-left at a document point (F11: under the pointer).
    Q_INVOKABLE QString addElementAt(const QString &type, const QPointF &docPos);
    Q_INVOKABLE QString addImage(const QUrl &source, double naturalW, double naturalH);
    // Swap the image file of an existing element, keeping its box (F14).
    Q_INVOKABLE bool replaceImage(const QString &id, const QUrl &source);
    // True when the selected elements disagree about `key` (F15: "Mixed").
    Q_INVOKABLE bool selMixed(const QString &key) const;
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void reorderElement(int from, int to);
    Q_INVOKABLE void raiseSelected();
    Q_INVOKABLE void lowerSelected();
    Q_INVOKABLE void frontSelected();
    Q_INVOKABLE void backSelected();
    Q_INVOKABLE void setElementVisible(const QString &id, bool visible);
    Q_INVOKABLE void setElementLocked(const QString &id, bool locked);
    Q_INVOKABLE void renameElement(const QString &id, const QString &name);
    Q_INVOKABLE void alignSelected(const QString &mode);
    Q_INVOKABLE void distributeSelected(const QString &axis);

    Q_INVOKABLE void addPage();
    Q_INVOKABLE void duplicatePage(int index);
    Q_INVOKABLE void deletePage(int index);
    Q_INVOKABLE void setCurrentPage(int index);
    Q_INVOKABLE void movePage(int from, int to);
    Q_INVOKABLE void setPageBackground(const QString &colour);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void clearStatus();

    // Exports render on a worker thread; the UI stays live and shows progress.
    Q_INVOKABLE void exportPng(const QUrl &path, int pageIndex, double scale);
    Q_INVOKABLE void exportPdf(const QUrl &path);
    Q_INVOKABLE void exportGif(const QUrl &path, int width, int fps);

    Q_INVOKABLE QString dataRoot() const;
    // "<title>.<ext>" for the export dialog's file name.
    Q_INVOKABLE QString suggestedExportName(const QString &extension) const;

signals:
    void editingChanged();
    void titleChanged();
    void docChanged();
    void pagesChanged();
    void selectionChanged();
    void historyChanged();
    void statusChanged();
    void contentsChanged();
    void typingChanged();
    void trashChanged();
    void exportingChanged();
    // A different document is open (new/template/open). The view fits itself to
    // the page on this, and only on this: undo/redo must keep zoom and pan.
    void documentReplaced();
    void geometryChanged();
    // Give the editor a chance to flush fields before leaving the document.
    void closingSoon();
    void leaveBlocked(bool quitting);

private:
    void setStatus(const QString &s);
    bool saveNowInternal(bool announce);
    void pushUndoSnapshot(const QJsonObject &before);
    void scheduleSave();
    void resetHistory();
    void restore(const QJsonObject &snapshot);
    QString docsDir() const;
    QString assetsDir() const;
    QString trashDir() const;
    void purgeTrash();
    bool beginExport(const QString &label, const QString &target);
    void finishExport(const QString &error, const QString &label, const QString &target);
    QString docPath(const QString &id) const;
    QString addElementInternal(const QString &type, const QPointF &docPos, bool atPoint);
    static bool writeDoc(const QString &path, const Doc &doc, QString *error);

    Doc m_doc;
    int m_pageIndex = 0;
    QStringList m_selection;
    bool m_editing = false;
    bool m_typing = false;
    QString m_status;

    ElementListModel m_model;
    QVector<QJsonObject> m_undo;
    QVector<QJsonObject> m_redo;
    QJsonObject m_pending; // snapshot taken before the first change of a gesture
    QPointF m_pointerPos;
    bool m_pointerInside = false;
    QTimer m_saveTimer;
    bool m_dirty = false;
    QElapsedTimer m_nudgeTimer;
    bool m_nudgeOpen = false;
    bool m_exporting = false;
    QFuture<QString> m_exportFuture;
    QString m_trashId;
};
