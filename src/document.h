#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

// One drawable object. Coordinates and sizes are in document pixels;
// x/y is the top-left of the unrotated bounding box, rotation is about its centre.
struct Element {
    QString id;
    QString type = QStringLiteral("rect");
    QString name;

    double x = 0;
    double y = 0;
    double w = 160;
    double h = 160;
    double rotation = 0;
    double opacity = 1.0;
    bool visible = true;
    bool locked = false;

    QColor fill = QColor(QStringLiteral("#8a8a8a"));
    bool gradient = false;
    QColor gradFrom = QColor(QStringLiteral("#ff8a5c"));
    QColor gradTo = QColor(QStringLiteral("#ff3d7f"));
    double gradAngle = 45;

    QColor stroke = QColor(QStringLiteral("#000000"));
    double strokeWidth = 0;
    double radius = 0;

    QString text;
    double fontSize = 48;
    int fontWeight = 600;
    QString textAlign = QStringLiteral("left");
    QString textVAlign = QStringLiteral("top");
    QColor textColor = QColor(QStringLiteral("#111111"));
    double letterSpacing = 0;
    double lineHeight = 1.25;

    QString imagePath;
    QString imageFit = QStringLiteral("cover");

    static Element make(const QString &type);

    QRectF rect() const
    {
        return QRectF(QPointF(qMin(x, x + w), qMin(y, y + h)),
                      QSizeF(qAbs(w), qAbs(h)));
    }
    QPointF centre() const { return rect().center(); }

    bool isText() const { return type == QLatin1String("text"); }
    bool isImage() const { return type == QLatin1String("image"); }
    bool isLineLike() const { return type == QLatin1String("line") || type == QLatin1String("arrow"); }
    bool hasFill() const { return type != QLatin1String("line") && type != QLatin1String("arrow"); }

    // The drawn vector: (x,y) is the start, (x+w,y+h) the end. Kept signed so a
    // line keeps the direction the user drew instead of becoming a box.
    QLineF segment() const { return QLineF(QPointF(x, y), QPointF(x + w, y + h)); }

    // Picking in document space. Lines and arrows are hit by distance to the
    // segment, because their bounding rect has a zero width or height and
    // QRectF::contains() would never report them.
    bool containsPoint(const QPointF &docPt, double strokeTol = 6.0) const;
    // Marquee test: segment/rectangle for lines, bounds otherwise.
    bool intersectsRect(const QRectF &docRect, double strokeTol = 6.0) const;

    QJsonObject toJson() const;
    static Element fromJson(const QJsonObject &o);
};

struct Page {
    QString id;
    QString background = QStringLiteral("#ffffff");
    QVector<Element> elements;

    QJsonObject toJson() const;
    static Page fromJson(const QJsonObject &o);
};

struct Doc {
    QString id;
    QString title = QStringLiteral("Untitled");
    double width = 1080;
    double height = 1080;
    QVector<Page> pages;

    QJsonObject toJson() const;
    static Doc fromJson(const QJsonObject &o);

    static QString newId();
    // Ids become file names, so only plain ids are allowed through.
    static bool isValidId(const QString &id);
    static QString sanitiseId(const QString &id);
};

QString elementTypeGlyph(const QString &type);
QString elementTypeLabel(const QString &type);
