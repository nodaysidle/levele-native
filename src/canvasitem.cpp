#include "canvasitem.h"

#include "exporter.h"
#include "studio.h"

#include <utility>
#include <QCursor>
#include <QFocusEvent>
#include <QHoverEvent>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QQuickTextDocument>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QPolygonF>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>

namespace {

constexpr qreal kHandleSize = 9.0;
constexpr qreal kRotateOffset = 26.0;
constexpr qreal kSnapPx = 7.0;

QPointF rotateAround(const QPointF &pt, const QPointF &centre, double degrees)
{
    if (qAbs(degrees) < 0.0001)
        return pt;
    const double rad = qDegreesToRadians(degrees);
    const double cs = std::cos(rad);
    const double sn = std::sin(rad);
    const QPointF d = pt - centre;
    return QPointF(centre.x() + d.x() * cs - d.y() * sn, centre.y() + d.x() * sn + d.y() * cs);
}

} // namespace

CanvasItem::CanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setFlag(QQuickItem::ItemIsFocusScope, true);
    setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    setOpaquePainting(true);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, false);
}

void CanvasItem::setStudio(Studio *s)
{
    if (m_studio == s)
        return;
    if (m_studio)
        m_studio->disconnect(this);
    m_studio = s;
    if (m_studio) {
        connect(m_studio, &Studio::contentsChanged, this, [this]() {
            updateEditRect();
            update();
        });
        connect(m_studio, &Studio::selectionChanged, this, [this]() {
            updateEditRect();
            update();
        });
        connect(m_studio, &Studio::geometryChanged, this, [this]() { update(); });
        connect(m_studio, &Studio::pagesChanged, this, [this]() {
            if (!m_editingId.isEmpty()) {
                m_editingId.clear();
                m_editRect = QRectF();
                emit editingChanged();
            }
            update();
        });
        connect(m_studio, &Studio::docChanged, this, [this]() {
            if (!m_editingId.isEmpty()) {
                m_editingId.clear();
                m_editRect = QRectF();
                emit editingChanged();
            }
            update();
        });
    }
    emit studioChanged();
    emit editingChanged();
    update();
}

void CanvasItem::setZoom(qreal z)
{
    const qreal clamped = qBound(0.05, z, 4.0);
    if (qFuzzyCompare(clamped, m_zoom))
        return;
    m_zoom = clamped;
    updateEditRect();
    emit viewChanged();
    update();
}

void CanvasItem::setPanX(qreal v)
{
    if (qFuzzyCompare(v, m_panX))
        return;
    m_panX = v;
    updateEditRect();
    emit viewChanged();
    update();
}

void CanvasItem::setPanY(qreal v)
{
    if (qFuzzyCompare(v, m_panY))
        return;
    m_panY = v;
    updateEditRect();
    emit viewChanged();
    update();
}

void CanvasItem::setGrid(bool on)
{
    if (m_grid == on)
        return;
    m_grid = on;
    emit gridChanged();
    update();
}

void CanvasItem::setTool(const QString &t)
{
    if (m_tool == t)
        return;
    m_tool = t;
    emit toolChanged();
    update();
}

void CanvasItem::setPageIndex(int i)
{
    if (m_pageIndex == i)
        return;
    m_pageIndex = i;
    emit pageIndexChanged();
    update();
}

void CanvasItem::setPreview(bool on)
{
    if (m_preview == on)
        return;
    m_preview = on;
    emit previewChanged();
    update();
}

void CanvasItem::setPreviewJson(const QString &json)
{
    if (m_previewJson == json)
        return;
    m_previewJson = json;
    m_previewDoc = Doc();
    m_hasPreviewDoc = false;
    if (!json.isEmpty()) {
        const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
        if (!o.isEmpty()) {
            m_previewDoc = Doc::fromJson(o);
            m_hasPreviewDoc = true;
        }
    }
    emit previewJsonChanged();
    update();
}

void CanvasItem::setInteractive(bool on)
{
    if (m_interactive == on)
        return;
    m_interactive = on;
    emit previewChanged();
}

// --------------------------------------------------------------- mapping

QPointF CanvasItem::toScreen(const QPointF &docPt) const
{
    return QPointF(m_panX + docPt.x() * m_zoom, m_panY + docPt.y() * m_zoom);
}

QPointF CanvasItem::toDoc(const QPointF &screenPt) const
{
    return QPointF((screenPt.x() - m_panX) / m_zoom, (screenPt.y() - m_panY) / m_zoom);
}

QRectF CanvasItem::pageScreenRect() const
{
    if (!m_studio)
        return QRectF();
    const Doc &d = m_studio->doc();
    return QRectF(m_panX, m_panY, d.width * m_zoom, d.height * m_zoom);
}

namespace {
int paintIndex(Studio *s, int requested)
{
    if (!s)
        return 0;
    if (requested >= 0 && requested < s->pageCount())
        return requested;
    return s->currentPage();
}
} // namespace
QRectF CanvasItem::selectionScreenRect() const
{
    if (!m_studio || m_studio->selectionCount() == 0)
        return QRectF();
    const QString first = m_studio->primaryId();
    for (const Element &e : std::as_const(m_studio->elements())) {
        if (e.id != first)
            continue;
        const QRectF r = e.rect();
        const QPointF tl = rotateAround(r.topLeft(), r.center(), e.rotation);
        const QPointF br = rotateAround(r.bottomRight(), r.center(), e.rotation);
        return QRectF(toScreen(QPointF(qMin(tl.x(), br.x()), qMin(tl.y(), br.y()))),
                      toScreen(QPointF(qMax(tl.x(), br.x()), qMax(tl.y(), br.y()))));
    }
    return QRectF();
}

// --------------------------------------------------------------- painting

void CanvasItem::paint(QPainter *p)
{
    ++m_paintCount;
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    p->fillRect(boundingRect(), m_canvasBg);
    if (!m_studio && !m_hasPreviewDoc)
        return;

    const Doc *docPtr = m_hasPreviewDoc ? &m_previewDoc : (m_studio ? &m_studio->doc() : nullptr);
    if (!docPtr)
        return;
    const Doc &doc = *docPtr;
    if (doc.pages.isEmpty())
        return;
    const int idx = m_hasPreviewDoc ? 0 : paintIndex(m_studio, m_pageIndex);

    QRectF target;
    if (m_preview) {
        const QRectF box = boundingRect().adjusted(0, 0, 0, 0);
        const double s = qMin(box.width() / qMax(1.0, doc.width), box.height() / qMax(1.0, doc.height));
        const double w = doc.width * s;
        const double h = doc.height * s;
        target = QRectF(box.center().x() - w / 2.0, box.center().y() - h / 2.0, w, h);
    } else {
        target = pageScreenRect();
    }

    if (!m_preview) {
        p->fillRect(target.adjusted(2, 4, 2, 8), QColor(0, 0, 0, 55));
        p->fillRect(target.adjusted(1, 2, 1, 4), QColor(0, 0, 0, 40));
    }

    Exporter::paintPage(*p, doc, idx, target);

    if (!m_preview) {
        p->setPen(QPen(m_borderColour, 1));
        p->setBrush(Qt::NoBrush);
        p->drawRect(target.adjusted(-0.5, -0.5, 0.5, 0.5));

        if (m_grid && m_zoom > 0.2) {
            QColor grid = m_accentColour;
            grid.setAlpha(m_zoom > 0.6 ? 26 : 16);
            p->setPen(QPen(grid, 1));
            const double step = 50.0 * m_zoom;
            for (double x = target.left() + step; x < target.right(); x += step)
                p->drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
            for (double y = target.top() + step; y < target.bottom(); y += step)
                p->drawLine(QPointF(target.left(), y), QPointF(target.right(), y));
        }
    }

    if (m_preview || m_hasPreviewDoc || !m_studio || !m_studio->selectionCount())
        return;

    // selection outlines
    p->setBrush(Qt::NoBrush);
    const QVector<Element> &els = m_studio->elements();
    for (const Element &e : std::as_const(els)) {
        if (!m_studio->isSelected(e.id))
            continue;
        const QRectF r = e.rect();
        QPolygonF poly;
        poly << rotateAround(r.topLeft(), r.center(), e.rotation)
             << rotateAround(r.topRight(), r.center(), e.rotation)
             << rotateAround(r.bottomRight(), r.center(), e.rotation)
             << rotateAround(r.bottomLeft(), r.center(), e.rotation);
        for (QPointF &pt : poly)
            pt = toScreen(pt);
        QColor line = m_accentColour;
        line.setAlpha(m_studio->primaryId() == e.id ? 255 : 140);
        p->setPen(QPen(line, 1.4));
        p->drawPolygon(poly);
    }

    // transform handles on the primary selection
    const Element primary = [&]() {
        Element out;
        const QString id = m_studio->primaryId();
        for (const Element &e : std::as_const(els)) {
            if (e.id == id)
                return e;
        }
        return out;
    }();

    if (!primary.id.isEmpty() && !primary.locked) {
        const QRectF r = primary.rect();
        const QPointF c = r.center();
        QVector<QPointF> pts;
        pts << r.topLeft() << QPointF(c.x(), r.top()) << r.topRight()
            << QPointF(r.right(), c.y()) << r.bottomRight() << QPointF(c.x(), r.bottom())
            << r.bottomLeft() << QPointF(r.left(), c.y());

        QPointF rotPt = QPointF(c.x(), r.top() - kRotateOffset / m_zoom);
        const QPointF rotScreen = toScreen(rotateAround(rotPt, c, primary.rotation));
        const QPointF topScreen = toScreen(rotateAround(QPointF(c.x(), r.top()), c, primary.rotation));

        p->setPen(QPen(m_accentColour, 1.2));
        p->drawLine(topScreen, rotScreen);

        for (QPointF &pt : pts)
            pt = toScreen(rotateAround(pt, c, primary.rotation));

        p->setPen(QPen(m_accentColour.darker(140), 1));
        for (const QPointF &pt : std::as_const(pts)) {
            const QRectF box(pt.x() - kHandleSize / 2, pt.y() - kHandleSize / 2, kHandleSize, kHandleSize);
            p->setBrush(m_canvasBg);
            p->drawRect(box);
            p->setBrush(Qt::NoBrush);
            p->drawRect(box);
            p->setBrush(m_canvasBg);
        }

        p->setBrush(m_accentColour);
        p->drawEllipse(rotScreen, 5, 5);
        p->setBrush(Qt::NoBrush);
    }

    // snap guides
    if (!m_guides.isEmpty()) {
        QColor guide = m_accentColour;
        guide.setAlpha(200);
        p->setPen(QPen(guide, 1.0, Qt::DashLine));
        for (const QLineF &l : std::as_const(m_guides)) {
            p->drawLine(toScreen(l.p1()), toScreen(l.p2()));
        }
    }

    if (m_mode == Mode::Marquee && !m_marquee.isEmpty()) {
        p->setPen(QPen(m_accentColour, 1));
        QColor fill = m_accentColour;
        fill.setAlpha(28);
        p->setBrush(fill);
        p->drawRect(m_marquee);
        p->setBrush(Qt::NoBrush);
    }
}

// -------------------------------------------------------------- hit testing

QString CanvasItem::hitTest(const QPointF &docPt) const
{
    if (!m_studio)
        return {};
    const QVector<Element> &els = m_studio->elements();
    for (int i = els.size() - 1; i >= 0; --i) {
        const Element &e = els.at(i);
        // Six screen pixels of slack, in document units.
        if (e.containsPoint(docPt, 6.0 / m_zoom))
            return e.id;
    }
    return {};
}

CanvasItem::Handle CanvasItem::handleAt(const QPointF &screenPt) const
{
    if (!m_studio || m_studio->selectionCount() == 0)
        return Handle::None;
    const QString id = m_studio->primaryId();
    const Element *found = nullptr;
    for (const Element &e : std::as_const(m_studio->elements())) {
        if (e.id == id) {
            found = &e;
            break;
        }
    }
    if (!found || found->locked)
        return Handle::None;

    const QRectF r = found->rect();
    const QPointF c = r.center();
    QVector<QPointF> pts;
    pts << r.topLeft() << QPointF(c.x(), r.top()) << r.topRight()
        << QPointF(r.right(), c.y()) << r.bottomRight() << QPointF(c.x(), r.bottom())
        << r.bottomLeft() << QPointF(r.left(), c.y());
    for (QPointF &pt : pts)
        pt = toScreen(rotateAround(pt, c, found->rotation));

    const QPointF rotPt = toScreen(rotateAround(QPointF(c.x(), r.top() - kRotateOffset / m_zoom), c,
                                                found->rotation));

    if (QLineF(screenPt, rotPt).length() <= 9)
        return Handle::Rotate;

    static const Handle order[8] = {Handle::TopLeft, Handle::Top,    Handle::TopRight, Handle::Right,
                                    Handle::BottomRight, Handle::Bottom, Handle::BottomLeft, Handle::Left};
    for (int i = 0; i < 8; ++i) {
        if (QLineF(screenPt, pts.at(i)).length() <= kHandleSize)
            return order[i];
    }
    return Handle::None;
}

void CanvasItem::clearGuides()
{
    if (m_guides.isEmpty())
        return;
    m_guides.clear();
    update();
}

void CanvasItem::applySnap(double &x, double &y, const Element &moving)
{
    m_guides.clear();
    if (!m_studio)
        return;
    const Doc &d = m_studio->doc();
    const double tol = kSnapPx / m_zoom;

    const QRectF mr = moving.rect();
    const double w = mr.width();
    const double h = mr.height();

    QVector<double> vx{0.0, d.width / 2.0, d.width};
    QVector<double> vy{0.0, d.height / 2.0, d.height};

    for (const Element &e : std::as_const(m_studio->elements())) {
        if (m_studio->isSelected(e.id) || !e.visible)
            continue;
        const QRectF er = e.rect();
        vx << er.left() << er.center().x() << er.right();
        vy << er.top() << er.center().y() << er.bottom();
    }

    double bestDx = 1e9;
    double snappedX = 0;
    bool gotX = false;
    for (double cand : std::as_const(vx)) {
        for (double edge : {x, x + w / 2.0, x + w}) {
            const double delta = cand - edge;
            if (qAbs(delta) <= tol && qAbs(delta) < qAbs(bestDx)) {
                bestDx = delta;
                snappedX = cand;
                gotX = true;
            }
        }
    }
    if (gotX) {
        x += bestDx;
        m_guides.append(QLineF(snappedX, -1e5, snappedX, 1e5));
    }

    double bestDy = 1e9;
    double snappedY = 0;
    bool gotY = false;
    for (double cand : std::as_const(vy)) {
        for (double edge : {y, y + h / 2.0, y + h}) {
            const double delta = cand - edge;
            if (qAbs(delta) <= tol && qAbs(delta) < qAbs(bestDy)) {
                bestDy = delta;
                snappedY = cand;
                gotY = true;
            }
        }
    }
    if (gotY) {
        y += bestDy;
        m_guides.append(QLineF(-1e5, snappedY, 1e5, snappedY));
    }
}

// ---------------------------------------------------------------- pointing

void CanvasItem::mousePressEvent(QMouseEvent *e)
{
    if (!m_studio || m_preview || !m_interactive) {
        e->ignore();
        return;
    }
    forceActiveFocus();
    const QPointF sp = e->position();
    const QPointF dp = toDoc(sp);
    m_pressScreen = sp;
    m_lastDoc = dp;
    m_moved = false;
    if (!m_editingId.isEmpty()) {
        m_editingId.clear();
        emit editingChanged();
    }
    m_studio->setPointerPosition(dp, true);

    if (e->button() == Qt::MiddleButton || m_spaceDown) {
        m_mode = Mode::Panning;
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }

    if (e->button() != Qt::LeftButton) {
        e->accept();
        return;
    }

    // transform handles win over hit-testing so grabbing a corner works even
    // when another element sits underneath.
    const Handle h = handleAt(sp);
    if (h != Handle::None) {
        m_handle = h;
        m_mode = (h == Handle::Rotate) ? Mode::Rotating : Mode::Resizing;
        const Element *primary = nullptr;
        const QString id = m_studio->primaryId();
        for (const Element &el : std::as_const(m_studio->elements())) {
            if (el.id == id) {
                primary = &el;
                break;
            }
        }
        if (primary) {
            m_startRect = primary->rect();
            m_startLine = primary->segment();
            m_startRotation = primary->rotation;
        }
        m_studio->takeGestureSnapshot();
        e->accept();
        return;
    }

    const QString tool = m_tool;
    if (tool != QLatin1String("select")) {
        if (tool == QLatin1String("text")) {
            const QString id = m_studio->addElementAt(QStringLiteral("text"), dp);
            setTool(QStringLiteral("select"));
            beginEditText(id);
            e->accept();
            return;
        }
        const QString type = tool;
        const QString id = m_studio->addElement(type);
        if (Element *el = m_studio->elementById(id)) {
            el->x = dp.x();
            el->y = dp.y();
            el->w = 0;
            el->h = 0;
        }
        m_creatingId = id;
        m_mode = Mode::Creating;
        setTool(QStringLiteral("select"));
        m_studio->touch();
        e->accept();
        return;
    }

    const QString hit = hitTest(dp);
    if (hit.isEmpty()) {
        const bool additive = e->modifiers().testFlag(Qt::ShiftModifier);
        if (!additive)
            m_studio->clearSelection();
        m_mode = Mode::Marquee;
        m_marquee = QRectF(sp, sp);
        e->accept();
        return;
    }

    const bool additive = e->modifiers().testFlag(Qt::ShiftModifier);
    if (additive || !m_studio->isSelected(hit))
        m_studio->selectElement(hit, additive);

    m_dragStart.clear();
    for (const Element &el : std::as_const(m_studio->elements())) {
        if (m_studio->isSelected(el.id) && !el.locked)
            m_dragStart.insert(el.id, el.rect());
    }
    if (!m_dragStart.isEmpty()) {
        m_mode = Mode::Moving;
        m_studio->takeGestureSnapshot();
    }
    e->accept();
}

void CanvasItem::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_studio || m_preview || m_mode == Mode::Idle) {
        e->ignore();
        return;
    }
    const QPointF sp = e->position();
    const QPointF dp = toDoc(sp);

    if (m_mode == Mode::Panning) {
        if (!m_userMoved) {
            m_userMoved = true;
            emit userMovedChanged();
        }
        m_panX += sp.x() - m_pressScreen.x();
        m_panY += sp.y() - m_pressScreen.y();
        m_pressScreen = sp;
        emit viewChanged();
        update();
        e->accept();
        return;
    }

    if (QLineF(sp, m_pressScreen).length() > 2)
        m_moved = true;

    if (m_mode == Mode::Moving) {
        const double dx = dp.x() - m_lastDoc.x();
        const double dy = dp.y() - m_lastDoc.y();
        m_lastDoc = dp;

        QString primaryId = m_studio->primaryId();
        Element *primary = m_studio->elementById(primaryId);
        const Element primaryCopy = primary ? *primary : Element();

        for (Element &el : m_studio->elements()) {
            if (!m_studio->isSelected(el.id) || el.locked)
                continue;
            el.x += dx;
            el.y += dy;
        }

        if (primary && !primary->locked) {
            const QRectF pr = primary->rect();
            double nx = pr.left();
            double ny = pr.top();
            applySnap(nx, ny, primaryCopy);
            const double ax = nx - pr.left();
            const double ay = ny - pr.top();
            if (ax != 0 || ay != 0) {
                for (Element &el : m_studio->elements()) {
                    if (!m_studio->isSelected(el.id) || el.locked)
                        continue;
                    el.x += ax;
                    el.y += ay;
                }
            }
        }
        m_studio->touchGeometry();
        e->accept();
        return;
    }

    if (m_mode == Mode::Creating) {
        if (Element *el = m_studio->elementById(m_creatingId)) {
            const double x0 = el->x;
            const double y0 = el->y;
            double w = dp.x() - x0;
            double h = dp.y() - y0;
            if (e->modifiers().testFlag(Qt::ShiftModifier)) {
                if (el->isLineLike()) {
                    // Lines snap to 45° steps, keeping the drawn length.
                    const double len = std::hypot(w, h);
                    const double step = qDegreesToRadians(45.0);
                    const double angle = qRound(std::atan2(h, w) / step) * step;
                    w = len * std::cos(angle);
                    h = len * std::sin(angle);
                } else {
                    const double s = qMax(qAbs(w), qAbs(h));
                    w = w < 0 ? -s : s;
                    h = h < 0 ? -s : s;
                }
            }
            el->w = w;
            el->h = h;
        }
        m_studio->touchGeometry();
        e->accept();
        return;
    }

    if (m_mode == Mode::Resizing) {
        const QString id = m_studio->primaryId();
        Element *el = m_studio->elementById(id);
        if (!el) {
            e->accept();
            return;
        }
        const double rot = el->rotation;
        const QPointF c0 = m_startRect.center();
        const QPointF local = rotateAround(dp, c0, -rot);

        QRectF r = m_startRect;
        switch (m_handle) {
        case Handle::TopLeft: r.setTopLeft(local); break;
        case Handle::Top: r.setTop(local.y()); break;
        case Handle::TopRight: r.setTopRight(local); break;
        case Handle::Right: r.setRight(local.x()); break;
        case Handle::BottomRight: r.setBottomRight(local); break;
        case Handle::Bottom: r.setBottom(local.y()); break;
        case Handle::BottomLeft: r.setBottomLeft(local); break;
        case Handle::Left: r.setLeft(local.x()); break;
        default: break;
        }

        if (el->isLineLike()) {
            const bool touchesLeft = m_handle == Handle::Left || m_handle == Handle::TopLeft
                || m_handle == Handle::BottomLeft;
            const bool touchesRight = m_handle == Handle::Right || m_handle == Handle::TopRight
                || m_handle == Handle::BottomRight;
            const bool touchesTop = m_handle == Handle::Top || m_handle == Handle::TopLeft
                || m_handle == Handle::TopRight;
            const bool touchesBottom = m_handle == Handle::Bottom || m_handle == Handle::BottomLeft
                || m_handle == Handle::BottomRight;

            if (e->modifiers().testFlag(Qt::ShiftModifier)
                && m_startRect.width() > 0.5 && m_startRect.height() > 0.5) {
                const double aspect = m_startRect.width() / m_startRect.height();
                if (touchesLeft)
                    r.setLeft(r.right() - r.height() * aspect);
                else if (touchesRight)
                    r.setRight(r.left() + r.height() * aspect);
            }

            double x1 = m_startLine.p1().x();
            double x2 = m_startLine.p2().x();
            if (m_startRect.width() > 1e-6) {
                const double t1x = (x1 - m_startRect.left()) / m_startRect.width();
                const double t2x = (x2 - m_startRect.left()) / m_startRect.width();
                x1 = r.left() + t1x * r.width();
                x2 = r.left() + t2x * r.width();
            } else if (touchesLeft || touchesRight) {
                x2 = local.x();
            }

            double y1 = m_startLine.p1().y();
            double y2 = m_startLine.p2().y();
            if (m_startRect.height() > 1e-6) {
                const double t1y = (y1 - m_startRect.top()) / m_startRect.height();
                const double t2y = (y2 - m_startRect.top()) / m_startRect.height();
                y1 = r.top() + t1y * r.height();
                y2 = r.top() + t2y * r.height();
            } else if (touchesTop || touchesBottom) {
                y2 = local.y();
            }

            if (std::hypot(x2 - x1, y2 - y1) < 2.0) {
                if (qAbs(x2 - x1) >= qAbs(y2 - y1))
                    x2 = x1 + (x2 >= x1 ? 2.0 : -2.0);
                else
                    y2 = y1 + (y2 >= y1 ? 2.0 : -2.0);
            }

            const QPointF c1((x1 + x2) / 2.0, (y1 + y2) / 2.0);
            const QPointF centreDoc = rotateAround(c1, c0, rot);
            el->w = x2 - x1;
            el->h = y2 - y1;
            el->x = centreDoc.x() - el->w / 2.0;
            el->y = centreDoc.y() - el->h / 2.0;
            m_studio->touchGeometry();
            e->accept();
            return;
        }

        QRectF norm = r.normalized();
        if (e->modifiers().testFlag(Qt::ShiftModifier) && m_startRect.height() > 0.5) {
            const double aspect = m_startRect.width() / m_startRect.height();
            const double h2 = norm.height();
            const double w2 = h2 * aspect;
            if (m_handle == Handle::Left || m_handle == Handle::TopLeft || m_handle == Handle::BottomLeft)
                norm.setLeft(norm.right() - w2);
            else
                norm.setWidth(w2);
        }
        if (norm.width() < 2)
            norm.setWidth(2);
        if (norm.height() < 2)
            norm.setHeight(2);

        // Re-anchor in the element's own rotated frame: the opposite edge must
        // not drift while the dragged edge follows the pointer.
        const QPointF c1 = norm.center();
        const QPointF centreDoc = rotateAround(c1, c0, rot);
        el->w = norm.width();
        el->h = norm.height();
        el->x = centreDoc.x() - norm.width() / 2.0;
        el->y = centreDoc.y() - norm.height() / 2.0;
        m_studio->touchGeometry();
        e->accept();
        return;
    }

    if (m_mode == Mode::Rotating) {
        const QString id = m_studio->primaryId();
        Element *el = m_studio->elementById(id);
        if (!el) {
            e->accept();
            return;
        }
        const QPointF c = m_startRect.center();
        const QPointF cs = toScreen(c);
        const double a0 = qRadiansToDegrees(std::atan2(m_pressScreen.y() - cs.y(),
                                                       m_pressScreen.x() - cs.x()));
        const double a1 = qRadiansToDegrees(std::atan2(sp.y() - cs.y(), sp.x() - cs.x()));
        double delta = a1 - a0;
        while (delta > 180.0)
            delta -= 360.0;
        while (delta < -180.0)
            delta += 360.0;
        double angle = m_startRotation + delta;
        if (e->modifiers().testFlag(Qt::ShiftModifier))
            angle = qRound(angle / 15.0) * 15.0;
        el->rotation = angle;
        m_studio->touchGeometry();
        e->accept();
        return;
    }

    if (m_mode == Mode::Marquee) {
        m_marquee = QRectF(m_pressScreen, sp).normalized();
        QRectF docRect(toDoc(m_marquee.topLeft()), toDoc(m_marquee.bottomRight()));
        QStringList ids;
        for (const Element &el : std::as_const(m_studio->elements())) {
            if (el.intersectsRect(docRect, 6.0 / m_zoom))
                ids.append(el.id);
        }
        // Replace, not accumulate: marquee is a fresh selection.
        m_studio->clearSelection();
        for (const QString &id : ids)
            m_studio->selectElement(id, true);
        update();
        e->accept();
        return;
    }
}

void CanvasItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_mode == Mode::Creating) {
        if (Element *el = m_studio->elementById(m_creatingId)) {
            if (el->isLineLike()) {
                // Keep the vector exactly as drawn, including a short one: the
                // old code replaced a horizontal drag with a default line and
                // turned a vertical one into a 200x200 box. Only a click with
                // no drag at all falls back to a default length.
                if (std::hypot(el->w, el->h) < 6) {
                    el->w = 240;
                    el->h = 0;
                    el->x -= el->w / 2.0;
                }
            } else if (qAbs(el->w) < 6 || qAbs(el->h) < 6) {
                const double w = 200.0;
                const double h = 200.0;
                el->x = el->x - w / 2.0;
                el->y = el->y - h / 2.0;
                el->w = w;
                el->h = h;
            } else {
                QRectF norm = el->rect();
                el->x = norm.x();
                el->y = norm.y();
                el->w = norm.width();
                el->h = norm.height();
            }
        }
        m_creatingId.clear();
        m_studio->touch();
        emit m_studio->selectionChanged();
    }

    if (m_mode == Mode::Panning)
        unsetCursor();

    if (m_mode == Mode::Marquee) {
        m_marquee = QRectF();
        update();
    }

    if (m_mode != Mode::Idle) {
        clearGuides();
        const bool wasGesture = m_mode == Mode::Moving || m_mode == Mode::Resizing
            || m_mode == Mode::Rotating;
        m_mode = Mode::Idle;
        m_handle = Handle::None;
        if (wasGesture) {
            // A click that never moved must not consume an undo step or dirty the doc.
            if (m_moved) {
                m_studio->commitGestureSnapshot();
                m_studio->touchSelection();
            } else {
                m_studio->discardGestureSnapshot();
            }
        }
    }
    e->accept();
}

void CanvasItem::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (!m_studio || m_preview || !m_interactive) {
        e->ignore();
        return;
    }
    const QString hit = hitTest(toDoc(e->position()));
    if (hit.isEmpty()) {
        e->accept();
        return;
    }
    const Element *el = m_studio->elementById(hit);
    if (el && el->isText()) {
        m_studio->selectElement(hit, false);
        beginEditText(hit);
    }
    e->accept();
}

double CanvasItem::zoomFactorForDelta(int angleDelta)
{
    // A notch is 120 units: 1.0015^120 = 1.197, so a mouse notch still zooms
    // about 20 % while a touchpad's 15-unit steps only move about 2 %.
    if (angleDelta == 0)
        return 1.0;
    return std::pow(1.0015, double(qBound(-1200, angleDelta, 1200)));
}

void CanvasItem::wheelEvent(QWheelEvent *e)
{
    if (!m_studio || m_preview) {
        e->ignore();
        return;
    }
    const QPointF sp = e->position();
    const int dy = e->angleDelta().y();
    if (e->modifiers().testFlag(Qt::ControlModifier) || e->modifiers().testFlag(Qt::MetaModifier)) {
        zoomAt(zoomFactorForDelta(dy), sp.x(), sp.y());
    } else {
        if (!m_userMoved) {
            m_userMoved = true;
            emit userMovedChanged();
        }
        // Touchpads report pixel deltas; mice report angle deltas.
        const QPoint pixel = e->pixelDelta();
        if (!pixel.isNull()) {
            m_panX += pixel.x();
            m_panY += pixel.y();
        } else {
            m_panX += e->angleDelta().x() * 0.6;
            m_panY += dy * 0.6;
        }
        emit viewChanged();
        update();
    }
    e->accept();
}

void CanvasItem::hoverLeaveEvent(QHoverEvent *e)
{
    if (m_studio)
        m_studio->setPointerPosition(QPointF(), false);
    QQuickItem::hoverLeaveEvent(e);
}

void CanvasItem::hoverMoveEvent(QHoverEvent *e)
{
    if (!m_studio || m_preview || !m_interactive)
        return;
    const QPointF sp = e->position();
    m_studio->setPointerPosition(toDoc(sp), true);
    const Handle h = handleAt(sp);
    if (h == m_hoverHandle)
        return;
    m_hoverHandle = h;
    switch (h) {
    case Handle::TopLeft:
    case Handle::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case Handle::TopRight:
    case Handle::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Handle::Top:
    case Handle::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Handle::Left:
    case Handle::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case Handle::Rotate:
        setCursor(Qt::CrossCursor);
        break;
    case Handle::None:
        setCursor(hitTest(toDoc(sp)).isEmpty() ? Qt::ArrowCursor : Qt::SizeAllCursor);
        break;
    }
}

void CanvasItem::keyReleaseEvent(QKeyEvent *e)
{
    // Space is a held modifier, not a toggle: without this the canvas kept
    // panning on the next click until a mouse release cleared the flag.
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = false;
        e->accept();
        return;
    }
    QQuickItem::keyReleaseEvent(e);
}

void CanvasItem::focusOutEvent(QFocusEvent *e)
{
    m_spaceDown = false;
    QQuickItem::focusOutEvent(e);
}

void CanvasItem::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space) {
        m_spaceDown = true;
        e->accept();
        return;
    }
    if (!m_studio) {
        e->ignore();
        return;
    }

    const bool noCmdMod = !e->modifiers().testFlag(Qt::ControlModifier)
        && !e->modifiers().testFlag(Qt::MetaModifier)
        && !e->modifiers().testFlag(Qt::AltModifier);
    if (noCmdMod) {
        switch (e->key()) {
        case Qt::Key_V: setTool(QStringLiteral("select")); e->accept(); return;
        case Qt::Key_R: setTool(QStringLiteral("rect")); e->accept(); return;
        case Qt::Key_U: setTool(QStringLiteral("roundRect")); e->accept(); return;
        case Qt::Key_O: setTool(QStringLiteral("ellipse")); e->accept(); return;
        case Qt::Key_Y: setTool(QStringLiteral("triangle")); e->accept(); return;
        case Qt::Key_P: setTool(QStringLiteral("star")); e->accept(); return;
        case Qt::Key_L: setTool(QStringLiteral("line")); e->accept(); return;
        case Qt::Key_A: setTool(QStringLiteral("arrow")); e->accept(); return;
        case Qt::Key_T: setTool(QStringLiteral("text")); e->accept(); return;
        case Qt::Key_I: emit imageRequested(); e->accept(); return;
        case Qt::Key_G:
            m_grid = !m_grid;
            emit gridChanged();
            update();
            e->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_studio->selectionCount() == 1) {
                if (const Element *el = m_studio->primaryElement(); el && el->isText() && !el->locked) {
                    beginEditText(el->id);
                    e->accept();
                    return;
                }
            }
            break;
        default:
            break;
        }
    }

    if (noCmdMod) {
        const double step = e->modifiers().testFlag(Qt::ShiftModifier) ? 10.0 : 1.0;
        switch (e->key()) {
        case Qt::Key_Left:
            nudge(-step, 0);
            e->accept();
            return;
        case Qt::Key_Right:
            nudge(step, 0);
            e->accept();
            return;
        case Qt::Key_Up:
            nudge(0, -step);
            e->accept();
            return;
        case Qt::Key_Down:
            nudge(0, step);
            e->accept();
            return;
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            m_studio->deleteSelected();
            e->accept();
            return;
        case Qt::Key_Escape:
            m_studio->clearSelection();
            e->accept();
            return;
        default:
            break;
        }
    }
    e->ignore();
}

void CanvasItem::nudge(qreal dx, qreal dy)
{
    if (!m_studio || m_studio->selectionCount() == 0)
        return;
    bool anyUnlocked = false;
    for (const Element &el : std::as_const(m_studio->elements())) {
        if (m_studio->isSelected(el.id) && !el.locked) {
            anyUnlocked = true;
            break;
        }
    }
    if (!anyUnlocked)
        return;
    // One undo step per burst of arrow keys (500 ms window in Studio).
    m_studio->beginNudge();
    for (Element &el : m_studio->elements()) {
        if (!m_studio->isSelected(el.id) || el.locked)
            continue;
        el.x += dx;
        el.y += dy;
    }
    m_studio->touchSelection();
}

// ------------------------------------------------------------------ view ops

void CanvasItem::fitToView()
{
    if (!m_studio)
        return;
    const Doc &d = m_studio->doc();
    const double availW = qMax(50.0, width() - 96.0);
    const double availH = qMax(50.0, height() - 96.0);
    const double z = qMin(availW / qMax(1.0, d.width), availH / qMax(1.0, d.height));
    m_zoom = qBound(0.05, z, 4.0);
    m_panX = (width() - d.width * m_zoom) / 2.0;
    m_panY = (height() - d.height * m_zoom) / 2.0;
    emit viewChanged();
    updateEditRect();
    update();
}

void CanvasItem::resetView()
{
    m_zoom = 1.0;
    if (m_studio) {
        m_panX = (width() - m_studio->doc().width) / 2.0;
        m_panY = (height() - m_studio->doc().height) / 2.0;
    }
    emit viewChanged();
    updateEditRect();
    update();
}

void CanvasItem::zoomBy(qreal factor)
{
    zoomAt(factor, width() / 2.0, height() / 2.0);
}

void CanvasItem::zoomAt(qreal factor, qreal sx, qreal sy)
{
    if (!m_userMoved) {
        m_userMoved = true;
        emit userMovedChanged();
    }
    const QPointF before = toDoc(QPointF(sx, sy));
    m_zoom = qBound(0.05, m_zoom * factor, 4.0);
    // Keep the document point under the cursor pinned.
    m_panX = sx - before.x() * m_zoom;
    m_panY = sy - before.y() * m_zoom;
    emit viewChanged();
    updateEditRect();
    update();
}

// -------------------------------------------------------------- text editing

void CanvasItem::beginEditText(const QString &id)
{
    if (!m_studio || id.isEmpty())
        return;
    Element *el = m_studio->elementById(id);
    if (!el || el->locked)
        return;
    m_editingId = id;
    m_editText = el->text;
    m_editColour = el->textColor;
    updateEditRect();
    emit editingChanged();
}

void CanvasItem::commitEditText(const QString &text)
{
    if (!m_studio || m_editingId.isEmpty()) {
        m_editingId.clear();
        emit editingChanged();
        return;
    }
    bool modified = false;
    if (Element *el = m_studio->elementById(m_editingId)) {
        if (el->text != text) {
            m_studio->pushUndo();
            el->text = text;
            if (el->name == QLatin1String("Text") && !text.isEmpty())
                el->name = text.left(24);
            m_studio->touch();
            modified = true;
        }
    }
    m_editingId.clear();
    if (modified)
        emit m_studio->selectionChanged();
    emit editingChanged();
    update();
}

// QQuickTextEdit exposes no line-height, but it does expose its QTextDocument:
// set the proportional height on every block so the editor lays the text out
// exactly as the exporter paints it.
void CanvasItem::applyEditLineHeight(QObject *textDocument)
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(textDocument);
    if (!wrapper || !wrapper->textDocument() || m_editingId.isEmpty())
        return;
    QTextDocument *doc = wrapper->textDocument();
    QTextBlockFormat format;
    format.setLineHeight(m_editLineHeight * 100.0, QTextBlockFormat::ProportionalHeight);
    QTextCursor all(doc);
    all.select(QTextCursor::Document);
    all.mergeBlockFormat(format);
}

void CanvasItem::cancelEditText()
{
    m_editingId.clear();
    emit editingChanged();
    update();
}

void CanvasItem::updateEditRect()
{
    if (m_editingId.isEmpty() || !m_studio) {
        m_editRect = QRectF();
        return;
    }
    const Element *el = m_studio->elementById(m_editingId);
    if (!el) {
        m_editingId.clear();
        m_editRect = QRectF();
        emit editingChanged();
        return;
    }
    const QRectF r = el->rect();
    m_editRect = QRectF(toScreen(r.topLeft()), QSizeF(r.width() * m_zoom, r.height() * m_zoom));
    m_editFontPx = qMax(6.0, el->fontSize * m_zoom);
    m_editColour = el->textColor;
    m_editWeight = el->fontWeight;
    m_editAlign = el->textAlign;
    m_editVAlign = el->textVAlign;
    m_editSpacing = el->letterSpacing * m_zoom;
    m_editLineHeight = el->lineHeight;
    emit editingChanged();
}
