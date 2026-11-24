#include "document.h"

#include <QPolygonF>
#include <QRegularExpression>
#include <QUuid>
#include <QtMath>

namespace {

QString colToStr(const QColor &c) { return c.name(QColor::HexArgb); }

QColor colFrom(const QJsonValue &v, const QColor &fallback)
{
    if (v.isString()) {
        const QColor c = QColor::fromString(v.toString());
        if (c.isValid())
            return c;
    }
    return fallback;
}

double num(const QJsonObject &o, const char *key, double fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isDouble())
        return fallback;
    const double d = v.toDouble();
    return std::isfinite(d) ? d : fallback;
}

bool boolean(const QJsonObject &o, const char *key, bool fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isBool() ? v.toBool() : fallback;
}

QString str(const QJsonObject &o, const char *key, const QString &fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString() : fallback;
}

QPointF rotatePt(const QPointF &pt, const QPointF &centre, double degrees)
{
    if (qAbs(degrees) < 0.0001)
        return pt;
    const double rad = qDegreesToRadians(degrees);
    const double cs = std::cos(rad);
    const double sn = std::sin(rad);
    const QPointF d = pt - centre;
    return QPointF(centre.x() + d.x() * cs - d.y() * sn, centre.y() + d.x() * sn + d.y() * cs);
}

// Shortest distance from `p` to the segment `seg`.
double distanceToSegment(const QPointF &p, const QLineF &seg)
{
    const QPointF a = seg.p1();
    const QPointF b = seg.p2();
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 <= 1e-9)
        return QLineF(p, a).length();
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = qBound(0.0, t, 1.0);
    return QLineF(p, a + ab * t).length();
}

bool segmentHitsRect(const QLineF &seg, const QRectF &rect)
{
    if (rect.contains(seg.p1()) || rect.contains(seg.p2()))
        return true;
    const QLineF edges[4] = {
        QLineF(rect.topLeft(), rect.topRight()), QLineF(rect.topRight(), rect.bottomRight()),
        QLineF(rect.bottomRight(), rect.bottomLeft()), QLineF(rect.bottomLeft(), rect.topLeft())};
    for (const QLineF &edge : edges) {
        QPointF hit;
        if (seg.intersects(edge, &hit) == QLineF::BoundedIntersection)
            return true;
    }
    return false;
}

} // namespace

QString Doc::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(13);
}

bool Doc::isValidId(const QString &id)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{0,63}$"));
    return re.match(id).hasMatch();
}

// A document that claims id "../../evil" must never reach the file system.
QString Doc::sanitiseId(const QString &id)
{
    return isValidId(id) ? id : Doc::newId();
}

Element Element::make(const QString &type)
{
    Element e;
    e.id = Doc::newId();
    e.type = type;
    e.name = elementTypeLabel(type);
    if (type == QLatin1String("text")) {
        e.w = 420;
        e.h = 120;
        e.text = QStringLiteral("Double-click to edit");
        e.fill = QColor(Qt::transparent);
        e.textColor = QColor(QStringLiteral("#111111"));
        e.fontSize = 56;
        e.name = QStringLiteral("Text");
    } else if (type == QLatin1String("image")) {
        e.w = 360;
        e.h = 360;
        e.fill = QColor(QStringLiteral("#e6e6e6"));
        e.name = QStringLiteral("Image");
    } else if (type == QLatin1String("line") || type == QLatin1String("arrow")) {
        e.w = 300;
        e.h = 0;
        e.strokeWidth = 6;
        e.stroke = QColor(QStringLiteral("#111111"));
    } else if (type == QLatin1String("star") || type == QLatin1String("triangle")) {
        e.w = 220;
        e.h = 220;
    } else if (type == QLatin1String("roundRect")) {
        e.radius = 28;
    }
    return e;
}

QJsonObject Element::toJson() const
{
    QJsonObject o;
    o["id"] = id;
    o["type"] = type;
    o["name"] = name;
    o["x"] = x;
    o["y"] = y;
    o["w"] = w;
    o["h"] = h;
    o["rotation"] = rotation;
    o["opacity"] = opacity;
    o["visible"] = visible;
    o["locked"] = locked;
    o["fill"] = colToStr(fill);
    o["gradient"] = gradient;
    o["gradFrom"] = colToStr(gradFrom);
    o["gradTo"] = colToStr(gradTo);
    o["gradAngle"] = gradAngle;
    o["stroke"] = colToStr(stroke);
    o["strokeWidth"] = strokeWidth;
    o["radius"] = radius;
    o["text"] = text;
    o["fontSize"] = fontSize;
    o["fontWeight"] = fontWeight;
    o["textAlign"] = textAlign;
    o["textVAlign"] = textVAlign;
    o["textColor"] = colToStr(textColor);
    o["letterSpacing"] = letterSpacing;
    o["lineHeight"] = lineHeight;
    o["imagePath"] = imagePath;
    o["imageFit"] = imageFit;
    return o;
}

Element Element::fromJson(const QJsonObject &o)
{
    Element e;
    e.id = str(o, "id", QString());
    if (e.id.isEmpty())
        e.id = Doc::newId();
    e.type = str(o, "type", QStringLiteral("rect"));
    e.name = str(o, "name", elementTypeLabel(e.type));
    e.x = qBound(-1e6, num(o, "x", 0), 1e6);
    e.y = qBound(-1e6, num(o, "y", 0), 1e6);
    e.w = qBound(-1e6, num(o, "w", 160), 1e6);
    e.h = qBound(-1e6, num(o, "h", 160), 1e6);
    e.rotation = qBound(-360000.0, num(o, "rotation", 0), 360000.0);
    e.opacity = qBound(0.0, num(o, "opacity", 1.0), 1.0);
    e.visible = boolean(o, "visible", true);
    e.locked = boolean(o, "locked", false);
    e.fill = colFrom(o.value("fill"), QColor(QStringLiteral("#8a8a8a")));
    e.gradient = boolean(o, "gradient", false);
    e.gradFrom = colFrom(o.value("gradFrom"), QColor(QStringLiteral("#ff8a5c")));
    e.gradTo = colFrom(o.value("gradTo"), QColor(QStringLiteral("#ff3d7f")));
    e.gradAngle = qBound(-360000.0, num(o, "gradAngle", 45), 360000.0);
    e.stroke = colFrom(o.value("stroke"), QColor(QStringLiteral("#000000")));
    e.strokeWidth = qBound(0.0, num(o, "strokeWidth", 0), 2000.0);
    e.radius = qBound(0.0, num(o, "radius", 0), 10000.0);
    e.text = str(o, "text", QString());
    e.fontSize = qBound(2.0, num(o, "fontSize", 48), 2000.0);
    e.fontWeight = qBound(1, qRound(qBound(1.0, num(o, "fontWeight", 600), 1000.0)), 1000);
    e.textAlign = str(o, "textAlign", QStringLiteral("left"));
    e.textVAlign = str(o, "textVAlign", QStringLiteral("top"));
    e.textColor = colFrom(o.value("textColor"), QColor(QStringLiteral("#111111")));
    e.letterSpacing = qBound(-200.0, num(o, "letterSpacing", 0), 1000.0);
    e.lineHeight = qBound(0.5, num(o, "lineHeight", 1.25), 10.0);
    e.imagePath = str(o, "imagePath", QString());
    e.imageFit = str(o, "imageFit", QStringLiteral("cover"));
    return e;
}

bool Element::containsPoint(const QPointF &docPt, double strokeTol) const
{
    if (!visible || locked)
        return false;
    const QRectF r = rect();
    const QPointF local = rotation == 0 ? docPt : rotatePt(docPt, r.center(), -rotation);
    if (isLineLike()) {
        const double tol = qMax(strokeWidth / 2.0, strokeTol);
        return distanceToSegment(local, segment()) <= tol;
    }
    return r.contains(local);
}

bool Element::intersectsRect(const QRectF &docRect, double strokeTol) const
{
    if (!visible || locked)
        return false;
    if (!isLineLike()) {
        const QRectF r = rect();
        if (qAbs(rotation) < 0.0001)
            return docRect.intersects(r);
        const QPointF c = r.center();
        QPolygonF poly;
        for (const QPointF &corner : {r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft()})
            poly << rotatePt(corner, c, rotation);
        return poly.intersects(QPolygonF(docRect));
    }

    // A line's bounding rect is degenerate, so test the drawn segment itself
    // (with the stroke width as slack). Rotation is applied by rotating the
    // marquee into the element's own frame.
    QRectF local = docRect;
    if (rotation != 0) {
        const QPointF c = rect().center();
        QPolygonF box;
        for (const QPointF &corner : {docRect.topLeft(), docRect.topRight(),
                                      docRect.bottomRight(), docRect.bottomLeft()})
            box << rotatePt(corner, c, -rotation);
        local = box.boundingRect();
    }
    const double tol = qMax(strokeWidth / 2.0, strokeTol);
    return segmentHitsRect(segment(), local.adjusted(-tol, -tol, tol, tol));
}

QJsonObject Page::toJson() const
{
    QJsonObject o;
    o["id"] = id;
    o["background"] = background;
    QJsonArray arr;
    for (const Element &e : elements)
        arr.append(e.toJson());
    o["elements"] = arr;
    return o;
}

Page Page::fromJson(const QJsonObject &o)
{
    Page p;
    p.id = str(o, "id", QString());
    if (p.id.isEmpty())
        p.id = Doc::newId();
    const QString bg = str(o, "background", QStringLiteral("#ffffff"));
    p.background = QColor::fromString(bg).isValid() ? bg : QStringLiteral("#ffffff");
    const QJsonArray arr = o.value("elements").toArray();
    p.elements.reserve(arr.size());
    for (const QJsonValue &v : arr)
        p.elements.append(Element::fromJson(v.toObject()));
    return p;
}

QJsonObject Doc::toJson() const
{
    QJsonObject o;
    o["version"] = 1;
    o["id"] = id;
    o["title"] = title;
    o["width"] = width;
    o["height"] = height;
    QJsonArray arr;
    for (const Page &p : pages)
        arr.append(p.toJson());
    o["pages"] = arr;
    return o;
}

Doc Doc::fromJson(const QJsonObject &o)
{
    Doc d;
    d.id = sanitiseId(str(o, "id", Doc::newId()));
    d.title = str(o, "title", QStringLiteral("Untitled"));
    // A negative size makes a null image on export and an absurd zoom; a huge
    // one allocates gigabytes. Both are clamped.
    d.width = qBound(16.0, num(o, "width", 1080), 16384.0);
    d.height = qBound(16.0, num(o, "height", 1080), 16384.0);
    const QJsonArray arr = o.value("pages").toArray();
    d.pages.reserve(qMax(1, arr.size()));
    for (const QJsonValue &v : arr)
        d.pages.append(Page::fromJson(v.toObject()));
    if (d.pages.isEmpty())
        d.pages.append(Page{Doc::newId(), QStringLiteral("#ffffff"), {}});
    return d;
}

QString elementTypeLabel(const QString &type)
{
    if (type == QLatin1String("rect")) return QStringLiteral("Rectangle");
    if (type == QLatin1String("roundRect")) return QStringLiteral("Rounded");
    if (type == QLatin1String("ellipse")) return QStringLiteral("Ellipse");
    if (type == QLatin1String("triangle")) return QStringLiteral("Triangle");
    if (type == QLatin1String("star")) return QStringLiteral("Star");
    if (type == QLatin1String("line")) return QStringLiteral("Line");
    if (type == QLatin1String("arrow")) return QStringLiteral("Arrow");
    if (type == QLatin1String("text")) return QStringLiteral("Text");
    if (type == QLatin1String("image")) return QStringLiteral("Image");
    return type;
}

QString elementTypeGlyph(const QString &type)
{
    if (type == QLatin1String("rect")) return QStringLiteral("\u25A1");
    if (type == QLatin1String("roundRect")) return QStringLiteral("\u25A2");
    if (type == QLatin1String("ellipse")) return QStringLiteral("\u25CB");
    if (type == QLatin1String("triangle")) return QStringLiteral("\u25B3");
    if (type == QLatin1String("star")) return QStringLiteral("\u2605");
    if (type == QLatin1String("line")) return QStringLiteral("\u2571");
    if (type == QLatin1String("arrow")) return QStringLiteral("\u2192");
    if (type == QLatin1String("text")) return QStringLiteral("T");
    if (type == QLatin1String("image")) return QStringLiteral("\u25A3");
    return QStringLiteral("\u25A1");
}
