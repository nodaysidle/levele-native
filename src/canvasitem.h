#pragma once

#include "document.h"
#include "studio.h"

#include <QColor>
#include <QLineF>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QVector>

// The interactive design surface: paints the page with QPainter and owns all
// pointer/keyboard interaction in document space. Preview mode is used by the
// page strip and the template thumbnails.
class CanvasItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(Studio *document READ studio WRITE setStudio NOTIFY studioChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(qreal panX READ panX WRITE setPanX NOTIFY viewChanged)
    Q_PROPERTY(qreal panY READ panY WRITE setPanY NOTIFY viewChanged)
    Q_PROPERTY(bool grid READ grid WRITE setGrid NOTIFY gridChanged)
    Q_PROPERTY(bool userMoved READ userMoved NOTIFY userMovedChanged)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(int pageIndex READ pageIndex WRITE setPageIndex NOTIFY pageIndexChanged)
    Q_PROPERTY(bool preview READ preview WRITE setPreview NOTIFY previewChanged)
    Q_PROPERTY(QString previewJson READ previewJson WRITE setPreviewJson NOTIFY previewJsonChanged)
    Q_PROPERTY(bool interactive READ interactive WRITE setInteractive NOTIFY previewChanged)
    Q_PROPERTY(QString editingId READ editingId NOTIFY editingChanged)
    Q_PROPERTY(QRectF editRect READ editRect NOTIFY editingChanged)
    Q_PROPERTY(qreal editFontPx READ editFontPx NOTIFY editingChanged)
    Q_PROPERTY(QColor editColour READ editColour NOTIFY editingChanged)
    Q_PROPERTY(QString editText READ editText NOTIFY editingChanged)
    // Typography of the element being edited, so the inline editor matches the
    // painted text instead of snapping when it opens and closes.
    Q_PROPERTY(int editWeight READ editWeight NOTIFY editingChanged)
    Q_PROPERTY(QString editAlign READ editAlign NOTIFY editingChanged)
    Q_PROPERTY(QString editVAlign READ editVAlign NOTIFY editingChanged)
    Q_PROPERTY(qreal editSpacing READ editSpacing NOTIFY editingChanged)
    Q_PROPERTY(qreal editLineHeight READ editLineHeight NOTIFY editingChanged)
    Q_PROPERTY(QColor canvasBg MEMBER m_canvasBg NOTIFY themeColoursChanged)
    Q_PROPERTY(QColor borderColour MEMBER m_borderColour NOTIFY themeColoursChanged)
    Q_PROPERTY(QColor accentColour MEMBER m_accentColour NOTIFY themeColoursChanged)
    Q_PROPERTY(QColor mutedColour MEMBER m_mutedColour NOTIFY themeColoursChanged)

public:
    explicit CanvasItem(QQuickItem *parent = nullptr);

    void paint(QPainter *p) override;

    Studio *studio() const { return m_studio; }
    void setStudio(Studio *s);

    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z);
    qreal panX() const { return m_panX; }
    void setPanX(qreal v);
    qreal panY() const { return m_panY; }
    void setPanY(qreal v);

    bool grid() const { return m_grid; }
    void setGrid(bool on);
    bool userMoved() const { return m_userMoved; }

    QString tool() const { return m_tool; }
    void setTool(const QString &t);

    int pageIndex() const { return m_pageIndex; }
    void setPageIndex(int i);

    bool preview() const { return m_preview; }
    void setPreview(bool on);
    QString previewJson() const { return m_previewJson; }
    void setPreviewJson(const QString &json);
    bool interactive() const { return m_interactive; }
    void setInteractive(bool on);

    // Number of times this item has painted. Tests use it to prove a gesture
    // does not repaint every page thumbnail.
    Q_INVOKABLE int paintCount() const { return m_paintCount; }
    QString editingId() const { return m_editingId; }
    QRectF editRect() const { return m_editRect; }
    qreal editFontPx() const { return m_editFontPx; }
    QColor editColour() const { return m_editColour; }
    QString editText() const { return m_editText; }
    int editWeight() const { return m_editWeight; }
    QString editAlign() const { return m_editAlign; }
    QString editVAlign() const { return m_editVAlign; }
    qreal editSpacing() const { return m_editSpacing; }
    qreal editLineHeight() const { return m_editLineHeight; }

    // Zoom step for a wheel delta: high-resolution wheels and touchpads send
    // many small deltas, so the factor has to be proportional to the delta.
    static double zoomFactorForDelta(int angleDelta);
    Q_INVOKABLE void fitToView();
    Q_INVOKABLE void zoomBy(qreal factor);
    Q_INVOKABLE void zoomAt(qreal factor, qreal sx, qreal sy);
    Q_INVOKABLE void beginEditText(const QString &id);
    Q_INVOKABLE void commitEditText(const QString &text);
    Q_INVOKABLE void cancelEditText();
    // TextEdit has no line-height property, so the element's line height is
    // pushed into its QTextDocument's block format while editing.
    Q_INVOKABLE void applyEditLineHeight(QObject *textDocument);
    Q_INVOKABLE void nudge(qreal dx, qreal dy);
    Q_INVOKABLE void resetView();
    // Picking in document space: the id of the topmost element under `docPt`,
    // or empty. Lines and arrows are picked by distance to their segment.
    Q_INVOKABLE QString hitTest(const QPointF &docPt) const;

signals:
    // The image tool needs a file dialog, which lives in QML.
    void imageRequested();
    void studioChanged();
    void viewChanged();
    void gridChanged();
    void userMovedChanged();
    void toolChanged();
    void pageIndexChanged();
    void previewChanged();
    // Not `paletteChanged`: QQuickItem already has that signal and shadowing it
    // confused clazy and anyone reading the class.
    void themeColoursChanged();
    void editingChanged();
    void zoomLabelChanged();
    void previewJsonChanged();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void hoverMoveEvent(QHoverEvent *e) override;
    void hoverLeaveEvent(QHoverEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    enum class Handle {
        None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left, Rotate
    };
    enum class Mode { Idle, Moving, Resizing, Rotating, Creating, Marquee, Panning };

    QPointF toScreen(const QPointF &docPt) const;
    QPointF toDoc(const QPointF &screenPt) const;
    QRectF pageScreenRect() const;
    QRectF selectionScreenRect() const;
    Handle handleAt(const QPointF &screenPt) const;
    void applySnap(double &x, double &y, const Element &moving);
    void clearGuides();
    void updateEditRect();

    Studio *m_studio = nullptr;

    qreal m_zoom = 1.0;
    qreal m_panX = 0;
    qreal m_panY = 0;
    bool m_grid = false;
    bool m_userMoved = false;
    QString m_tool = QStringLiteral("select");
    int m_pageIndex = -1;
    int m_paintCount = 0;
    bool m_preview = false;
    bool m_interactive = true;

    QColor m_canvasBg = QColor(0x0c, 0x0d, 0x0b);
    QColor m_borderColour = QColor(0x2b, 0x2a, 0x28);
    QColor m_accentColour = QColor(0xf5, 0x45, 0xc9);
    QColor m_mutedColour = QColor(0x9a, 0x97, 0x91);

    QString m_editingId;
    QRectF m_editRect;
    qreal m_editFontPx = 16;
    QColor m_editColour = Qt::white;
    QString m_editText;
    int m_editWeight = 600;
    QString m_editAlign = QStringLiteral("left");
    QString m_editVAlign = QStringLiteral("top");
    qreal m_editSpacing = 0;
    qreal m_editLineHeight = 1.25;

    QString m_previewJson;
    Doc m_previewDoc;
    bool m_hasPreviewDoc = false;

    Mode m_mode = Mode::Idle;
    Handle m_handle = Handle::None;
    Handle m_hoverHandle = Handle::None;
    QPointF m_pressScreen;
    QPointF m_lastDoc;
    QRectF m_startRect;
    QLineF m_startLine;
    double m_startRotation = 0;
    QString m_creatingId;
    bool m_spaceDown = false;
    bool m_moved = false;
    QVector<QLineF> m_guides;
    QRectF m_marquee;
    QHash<QString, QRectF> m_dragStart;
};
