#include "templates.h"

namespace {

Element shape(const QString &type, double x, double y, double w, double h, const QString &fill)
{
    Element e = Element::make(type);
    e.x = x;
    e.y = y;
    e.w = w;
    e.h = h;
    e.fill = QColor::fromString(fill);
    e.strokeWidth = 0;
    return e;
}

Element txt(double x, double y, double w, double h, const QString &s, const QString &color,
            double size, int weight = 600, const QString &align = QStringLiteral("left"))
{
    Element e = Element::make(QStringLiteral("text"));
    e.x = x;
    e.y = y;
    e.w = w;
    e.h = h;
    e.text = s;
    e.textColor = QColor::fromString(color);
    e.fontSize = size;
    e.fontWeight = weight;
    e.textAlign = align;
    e.lineHeight = 1.12;
    e.name = s.left(24);
    return e;
}

Element rule(double x, double y, double w, const QString &color, double thickness = 3)
{
    Element e = Element::make(QStringLiteral("line"));
    e.x = x;
    e.y = y;
    e.w = w;
    e.h = 0;
    e.stroke = QColor::fromString(color);
    e.strokeWidth = thickness;
    e.name = QStringLiteral("Rule");
    return e;
}

Doc base(const QString &title, double w, double h, const QString &bg)
{
    Doc d;
    d.id = Doc::newId();
    d.title = title;
    d.width = w;
    d.height = h;
    Page p;
    p.id = Doc::newId();
    p.background = bg;
    d.pages.append(p);
    return d;
}

Page &page(Doc &d, int i) { return d.pages[i]; }

} // namespace

QVector<DocPreset> buildPresets()
{
    return {
        {QStringLiteral("Square post"), 1080, 1080},
        {QStringLiteral("Story"), 1080, 1920},
        {QStringLiteral("Slide 16:9"), 1280, 720},
        {QStringLiteral("Portrait 4:5"), 1080, 1440},
        {QStringLiteral("A4 poster"), 1240, 1748},
        {QStringLiteral("Wide banner"), 1050, 600},
        {QStringLiteral("Full HD"), 1920, 1080},
    };
}

Doc makeBlankDoc(double w, double h, const QString &title)
{
    return base(title, w, h, QStringLiteral("#ffffff"));
}

QVector<TemplateInfo> buildTemplates()
{
    QVector<TemplateInfo> out;

    // 1 — Make it bold
    {
        Doc d = base(QStringLiteral("Make it bold"), 1080, 1080, QStringLiteral("#111113"));
        auto &p = page(d, 0);
        p.elements << shape(QStringLiteral("ellipse"), 620, 360, 480, 480, QStringLiteral("#f545c9"));
        p.elements << shape(QStringLiteral("rect"), 90, 90, 120, 8, QStringLiteral("#f545c9"));
        p.elements << txt(90, 140, 820, 320, QStringLiteral("MAKE\nIT BOLD"),
                          QStringLiteral("#ffffff"), 132, 800);
        p.elements << txt(90, 880, 620, 140,
                          QStringLiteral("Type that refuses to whisper. Built in Levele."),
                          QStringLiteral("#a8a8b0"), 34, 500);
        p.elements << txt(760, 940, 230, 60, QStringLiteral("01 / 01"), QStringLiteral("#f545c9"),
                          26, 600, QStringLiteral("right"));
        out.append({QStringLiteral("Make it bold"), QStringLiteral("Social"), d});
    }

    // 2 — Quiet quote
    {
        Doc d = base(QStringLiteral("Quiet quote"), 1080, 1080, QStringLiteral("#f4f1ea"));
        auto &p = page(d, 0);
        p.elements << shape(QStringLiteral("ellipse"), 92, 92, 22, 22, QStringLiteral("#c2603a"));
        p.elements << txt(90, 300, 900, 330,
                          QStringLiteral("\u201cSimplicity is the\nslow removal of\nthe unnecessary.\u201d"),
                          QStringLiteral("#20201d"), 88, 400);
        p.elements << rule(90, 700, 120, QStringLiteral("#c2603a"), 4);
        p.elements << txt(90, 730, 700, 60, QStringLiteral("ANON, EDITOR AT LARGE"),
                          QStringLiteral("#6d6a63"), 26, 600);
        p.elements << txt(90, 940, 900, 50, QStringLiteral("levele \u00b7 studio notes"),
                          QStringLiteral("#9a968c"), 24, 400);
        out.append({QStringLiteral("Quiet quote"), QStringLiteral("Editorial"), d});
    }

    // 3 — Big weekend sale
    {
        Doc d = base(QStringLiteral("Big weekend sale"), 1080, 1080, QStringLiteral("#f545c9"));
        auto &p = page(d, 0);
        p.elements << shape(QStringLiteral("star"), 700, 90, 230, 230, QStringLiteral("#111113"));
        p.elements << txt(70, 260, 940, 150, QStringLiteral("WEEKEND"), QStringLiteral("#111113"), 118, 800,
                          QStringLiteral("center"));
        p.elements << txt(70, 370, 940, 330, QStringLiteral("40%"), QStringLiteral("#ffffff"), 300, 800,
                          QStringLiteral("center"));
        p.elements << txt(70, 710, 940, 60, QStringLiteral("OFF EVERYTHING"), QStringLiteral("#111113"),
                          44, 700, QStringLiteral("center"));
        p.elements << shape(QStringLiteral("roundRect"), 340, 800, 400, 96, QStringLiteral("#111113"));
        p.elements.last().radius = 48;
        p.elements << txt(340, 822, 400, 60, QStringLiteral("SHOP NOW"), QStringLiteral("#f545c9"), 34, 700,
                          QStringLiteral("center"));
        out.append({QStringLiteral("Big weekend sale"), QStringLiteral("Commerce"), d});
    }

    // 4 — Slow mornings
    {
        Doc d = base(QStringLiteral("Slow mornings"), 1080, 1440, QStringLiteral("#eae4d8"));
        auto &p = page(d, 0);
        Element bgGrad = shape(QStringLiteral("rect"), 0, 0, 1080, 1440, QStringLiteral("#eae4d8"));
        bgGrad.gradient = true;
        bgGrad.gradFrom = QColor::fromString(QStringLiteral("#f3ede2"));
        bgGrad.gradTo = QColor::fromString(QStringLiteral("#cfc3ad"));
        bgGrad.gradAngle = 115;
        bgGrad.name = QStringLiteral("Wash");
        p.elements << bgGrad;
        p.elements << shape(QStringLiteral("ellipse"), 540, 360, 420, 420, QStringLiteral("#e8b06a"));
        p.elements << txt(90, 780, 900, 300, QStringLiteral("Slow\nmornings"), QStringLiteral("#20201d"), 120, 500);
        p.elements << rule(90, 1128, 900, QStringLiteral("#20201d"), 2);
        p.elements << txt(90, 1156, 460, 120,
                          QStringLiteral("Coffee, unhurried. A studio built for one good idea at a time."),
                          QStringLiteral("#4b4740"), 30, 400);
        p.elements << txt(700, 1156, 290, 60, QStringLiteral("VOL. 04"), QStringLiteral("#4b4740"), 26, 600,
                          QStringLiteral("right"));
        out.append({QStringLiteral("Slow mornings"), QStringLiteral("Editorial"), d});
    }

    // 5 — Jazz night
    {
        Doc d = base(QStringLiteral("Jazz night"), 1080, 1440, QStringLiteral("#0e0d16"));
        auto &p = page(d, 0);
        p.elements << shape(QStringLiteral("ellipse"), 740, 120, 300, 300, QStringLiteral("#2a1f4d"));
        p.elements << shape(QStringLiteral("ellipse"), 860, 260, 180, 180, QStringLiteral("#e6c15a"));
        p.elements << txt(90, 640, 900, 90, QStringLiteral("LIVE \u00b7 ONE NIGHT"), QStringLiteral("#e6c15a"),
                          32, 700);
        p.elements << txt(90, 730, 900, 360, QStringLiteral("JAZZ\nNIGHT"), QStringLiteral("#f6f2e8"), 150, 800);
        p.elements << rule(90, 1128, 900, QStringLiteral("#3a3358"), 2);
        p.elements << txt(90, 1156, 500, 120,
                          QStringLiteral("FRI 21 NOV \u00b7 20:00\nBasement Room, 12 Rue Basse"),
                          QStringLiteral("#a9a3c4"), 30, 400);
        p.elements << txt(700, 1156, 290, 60, QStringLiteral("DOORS 19:30"), QStringLiteral("#e6c15a"), 26, 600,
                          QStringLiteral("right"));
        out.append({QStringLiteral("Jazz night"), QStringLiteral("Event"), d});
    }

    // 6 — Editorial resume
    {
        Doc d = base(QStringLiteral("Editorial resume"), 1240, 1748, QStringLiteral("#fbfaf7"));
        auto &p = page(d, 0);
        p.elements << shape(QStringLiteral("rect"), 0, 0, 26, 1748, QStringLiteral("#1d3a34"));
        p.elements << txt(90, 100, 700, 70, QStringLiteral("ELENA MARSH"), QStringLiteral("#1d3a34"), 54, 700);
        p.elements << txt(90, 165, 700, 50, QStringLiteral("Product designer \u00b7 Rotterdam"),
                          QStringLiteral("#6b6b64"), 28, 400);
        p.elements << rule(90, 250, 1060, QStringLiteral("#dcd8cd"), 2);
        p.elements << txt(90, 300, 300, 40, QStringLiteral("EXPERIENCE"), QStringLiteral("#1d3a34"), 22, 700);
        p.elements << txt(400, 300, 750, 40, QStringLiteral("2019 \u2014 Present"), QStringLiteral("#6b6b64"),
                          22, 400);
        p.elements << txt(400, 350, 750, 120,
                          QStringLiteral("Lead Designer, Northwind\nSystems for field research teams."),
                          QStringLiteral("#26261f"), 32, 500);
        p.elements << rule(400, 500, 750, QStringLiteral("#dcd8cd"), 1);
        p.elements << txt(400, 530, 750, 120,
                          QStringLiteral("Designer, Studio Kade\nBrand systems and editorial work."),
                          QStringLiteral("#26261f"), 32, 500);
        p.elements << txt(400, 700, 750, 40, QStringLiteral("2016 \u2014 2019"), QStringLiteral("#6b6b64"),
                          22, 400);
        p.elements << rule(90, 800, 1060, QStringLiteral("#dcd8cd"), 2);
        p.elements << txt(90, 850, 300, 40, QStringLiteral("TOOLS"), QStringLiteral("#1d3a34"), 22, 700);
        p.elements << txt(400, 850, 750, 200,
                          QStringLiteral("Figma \u00b7 Levele \u00b7 Type design\nMotion basics \u00b7 Print production"),
                          QStringLiteral("#26261f"), 32, 500);
        out.append({QStringLiteral("Editorial resume"), QStringLiteral("Document"), d});
    }

    // 7 — Northwind pitch (3 pages)
    {
        Doc d = base(QStringLiteral("Northwind pitch"), 1280, 720, QStringLiteral("#101613"));
        {
            auto &p = page(d, 0);
            p.elements << shape(QStringLiteral("roundRect"), 80, 80, 60, 60, QStringLiteral("#8ee07a"));
            p.elements.last().radius = 16;
            p.elements << txt(160, 92, 400, 40, QStringLiteral("NORTHWIND"), QStringLiteral("#8ee07a"), 26, 700);
            p.elements << txt(80, 260, 1000, 200, QStringLiteral("Field data,\nwithout the friction."),
                              QStringLiteral("#f2f5ec"), 84, 700);
            p.elements << txt(80, 520, 900, 60, QStringLiteral("Pitch \u00b7 November 2026"),
                              QStringLiteral("#8c968a"), 26, 400);
        }
        {
            Page p;
            p.id = Doc::newId();
            p.background = QStringLiteral("#101613");
            p.elements << txt(80, 90, 1120, 60, QStringLiteral("THE PROBLEM"), QStringLiteral("#8ee07a"), 24,
                              700);
            p.elements << txt(80, 180, 1120, 200,
                              QStringLiteral("Teams still capture research on paper, then spend two days "
                                             "retyping it into slides."),
                              QStringLiteral("#f2f5ec"), 52, 600);
            p.elements << shape(QStringLiteral("rect"), 80, 470, 340, 160, QStringLiteral("#182420"));
            p.elements << shape(QStringLiteral("rect"), 460, 470, 340, 160, QStringLiteral("#182420"));
            p.elements << shape(QStringLiteral("rect"), 840, 470, 340, 160, QStringLiteral("#182420"));
            p.elements << txt(110, 510, 280, 90, QStringLiteral("2 days\nlost per study"),
                              QStringLiteral("#f2f5ec"), 30, 500);
            p.elements << txt(490, 510, 280, 90, QStringLiteral("0 live\ncollaboration"),
                              QStringLiteral("#f2f5ec"), 30, 500);
            p.elements << txt(870, 510, 280, 90, QStringLiteral("1 export\nformat"),
                              QStringLiteral("#f2f5ec"), 30, 500);
            d.pages.append(p);
        }
        {
            Page p;
            p.id = Doc::newId();
            p.background = QStringLiteral("#8ee07a");
            p.elements << txt(80, 200, 1120, 240, QStringLiteral("Ask: \u20ac1.4M\nfor 18 months."),
                              QStringLiteral("#101613"), 92, 800);
            p.elements << txt(80, 520, 900, 60, QStringLiteral("hello@northwind.example"),
                              QStringLiteral("#1d3a24"), 30, 500);
            d.pages.append(p);
        }
        out.append({QStringLiteral("Northwind pitch"), QStringLiteral("Deck"), d});
    }

    // 8 — Fieldwork portfolio (3 pages)
    {
        Doc d = base(QStringLiteral("Fieldwork portfolio"), 1080, 1080, QStringLiteral("#f2efe9"));
        {
            auto &p = page(d, 0);
            p.elements << txt(90, 110, 900, 200, QStringLiteral("Fieldwork"), QStringLiteral("#1b1b18"), 120, 700);
            p.elements << txt(90, 320, 620, 100,
                              QStringLiteral("Objects, surfaces and the people who use them."),
                              QStringLiteral("#55524b"), 32, 400);
            p.elements << shape(QStringLiteral("roundRect"), 90, 480, 900, 420, QStringLiteral("#c9c2b4"));
            p.elements.last().radius = 8;
            p.elements << txt(90, 960, 900, 50, QStringLiteral("SELECTED WORK \u00b7 2024\u20132026"),
                              QStringLiteral("#77736a"), 24, 500);
        }
        {
            Page p;
            p.id = Doc::newId();
            p.background = QStringLiteral("#f2efe9");
            p.elements << shape(QStringLiteral("rect"), 90, 110, 480, 620, QStringLiteral("#b9a68d"));
            p.elements << shape(QStringLiteral("rect"), 600, 110, 390, 300, QStringLiteral("#8a9a8f"));
            p.elements << shape(QStringLiteral("rect"), 600, 440, 390, 290, QStringLiteral("#d9c9a8"));
            p.elements << txt(90, 780, 900, 90, QStringLiteral("Kettle No.2"), QStringLiteral("#1b1b18"), 52,
                              600);
            p.elements << txt(90, 870, 900, 60,
                              QStringLiteral("Cast aluminium, powder coat \u00b7 2025"),
                              QStringLiteral("#55524b"), 28, 400);
            d.pages.append(p);
        }
        {
            Page p;
            p.id = Doc::newId();
            p.background = QStringLiteral("#1b1b18");
            p.elements << txt(90, 380, 900, 260, QStringLiteral("Let\u2019s make\nsomething."),
                              QStringLiteral("#f2efe9"), 96, 600);
            p.elements << rule(90, 700, 200, QStringLiteral("#c9c2b4"), 3);
            p.elements << txt(90, 730, 900, 60, QStringLiteral("studio@fieldwork.example"),
                              QStringLiteral("#a49e91"), 30, 400);
            d.pages.append(p);
        }
        out.append({QStringLiteral("Fieldwork portfolio"), QStringLiteral("Portfolio"), d});
    }

    // 9 — The quiet north (Story)
    {
        Doc d = base(QStringLiteral("The quiet north"), 1080, 1920, QStringLiteral("#0B1220"));
        auto &p = page(d, 0);
        Element sky = shape(QStringLiteral("rect"), 0, 0, 1080, 1920, QStringLiteral("#0B1220"));
        sky.gradient = true;
        sky.gradFrom = QColor::fromString(QStringLiteral("#1E3354"));
        sky.gradTo = QColor::fromString(QStringLiteral("#0B1220"));
        sky.gradAngle = 165;
        sky.name = QStringLiteral("Fjord dusk");
        p.elements << sky;
        p.elements << shape(QStringLiteral("triangle"), -120, 620, 860, 560, QStringLiteral("#142138"));
        p.elements << shape(QStringLiteral("triangle"), 380, 540, 820, 640, QStringLiteral("#0F1A2E"));
        p.elements << txt(80, 100, 600, 50, QStringLiteral("FIELD NOTES / 01"), QStringLiteral("#F6EFE4"), 24, 600);
        p.elements << txt(76, 1070, 900, 340, QStringLiteral("The quiet\nnorth."), QStringLiteral("#FFFDF8"), 148, 600);
        p.elements << shape(QStringLiteral("rect"), 80, 1450, 120, 4, QStringLiteral("#FF5A36"));
        p.elements << txt(80, 1490, 680, 130,
                          QStringLiteral("A slow journey through the fjords\nof western Norway."),
                          QStringLiteral("#EDE7DD"), 34, 400);
        Element pill = shape(QStringLiteral("roundRect"), 80, 1680, 340, 72, QStringLiteral("#0B1220"));
        pill.fill = QColor(0, 0, 0, 0);
        pill.stroke = QColor::fromString(QStringLiteral("#F6EFE4"));
        pill.strokeWidth = 2;
        pill.radius = 36;
        p.elements << pill;
        p.elements << txt(80, 1702, 340, 44, QStringLiteral("READ THE STORY  \u2192"),
                          QStringLiteral("#F6EFE4"), 20, 600, QStringLiteral("center"));
        p.elements << txt(860, 1704, 140, 44, QStringLiteral("01 / 04"),
                          QStringLiteral("#F6EFE4"), 20, 600, QStringLiteral("right"));
        out.append({QStringLiteral("The quiet north"), QStringLiteral("Story"), d});
    }

    // 10 — Form & function (Poster)
    {
        Doc d = base(QStringLiteral("Form & function"), 1240, 1748, QStringLiteral("#F6EFE4"));
        auto &p = page(d, 0);
        p.elements << shape(QStringLiteral("ellipse"), 930, 82, 200, 200, QStringLiteral("#FF5A36"));
        p.elements << txt(64, 96, 1120, 340, QStringLiteral("FORM"), QStringLiteral("#17161A"), 260, 800);
        p.elements << txt(70, 390, 1000, 120, QStringLiteral("& FUNCTION"), QStringLiteral("#17161A"), 86, 500);
        p.elements << rule(70, 540, 1100, QStringLiteral("#17161A"), 2);
        Element frame = shape(QStringLiteral("rect"), 70, 600, 1100, 700, QStringLiteral("#E9DCC9"));
        frame.gradient = true;
        frame.gradFrom = QColor::fromString(QStringLiteral("#D8C9B2"));
        frame.gradTo = QColor::fromString(QStringLiteral("#A9947A"));
        frame.gradAngle = 135;
        frame.name = QStringLiteral("Exhibition study");
        p.elements << frame;
        p.elements << shape(QStringLiteral("ellipse"), 440, 740, 360, 360, QStringLiteral("#17161A"));
        p.elements << shape(QStringLiteral("rect"), 520, 820, 200, 200, QStringLiteral("#FF5A36"));
        p.elements << txt(70, 1350, 560, 140, QStringLiteral("AN EXHIBITION OF\nEVERYDAY OBJECTS"),
                          QStringLiteral("#17161A"), 44, 600);
        p.elements << txt(690, 1350, 300, 120, QStringLiteral("04 \u2014 28\nOCTOBER 2026"),
                          QStringLiteral("#17161A"), 30, 500);
        p.elements << txt(1010, 1350, 160, 120, QStringLiteral("HALL\nNORTH"),
                          QStringLiteral("#17161A"), 30, 600);
        p.elements << rule(70, 1600, 1100, QStringLiteral("#17161A"), 2);
        p.elements << txt(70, 1630, 1100, 50, QStringLiteral("FREE ENTRY \u00b7 TUESDAY \u2014 SUNDAY \u00b7 10:00 \u2014 18:00"),
                          QStringLiteral("#6F6A75"), 22, 500);
        out.append({QStringLiteral("Form & function"), QStringLiteral("Poster"), d});
    }

    // 11 — 30-day experiment (Thumbnail)
    {
        Doc d = base(QStringLiteral("30-day experiment"), 1280, 720, QStringLiteral("#0F1420"));
        auto &p = page(d, 0);
        Element bgGrad = shape(QStringLiteral("rect"), 0, 0, 1280, 720, QStringLiteral("#0F1420"));
        bgGrad.gradient = true;
        bgGrad.gradFrom = QColor::fromString(QStringLiteral("#1E293B"));
        bgGrad.gradTo = QColor::fromString(QStringLiteral("#0B0F1A"));
        bgGrad.gradAngle = 125;
        p.elements << bgGrad;
        Element badge = shape(QStringLiteral("roundRect"), 70, 62, 310, 56, QStringLiteral("#FF5A36"));
        badge.radius = 28;
        p.elements << badge;
        p.elements << txt(70, 77, 310, 40, QStringLiteral("30-DAY EXPERIMENT"),
                          QStringLiteral("#17161A"), 20, 700, QStringLiteral("center"));
        p.elements << txt(64, 148, 1020, 420, QStringLiteral("I TRIED\nTHIS FOR\n30 DAYS"),
                          QStringLiteral("#F6EFE4"), 126, 800);
        p.elements << txt(72, 615, 760, 56, QStringLiteral("Here\u2019s what nobody tells you."),
                          QStringLiteral("#F6EFE4"), 34, 500);
        p.elements << shape(QStringLiteral("ellipse"), 1040, 430, 180, 180, QStringLiteral("#FFE066"));
        p.elements << txt(1040, 488, 180, 70, QStringLiteral("WOW"),
                          QStringLiteral("#17161A"), 48, 800, QStringLiteral("center"));
        p.elements << txt(1120, 648, 120, 40, QStringLiteral("EP. 14"),
                          QStringLiteral("#F6EFE4"), 18, 700, QStringLiteral("right"));
        out.append({QStringLiteral("30-day experiment"), QStringLiteral("Thumbnail"), d});
    }

    // 12 — Amara & Noah (Invitation)
    {
        Doc d = base(QStringLiteral("Amara & Noah"), 1080, 1440, QStringLiteral("#F6EFE4"));
        auto &p = page(d, 0);
        Element border = shape(QStringLiteral("rect"), 46, 46, 988, 1348, QStringLiteral("#F6EFE4"));
        border.fill = QColor(0, 0, 0, 0);
        border.stroke = QColor::fromString(QStringLiteral("#B08968"));
        border.strokeWidth = 2;
        p.elements << border;
        p.elements << txt(80, 100, 920, 40, QStringLiteral("TOGETHER WITH THEIR FAMILIES"),
                          QStringLiteral("#8A6A4E"), 20, 600, QStringLiteral("center"));
        p.elements << txt(80, 290, 920, 180, QStringLiteral("Amara"), QStringLiteral("#17161A"), 140, 500, QStringLiteral("center"));
        p.elements << txt(80, 445, 920, 110, QStringLiteral("&"), QStringLiteral("#FF5A36"), 82, 400, QStringLiteral("center"));
        p.elements << txt(80, 540, 920, 180, QStringLiteral("Noah"), QStringLiteral("#17161A"), 140, 500, QStringLiteral("center"));
        p.elements << rule(470, 760, 140, QStringLiteral("#B08968"), 2);
        p.elements << txt(80, 800, 920, 50, QStringLiteral("SATURDAY \u00b7 SEPTEMBER 12 \u00b7 2026"),
                          QStringLiteral("#17161A"), 26, 600, QStringLiteral("center"));
        p.elements << txt(80, 850, 920, 40, QStringLiteral("FOUR IN THE AFTERNOON"),
                          QStringLiteral("#8A6A4E"), 18, 500, QStringLiteral("center"));
        p.elements << txt(80, 980, 920, 50, QStringLiteral("THE OLD ORCHARD"),
                          QStringLiteral("#17161A"), 30, 500, QStringLiteral("center"));
        p.elements << txt(80, 1030, 920, 40, QStringLiteral("SOMERSET, ENGLAND"),
                          QStringLiteral("#8A6A4E"), 18, 500, QStringLiteral("center"));
        p.elements << txt(120, 1160, 840, 110,
                          QStringLiteral("Dinner, dancing and a late-night fire.\nPlease RSVP by July 1."),
                          QStringLiteral("#6F6A75"), 24, 400, QStringLiteral("center"));
        p.elements << txt(80, 1340, 920, 40, QStringLiteral("AMARAANDNOAH.COM"),
                          QStringLiteral("#8A6A4E"), 16, 600, QStringLiteral("center"));
        out.append({QStringLiteral("Amara & Noah"), QStringLiteral("Invitation"), d});
    }

    // 13 — Thirty, flirty (Invitation)
    {
        Doc d = base(QStringLiteral("Thirty, flirty"), 1080, 1440, QStringLiteral("#D9D2FF"));
        auto &p = page(d, 0);
        struct Confetti { double x, y, w, h, rot; const char *col; };
        const Confetti bits[] = {
            {70, 120, 26, 26, 18, "#FF5A36"},
            {940, 160, 32, 18, -24, "#FFE066"},
            {160, 1200, 38, 18, 32, "#0E7C66"},
            {890, 1180, 28, 28, 12, "#FF5A36"},
            {790, 90, 18, 40, 42, "#0E7C66"},
            {330, 180, 22, 22, 0, "#FF8FB1"},
            {1000, 640, 20, 20, 0, "#FFE066"},
            {60, 760, 26, 26, 0, "#FFE066"},
        };
        for (const auto &b : bits) {
            Element c = shape(QStringLiteral("roundRect"), b.x, b.y, b.w, b.h, QString::fromLatin1(b.col));
            c.radius = 6;
            c.rotation = b.rot;
            p.elements << c;
        }
        p.elements << txt(80, 180, 920, 60, QStringLiteral("MAYA IS TURNING"),
                          QStringLiteral("#1D3FBF"), 30, 700, QStringLiteral("center"));
        p.elements << txt(80, 220, 920, 460, QStringLiteral("30"),
                          QStringLiteral("#1D3FBF"), 420, 800, QStringLiteral("center"));
        p.elements << txt(80, 700, 920, 130, QStringLiteral("LET\u2019S PARTY"),
                          QStringLiteral("#FF5A36"), 96, 800, QStringLiteral("center"));
        Element card = shape(QStringLiteral("roundRect"), 240, 860, 600, 330, QStringLiteral("#FFFDF8"));
        card.radius = 30;
        p.elements << card;
        p.elements << txt(240, 900, 600, 50, QStringLiteral("SATURDAY, NOVEMBER 8"),
                          QStringLiteral("#17161A"), 28, 700, QStringLiteral("center"));
        p.elements << txt(240, 960, 600, 60, QStringLiteral("8 PM \u2014 LATE"),
                          QStringLiteral("#FF5A36"), 42, 700, QStringLiteral("center"));
        p.elements << txt(240, 1036, 600, 80,
                          QStringLiteral("The Loft \u00b7 44 Mercer Street\nDress code: your best dancing shoes"),
                          QStringLiteral("#6F6A75"), 22, 400, QStringLiteral("center"));
        p.elements << txt(240, 1134, 600, 36, QStringLiteral("RSVP TO MAYA \u00b7 555 0142"),
                          QStringLiteral("#17161A"), 16, 600, QStringLiteral("center"));
        out.append({QStringLiteral("Thirty, flirty"), QStringLiteral("Invitation"), d});
    }

    // 14 — Studio North (Business card, 2 pages)
    {
        Doc d = base(QStringLiteral("Studio North"), 1050, 600, QStringLiteral("#17161A"));
        {
            auto &front = page(d, 0);
            front.elements << shape(QStringLiteral("ellipse"), 790, -90, 330, 330, QStringLiteral("#FF5A36"));
            front.elements << shape(QStringLiteral("star"), 850, 60, 86, 86, QStringLiteral("#17161A"));
            front.elements << txt(72, 110, 560, 220, QStringLiteral("STUDIO\nNORTH"),
                                  QStringLiteral("#F6EFE4"), 92, 700);
            front.elements << txt(76, 380, 500, 44, QStringLiteral("DESIGN & DIRECTION"),
                                  QStringLiteral("#FFE066"), 20, 600);
            front.elements << txt(72, 500, 340, 40, QStringLiteral("studionorth.co"),
                                  QStringLiteral("#B8B4BC"), 22, 400);
            front.elements << txt(680, 500, 300, 40, QStringLiteral("OSLO / 59.91\u00b0 N"),
                                  QStringLiteral("#B8B4BC"), 18, 500, QStringLiteral("right"));
        }
        {
            Page back;
            back.id = Doc::newId();
            back.background = QStringLiteral("#F6EFE4");
            back.elements << shape(QStringLiteral("rect"), 0, 0, 18, 600, QStringLiteral("#FF5A36"));
            back.elements << txt(80, 90, 600, 80, QStringLiteral("Ingrid Solberg"),
                                 QStringLiteral("#17161A"), 56, 600);
            back.elements << txt(82, 170, 500, 46, QStringLiteral("Creative director"),
                                 QStringLiteral("#FF5A36"), 26, 400);
            back.elements << txt(80, 280, 500, 44, QStringLiteral("ingrid@studionorth.co"),
                                 QStringLiteral("#17161A"), 24, 600);
            back.elements << txt(80, 330, 440, 110,
                                 QStringLiteral("+47 22 44 18 90\nStorgata 14, 0184 Oslo"),
                                 QStringLiteral("#6F6A75"), 20, 400);
            for (int i = 0; i < 9; ++i) {
                const int col = i % 3;
                const int row = i / 3;
                back.elements << shape(QStringLiteral("ellipse"), 820 + col * 50, 360 + row * 50, 22, 22,
                                       i == 4 ? QStringLiteral("#FF5A36") : QStringLiteral("#17161A"));
            }
            back.elements << txt(760, 90, 210, 90, QStringLiteral("SN"),
                                 QStringLiteral("#E2D8CA"), 72, 700, QStringLiteral("right"));
            d.pages.append(back);
        }
        out.append({QStringLiteral("Studio North"), QStringLiteral("Business card"), d});
    }

    // 15 — Elena Marsh (Business card)
    {
        Doc d = base(QStringLiteral("Elena Marsh"), 1050, 600, QStringLiteral("#FFFDF8"));
        auto &p = page(d, 0);
        p.elements << shape(QStringLiteral("rect"), 0, 0, 1050, 18, QStringLiteral("#0E7C66"));
        p.elements << shape(QStringLiteral("ellipse"), 830, 420, 150, 150, QStringLiteral("#E9F5EF"));
        p.elements << txt(64, 90, 600, 74, QStringLiteral("Elena Marsh"), QStringLiteral("#17161A"), 54, 600);
        p.elements << txt(66, 158, 600, 40, QStringLiteral("Strategy & operations consultant"),
                          QStringLiteral("#0E7C66"), 22, 500);
        p.elements << shape(QStringLiteral("rect"), 66, 216, 90, 3, QStringLiteral("#FF5A36"));
        p.elements << txt(66, 260, 500, 150,
                          QStringLiteral("elena@marshconsulting.co\n+1 212 555 0184\nmarshconsulting.co"),
                          QStringLiteral("#6F6A75"), 22, 400);
        p.elements << txt(66, 470, 500, 36, QStringLiteral("NEW YORK \u00b7 LONDON"),
                          QStringLiteral("#17161A"), 17, 600);
        p.elements << txt(750, 80, 240, 64, QStringLiteral("MARSH"),
                          QStringLiteral("#E4EBE6"), 46, 800, QStringLiteral("right"));
        out.append({QStringLiteral("Elena Marsh"), QStringLiteral("Business card"), d});
    }

    return out;
}
