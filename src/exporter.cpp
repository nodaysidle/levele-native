#include "exporter.h"

#include <utility>
#include <QCache>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QImageReader>
#include <QImageWriter>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSet>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QCoreApplication>
#include <QTextDocument>
#include <QTextOption>
#include <QThread>

#include <climits>
#include <cmath>
#include <memory>

namespace {

QSize scaledPageSize(const Doc &doc, double scale)
{
    const double width = doc.width * scale;
    const double height = doc.height * scale;
    if (!(width > 0 && width < INT_MAX - 1 && height > 0 && height < INT_MAX - 1))
        return {};
    return {qMax(1, qRound(width)), qMax(1, qRound(height))};
}

// Images are cached at a bounded size: a 6000 px photo in a 1080 px document
// was being kept at full resolution forever. The key carries the source mtime,
// so a replaced file is re-read instead of serving the old picture.
QImage imageFor(const QString &path, double maxSide)
{
    static thread_local QCache<QString, QImage> cache(64 * 1024 * 1024);
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
        return QImage();
    maxSide = qBound(32.0, maxSide, 16384.0);

    QString key = path + QLatin1Char('@') + QString::number(fi.lastModified().toMSecsSinceEpoch())
        + QLatin1Char('@') + QString::number(qRound(maxSide));
    if (const QImage *hit = cache.object(key))
        return *hit;

    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize target = reader.size();
    if (target.isValid() && maxSide > 32 && qMax(target.width(), target.height()) > maxSide) {
        target.scale(int(maxSide), int(maxSide), Qt::KeepAspectRatio);
        target.setWidth(qMax(1, target.width()));
        target.setHeight(qMax(1, target.height()));
        reader.setScaledSize(target);
    }
    QImage img = reader.read();

    if (!img.isNull())
        cache.insert(key, new QImage(img), img.sizeInBytes());
    return img;
}

QFont elementFont(const Element &e)
{
    QFont f(QStringLiteral("Noto Sans"));
    f.setPixelSize(qMax(2, int(qRound(e.fontSize))));
    f.setWeight(QFont::Weight(qBound(1, e.fontWeight, 1000)));
    return f;
}

QBrush fillBrush(const Element &e, const QRectF &r)
{
    if (!e.gradient)
        return QBrush(e.fill);
    const double rad = e.gradAngle * M_PI / 180.0;
    const QPointF c = r.center();
    const double dx = std::cos(rad) * r.width() / 2.0;
    const double dy = std::sin(rad) * r.height() / 2.0;
    QLinearGradient g(c.x() - dx, c.y() - dy, c.x() + dx, c.y() + dy);
    g.setColorAt(0.0, e.gradFrom);
    g.setColorAt(1.0, e.gradTo);
    return QBrush(g);
}

QPainterPath shapePath(const Element &e, const QRectF &r)
{
    QPainterPath path;
    if (e.type == QLatin1String("ellipse")) {
        path.addEllipse(r);
    } else if (e.type == QLatin1String("triangle")) {
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
    } else if (e.type == QLatin1String("star")) {
        const QPointF c = r.center();
        const double outer = qMin(r.width(), r.height()) / 2.0;
        const double inner = outer * 0.42;
        for (int i = 0; i < 10; ++i) {
            const double a = -M_PI / 2.0 + i * M_PI / 5.0;
            const double rr = (i % 2 == 0) ? outer : inner;
            const QPointF pt(c.x() + std::cos(a) * rr, c.y() + std::sin(a) * rr);
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        path.closeSubpath();
    } else {
        const double rad = qMin(e.radius, qMin(r.width(), r.height()) / 2.0);
        if (rad > 0.5)
            path.addRoundedRect(r, rad, rad);
        else
            path.addRect(r);
    }
    return path;
}

std::unique_ptr<QTextDocument> buildTextDocument(const Element &e, double width)
{
    auto doc = std::make_unique<QTextDocument>();
    doc->setDocumentMargin(0);
    doc->setDefaultFont(elementFont(e));

    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    if (e.textAlign == QLatin1String("center"))
        opt.setAlignment(Qt::AlignHCenter);
    else if (e.textAlign == QLatin1String("right"))
        opt.setAlignment(Qt::AlignRight);
    else if (e.textAlign == QLatin1String("justify"))
        opt.setAlignment(Qt::AlignJustify);
    else
        opt.setAlignment(Qt::AlignLeft);
    doc->setDefaultTextOption(opt);

    QTextCharFormat fmt;
    fmt.setForeground(e.textColor);
    fmt.setFontWeight(QFont::Weight(qBound(1, e.fontWeight, 1000)));
    if (qAbs(e.letterSpacing) > 0.01) {
        fmt.setFontLetterSpacing(e.letterSpacing);
        fmt.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    }

    QTextCursor cur(doc.get());
    cur.insertText(e.text, fmt);

    QTextBlockFormat block;
    block.setLineHeight(e.lineHeight * 100.0, QTextBlockFormat::ProportionalHeight);
    QTextCursor all(doc.get());
    all.select(QTextCursor::Document);
    all.mergeBlockFormat(block);

    doc->setTextWidth(width);
    return doc;
}

QCache<QString, QTextDocument> &textDocCache()
{
    static QCache<QString, QTextDocument> cache(64);
    return cache;
}

void clearTextDocCache()
{
    textDocCache().clear();
}

// Laid-out text is cached on the GUI thread only: a page strip repaint used to
// re-lay every text element out from scratch, while worker threads build and
// free a local QTextDocument so no QObject outlives QGuiApplication at shutdown.
QTextDocument *cachedTextDocumentFor(const Element &e, double width)
{
    static bool hookedQuit = false;
    if (!hookedQuit) {
        hookedQuit = true;
        qAddPostRoutine(clearTextDocCache);
        if (auto *app = QCoreApplication::instance()) {
            QObject::connect(app, &QCoreApplication::aboutToQuit, []() { clearTextDocCache(); });
        }
    }
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(e.text, QString::number(e.fontSize, 'f', 3), QString::number(e.fontWeight), e.textAlign,
             QString::number(e.letterSpacing, 'f', 3), QString::number(e.lineHeight, 'f', 4),
             QString::number(width, 'f', 3), e.textColor.name(QColor::HexArgb));
    QCache<QString, QTextDocument> &cache = textDocCache();
    if (QTextDocument *hit = cache.object(key))
        return hit;

    QTextDocument *doc = buildTextDocument(e, width).release();
    cache.insert(key, doc);
    return doc;
}

void paintText(QPainter &p, const Element &e, const QRectF &r)
{
    if (e.text.isEmpty())
        return;

    const bool onMainThread = QCoreApplication::instance()
        && !QCoreApplication::closingDown()
        && QThread::currentThread() == QCoreApplication::instance()->thread();
    std::unique_ptr<QTextDocument> localDoc;
    QTextDocument *td = nullptr;
    if (onMainThread) {
        td = cachedTextDocumentFor(e, r.width());
    } else {
        localDoc = buildTextDocument(e, r.width());
        td = localDoc.get();
    }
    const double textHeight = td->size().height();

    double offsetY = 0;
    if (e.textVAlign == QLatin1String("middle"))
        offsetY = (r.height() - textHeight) / 2.0;
    else if (e.textVAlign == QLatin1String("bottom"))
        offsetY = r.height() - textHeight;

    p.save();
    p.translate(r.topLeft() + QPointF(0, offsetY));
    // Clip to the laid-out text height, not the element box: a two-line
    // headline in a box sized for one line must still draw both lines.
    const QRectF textClip(0, 0, r.width(), qMax(textHeight, r.height()) + 2);
    p.setClipRect(textClip);
    td->drawContents(&p, textClip);
    p.restore();
}

void paintImage(QPainter &p, const Element &e, const QRectF &r, double maxSide)
{
    const QImage img = e.imagePath.isEmpty() ? QImage() : imageFor(e.imagePath, maxSide);
    if (img.isNull()) {
        // A missing file must look missing, not vanish.
        p.save();
        p.setPen(QPen(QColor(0x88, 0x88, 0x88), 2, Qt::DashLine));
        p.setBrush(QColor(0, 0, 0, 20));
        p.drawRect(r.adjusted(1, 1, -1, -1));
        QFont f = elementFont(e);
        f.setPixelSize(qMax(10, int(qRound(qBound(10.0, r.height() / 5.0, 22.0)))));
        p.setFont(f);
        p.setPen(QColor(0x99, 0x99, 0x99));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Missing image"));
        p.restore();
        return;
    }

    QRectF source(0, 0, img.width(), img.height());
    const double targetAspect = r.width() / qMax(1.0, r.height());
    const double sourceAspect = source.width() / qMax(1.0, source.height());

    if (e.imageFit == QLatin1String("contain")) {
        QRectF dest = r;
        if (sourceAspect > targetAspect) {
            dest.setHeight(r.width() / sourceAspect);
            dest.moveCenter(r.center());
        } else {
            dest.setWidth(r.height() * sourceAspect);
            dest.moveCenter(r.center());
        }
        p.drawImage(dest, img, source);
        return;
    }

    // cover: crop the source to the target aspect
    if (sourceAspect > targetAspect) {
        const double w = source.height() * targetAspect;
        source = QRectF((source.width() - w) / 2.0, 0, w, source.height());
    } else {
        const double h = source.width() / targetAspect;
        source = QRectF(0, (source.height() - h) / 2.0, source.width(), h);
    }
    p.drawImage(r, img, source);
}

void paintElement(QPainter &p, const Element &e, double maxImageSide)
{
    if (!e.visible || e.opacity <= 0.001)
        return;

    const QRectF r = e.rect();
    p.save();
    p.setOpacity(e.opacity);
    if (qAbs(e.rotation) > 0.01) {
        p.translate(r.center());
        p.rotate(e.rotation);
        p.translate(-r.center());
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (e.isText()) {
        paintText(p, e, r);
    } else if (e.isImage()) {
        const double rad = qMin(e.radius, qMin(r.width(), r.height()) / 2.0);
        if (rad > 0.5) {
            QPainterPath clip;
            clip.addRoundedRect(r, rad, rad);
            p.setClipPath(clip, Qt::IntersectClip);
        }
        paintImage(p, e, r, maxImageSide);
    } else if (e.type == QLatin1String("line") || e.type == QLatin1String("arrow")) {
        const QPointF a(e.x, e.y);
        const QPointF b(e.x + e.w, e.y + e.h);
        QPen pen(e.stroke, qMax(0.5, e.strokeWidth), Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(a, b);
        if (e.type == QLatin1String("arrow")) {
            const double angle = std::atan2(b.y() - a.y(), b.x() - a.x());
            const double len = qMax(10.0, e.strokeWidth * 3.4);
            const double spread = 0.42;
            QPainterPath head;
            head.moveTo(b);
            head.lineTo(b.x() - std::cos(angle - spread) * len, b.y() - std::sin(angle - spread) * len);
            head.moveTo(b);
            head.lineTo(b.x() - std::cos(angle + spread) * len, b.y() - std::sin(angle + spread) * len);
            p.setBrush(Qt::NoBrush);
            p.drawPath(head);
        }
    } else {
        const QPainterPath path = shapePath(e, r);
        p.setBrush(fillBrush(e, r));
        if (e.strokeWidth > 0.01) {
            p.setPen(QPen(e.stroke, e.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        } else {
            p.setPen(Qt::NoPen);
        }
        p.drawPath(path);
    }

    p.restore();
}

// ------------------------------------------------------------------ GIF bits

struct GifPalette {
    QVector<QRgb> colours;
};

int colourDistance(QRgb a, QRgb b)
{
    const int dr = qRed(a) - qRed(b);
    const int dg = qGreen(a) - qGreen(b);
    const int db = qBlue(a) - qBlue(b);
    return dr * dr + dg * dg + db * db;
}

// Sample a bounded set of colours across all pages for the global table.
// When `cachedFrames` is non-null, rendered pages are kept for the encode pass
// so normal-sized GIFs do not rasterise every page twice.
GifPalette buildPalette(const Doc &doc, double scale, QVector<QImage> *cachedFrames)
{
    QSet<QRgb> colours;
    const int sampleSide = qMin(96, int(std::sqrt(262'144.0 / doc.pages.size())));
    if (cachedFrames)
        cachedFrames->reserve(doc.pages.size());
    for (int i = 0; i < doc.pages.size(); ++i) {
        const QImage img = Exporter::renderPage(doc, i, scale);
        if (img.isNull()) {
            if (cachedFrames)
                cachedFrames->clear();
            return {};
        }
        if (cachedFrames)
            cachedFrames->append(img);
        const int stepX = (img.width() + sampleSide - 1) / sampleSide;
        const int stepY = (img.height() + sampleSide - 1) / sampleSide;
        for (int y = 0; y < img.height(); y += stepY) {
            const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            for (int x = 0; x < img.width(); x += stepX)
                colours.insert(line[x] & 0x00ffffff);
        }
    }

    struct Box {
        QVector<QRgb> colours;
        int widest = 0;
        int range = 0;
    };

    auto measure = [](Box &b) {
        int rmin = 255, rmax = 0, gmin = 255, gmax = 0, bmin = 255, bmax = 0;
        for (QRgb c : std::as_const(b.colours)) {
            rmin = qMin(rmin, qRed(c)); rmax = qMax(rmax, qRed(c));
            gmin = qMin(gmin, qGreen(c)); gmax = qMax(gmax, qGreen(c));
            bmin = qMin(bmin, qBlue(c)); bmax = qMax(bmax, qBlue(c));
        }
        const int dr = rmax - rmin, dg = gmax - gmin, db = bmax - bmin;
        b.range = qMax(dr, qMax(dg, db));
        b.widest = (b.range == dr) ? 0 : (b.range == dg ? 1 : 2);
    };

    QVector<Box> boxes;
    Box first;
    first.colours = colours.values();
    measure(first);
    boxes.append(first);

    while (boxes.size() < 256) {
        int best = -1;
        int bestRange = 0;
        for (int i = 0; i < boxes.size(); ++i) {
            if (boxes.at(i).colours.size() > 1 && boxes.at(i).range > bestRange) {
                bestRange = boxes.at(i).range;
                best = i;
            }
        }
        if (best < 0 || bestRange <= 0)
            break;

        Box box = boxes.takeAt(best);
        const int channel = box.widest;
        std::sort(box.colours.begin(), box.colours.end(), [channel](QRgb x, QRgb y) {
            if (channel == 0) return qRed(x) < qRed(y);
            if (channel == 1) return qGreen(x) < qGreen(y);
            return qBlue(x) < qBlue(y);
        });
        const int mid = box.colours.size() / 2;
        Box lo, hi;
        lo.colours = box.colours.mid(0, mid);
        hi.colours = box.colours.mid(mid);
        measure(lo);
        measure(hi);
        if (!lo.colours.isEmpty())
            boxes.append(lo);
        if (!hi.colours.isEmpty())
            boxes.append(hi);
    }

    GifPalette pal;
    for (const Box &b : boxes) {
        qint64 r = 0, g = 0, bl = 0;
        for (QRgb c : std::as_const(b.colours)) {
            r += qRed(c);
            g += qGreen(c);
            bl += qBlue(c);
        }
        const int n = qMax(1, int(b.colours.size()));
        pal.colours.append(qRgb(int(r / n), int(g / n), int(bl / n)));
    }
    if (pal.colours.isEmpty())
        pal.colours.append(qRgb(0, 0, 0));
    while (pal.colours.size() < 256)
        pal.colours.append(pal.colours.last());
    return pal;
}

quint8 nearestIndex(QRgb c, const GifPalette &pal)
{
    int best = 0;
    int bestDist = INT_MAX;
    for (int i = 0; i < pal.colours.size(); ++i) {
        const int d = colourDistance(c, pal.colours.at(i));
        if (d < bestDist) {
            bestDist = d;
            best = i;
            if (d == 0)
                break;
        }
    }
    return quint8(best);
}

class GifWriter
{
public:
    explicit GifWriter(QByteArray &out)
        : m_out(out)
    {
    }

    void u8(quint8 v) { m_out.append(char(v)); }
    void u16(quint16 v)
    {
        m_out.append(char(v & 0xff));
        m_out.append(char((v >> 8) & 0xff));
    }
    void bytes(const char *d, int n) { m_out.append(d, n); }

    void flushBits()
    {
        while (m_bitCount >= 8) {
            m_block.append(char(m_bitBuffer & 0xff));
            m_bitBuffer >>= 8;
            m_bitCount -= 8;
            if (m_block.size() == 255)
                flushBlock();
        }
    }

    void writeCode(int code, int size)
    {
        m_bitBuffer |= (quint32(code) & ((1u << size) - 1u)) << m_bitCount;
        m_bitCount += size;
        flushBits();
    }

    void endBits()
    {
        if (m_bitCount > 0) {
            m_block.append(char(m_bitBuffer & 0xff));
            m_bitBuffer = 0;
            m_bitCount = 0;
        }
        flushBlock();
    }

    void flushBlock()
    {
        if (m_block.isEmpty())
            return;
        m_out.append(char(m_block.size()));
        m_out.append(m_block);
        m_block.clear();
    }

private:
    QByteArray &m_out;
    quint32 m_bitBuffer = 0;
    int m_bitCount = 0;
    QByteArray m_block;
};

void lzwEncode(GifWriter &w, const QVector<quint8> &indices, int minCodeSize)
{
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    int codeSize = minCodeSize + 1;
    int nextCode = endCode + 1;
    QHash<quint32, int> dict;

    w.writeCode(clearCode, codeSize);
    if (indices.isEmpty()) {
        w.writeCode(endCode, codeSize);
        w.endBits();
        return;
    }

    int prefix = indices.at(0);
    for (int i = 1; i < indices.size(); ++i) {
        const int k = indices.at(i);
        const quint32 key = (quint32(prefix) << 8) | quint32(k);
        const auto it = dict.constFind(key);
        if (it != dict.constEnd()) {
            prefix = it.value();
            continue;
        }
        w.writeCode(prefix, codeSize);
        if (nextCode < 4096) {
            dict.insert(key, nextCode);
            ++nextCode;
            // The decoder adds its table entry one code later than the encoder,
            // so the width must grow one code later too: at 2^codeSize + 1,
            // not at 2^codeSize. Growing early desyncs every subsequent code.
            if (nextCode > (1 << codeSize) && codeSize < 12)
                ++codeSize;
        } else {
            w.writeCode(clearCode, codeSize);
            dict.clear();
            codeSize = minCodeSize + 1;
            nextCode = endCode + 1;
        }
        prefix = k;
    }
    w.writeCode(prefix, codeSize);
    w.writeCode(endCode, codeSize);
    w.endBits();
}

} // namespace

namespace Exporter {

void paintPage(QPainter &p, const Doc &doc, int pageIndex, const QRectF &target)
{
    if (pageIndex < 0 || pageIndex >= doc.pages.size())
        return;
    const Page &page = doc.pages.at(pageIndex);

    p.save();
    p.setClipRect(target);
    const QColor bg = QColor::fromString(page.background);
    p.fillRect(target, bg.isValid() ? bg : QColor(Qt::white));

    if (qAbs(target.width() - doc.width) < 0.01 && qAbs(target.height() - doc.height) < 0.01) {
        p.translate(target.topLeft());
    } else {
        p.translate(target.topLeft());
        p.scale(target.width() / qMax(1.0, doc.width), target.height() / qMax(1.0, doc.height));
    }

    // Bound the decoded image size: 2x the document, or the export target when
    // that is bigger, so exports stay sharp.
    const double maxImageSide = qMax(2.0 * qMax(doc.width, doc.height), target.width());
    for (const Element &e : page.elements)
        paintElement(p, e, maxImageSide);

    p.restore();
}

QImage renderPage(const Doc &doc, int pageIndex, double scale)
{
    const QSize size = scaledPageSize(doc, scale);
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    if (img.isNull())
        return {};

    img.fill(Qt::white);
    QPainter p(&img);
    paintPage(p, doc, pageIndex, QRectF(QPointF(0, 0), size));
    p.end();
    return img;
}

bool writePng(const Doc &doc, int pageIndex, double scale, const QString &path, QString *error)
{
    if (pageIndex < 0 || pageIndex >= doc.pages.size()) {
        if (error)
            *error = QStringLiteral("that page no longer exists");
        return false;
    }
    scale = qBound(0.1, scale, 8.0);
    const QSize size = scaledPageSize(doc, scale);
    if (size.isEmpty() || qint64(size.width()) * size.height() > 64'000'000) {
        if (error)
            *error = QStringLiteral("the PNG is too large; choose a smaller scale");
        return false;
    }
    const QImage img = renderPage(doc, pageIndex, scale);
    if (img.isNull()) {
        if (error)
            *error = QStringLiteral("the page could not be rendered");
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QImageWriter writer(&file, "PNG");
    if (!writer.write(img)) {
        if (error)
            *error = writer.errorString();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool writePdf(const Doc &doc, const QString &path, QString *error)
{
    if (doc.pages.isEmpty()) {
        if (error)
            *error = QStringLiteral("there are no pages to export");
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    {
        QPdfWriter writer(&file);
        writer.setResolution(96);
        writer.setTitle(doc.title);
        writer.setCreator(QStringLiteral("Levele"));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        const QSizeF mm(doc.width * 25.4 / 96.0, doc.height * 25.4 / 96.0);
        writer.setPageSize(QPageSize(mm, QPageSize::Millimeter));

        QPainter p;
        if (!p.begin(&writer)) {
            file.cancelWriting();
            if (error)
                *error = QStringLiteral("the file could not be opened for writing");
            return false;
        }
        p.setRenderHint(QPainter::Antialiasing, true);

        for (int i = 0; i < doc.pages.size(); ++i) {
            if (i > 0)
                writer.newPage();
            paintPage(p, doc, i, QRectF(0, 0, writer.width(), writer.height()));
        }
        p.end();
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (!QFileInfo::exists(path) || QFileInfo(path).size() == 0) {
        if (error)
            *error = QStringLiteral("the PDF came out empty");
        return false;
    }
    return true;
}

bool writeGif(const Doc &doc, const QString &path, int width, int fps, QString *error)
{
    if (doc.pages.isEmpty()) {
        if (error)
            *error = QStringLiteral("there are no pages to export");
        return false;
    }
    const double scale = double(width) / doc.width;
    const QSize size = scaledPageSize(doc, scale);
    const qint64 pixels = qint64(size.width()) * size.height();
    if (doc.pages.size() > 1024 || size.isEmpty() || size.width() > 8192 || size.height() > 8192
        || pixels > 8'000'000 || pixels * doc.pages.size() > 512'000'000) {
        if (error)
            *error = QStringLiteral("the GIF is too large; choose a smaller width or fewer pages");
        return false;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QVector<QImage> cachedFrames;
    const bool keepFrames = pixels * 4 * doc.pages.size() <= 64'000'000;
    const GifPalette pal = buildPalette(doc, scale, keepFrames ? &cachedFrames : nullptr);
    if (pal.colours.isEmpty()) {
        if (error)
            *error = QStringLiteral("the frames could not be rendered");
        return false;
    }

    QByteArray out;
    GifWriter w(out);
    w.bytes("GIF89a", 6);
    w.u16(quint16(size.width()));
    w.u16(quint16(size.height()));
    w.u8(0xF7); // global colour table, 256 entries
    w.u8(0);    // background index
    w.u8(0);    // pixel aspect ratio
    for (int i = 0; i < 256; ++i) {
        const QRgb c = pal.colours.at(i);
        w.u8(quint8(qRed(c)));
        w.u8(quint8(qGreen(c)));
        w.u8(quint8(qBlue(c)));
    }

    // NETSCAPE2.0 — loop forever
    w.u8(0x21);
    w.u8(0xFF);
    w.u8(0x0B);
    w.bytes("NETSCAPE2.0", 11);
    w.u8(0x03);
    w.u8(0x01);
    w.u16(0);
    w.u8(0x00);

    const int delay = qMax(2, int(qRound(100.0 / qBound(1, fps, 50))));

    for (int i = 0; i < doc.pages.size(); ++i) {
        const QImage frame = (i < cachedFrames.size()) ? cachedFrames.at(i)
                                                       : renderPage(doc, i, scale);
        if (frame.isNull()) {
            if (error)
                *error = QStringLiteral("the frames could not be rendered");
            return false;
        }
        w.u8(0x21);
        w.u8(0xF9);
        w.u8(0x04);
        w.u8(0x04); // leave the frame in place before drawing the next
        w.u16(quint16(delay));
        w.u8(0x00);
        w.u8(0x00);

        w.u8(0x2C);
        w.u16(0);
        w.u16(0);
        w.u16(quint16(frame.width()));
        w.u16(quint16(frame.height()));
        w.u8(0x00);

        QVector<quint8> indices;
        indices.resize(pixels);
        QHash<QRgb, quint8> lookup;
        for (int y = 0; y < frame.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(frame.constScanLine(y));
            quint8 *dst = indices.data() + y * frame.width();
            for (int x = 0; x < frame.width(); ++x) {
                const QRgb c = line[x] & 0x00ffffff;
                const auto hit = lookup.constFind(c);
                if (hit != lookup.constEnd()) {
                    dst[x] = hit.value();
                } else {
                    const quint8 index = nearestIndex(c, pal);
                    if (lookup.size() < 65'536)
                        lookup.insert(c, index);
                    dst[x] = index;
                }
            }
        }

        w.u8(0x08); // LZW minimum code size
        lzwEncode(w, indices, 8);
        w.u8(0x00); // block terminator
        if (file.write(out) != out.size()) {
            if (error)
                *error = file.errorString();
            return false;
        }
        out.clear();
    }
    w.u8(0x3B);

    if (file.write(out) != out.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace Exporter
