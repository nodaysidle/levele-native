#pragma once

#include "document.h"

#include <QImage>
#include <QString>

class QPainter;

namespace Exporter {

// Paints one page into `target` (device coordinates). Shared by the on-screen
// canvas, the thumbnails and every export path, so exported output is exactly
// what the editor showed.
void paintPage(QPainter &p, const Doc &doc, int pageIndex, const QRectF &target);

QImage renderPage(const Doc &doc, int pageIndex, double scale);

// `error`, when given, receives a human-readable reason on failure.
bool writePng(const Doc &doc, int pageIndex, double scale, const QString &path,
              QString *error = nullptr);
bool writePdf(const Doc &doc, const QString &path, QString *error = nullptr);
bool writeGif(const Doc &doc, const QString &path, int width, int fps, QString *error = nullptr);

} // namespace Exporter
