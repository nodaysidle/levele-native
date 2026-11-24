#pragma once

#include "document.h"

#include <QColor>
#include <QString>
#include <QVector>

// A template is a ready-made document. Kept as real geometry + text so the
// gallery previews something the editor can actually open and edit.
struct TemplateInfo {
    QString name;
    QString category;
    Doc doc;
};

QVector<TemplateInfo> buildTemplates();

// Document presets offered by "New design".
struct DocPreset {
    QString name;
    double w = 1080;
    double h = 1080;
};
QVector<DocPreset> buildPresets();

Doc makeBlankDoc(double w, double h, const QString &title);
