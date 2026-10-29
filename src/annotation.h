#pragma once

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

class QImage;
class QPainter;

enum class Tool {
    Select,
    Arrow,
    Rectangle,
    Text,
    Highlighter,
    Pixelate,
};

struct Annotation {
    Tool tool = Tool::Arrow;
    QColor color;
    int size = 1; // index into the small / medium / large presets
    QPoint from; // start point; top-left corner for text
    QPoint to; // end point for arrows, rectangles and pixelation
    QVector<QPoint> points; // highlighter stroke
    QString text;

    // Area the annotation can paint into, including pen width and arrow heads.
    QRect bounds() const;
    // False for stray clicks that would leave an invisible annotation behind.
    bool isMeaningful() const;
};

namespace Annotations
{

constexpr int kSizeCount = 3;

int penWidth(int size);
int highlighterWidth(int size);
int pixelBlock(int size);
QFont font(int size);
QRect textRect(const Annotation &annotation);

// Paints everything except pixelation, which has to read the pixels underneath.
void paint(QPainter &painter, const Annotation &annotation);

// Replaces rect with block-averaged cells. Unlike a light blur this cannot be undone.
void pixelate(QImage &image, const QRect &rect, int block);

// Draws the annotations onto image in order, so pixelation also hides earlier ones.
void render(QImage &image, const QVector<Annotation> &annotations);

}
