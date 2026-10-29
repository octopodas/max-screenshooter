#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>

// Pure rectangle math for the selection, kept free of widgets so it can be unit tested.
// All rectangles use inclusive pixel coordinates, like QRect.
namespace Geometry
{

// A handle is a combination of the edges it drags; Inside means "move the whole rectangle".
enum Handle {
    NoHandle = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8,
    Inside = 16,
};

// The eight resize handles, corners first so they win hit tests on small selections.
const QVector<int> &resizeHandles();

// Rectangle spanned by two corner pixels, both included.
QRect rectFromPoints(const QPoint &a, const QPoint &b);

// Centre of a resize handle, on the outer boundary of the rectangle.
QPoint handlePosition(const QRect &rect, int handle);

// Which handle (or Inside) is under pos, NoHandle if none.
int hitTest(const QRect &rect, const QPoint &pos, int radius);

// Moves the edges named by handle to pos. When an edge is dragged across its opposite edge
// the rectangle flips and handle is updated to keep following the pointer.
QRect resize(const QRect &rect, int &handle, const QPoint &pos);

// Shifts rect so it lies inside bounds, shrinking it only when it is larger than bounds.
QRect clampInto(const QRect &rect, const QRect &bounds);

QPoint clampPoint(const QPoint &pos, const QRect &bounds);

// Where to put a box of the given size next to anchor: centred below it, else above it,
// else inside its bottom edge; always kept within bounds.
QRect placeBeside(const QRect &anchor, const QSize &size, const QRect &bounds, int gap);

// The top-left that keeps anchor over the same image point when a window at topLeft is scaled by factor.
QPoint scaleAround(const QPoint &topLeft, const QPoint &anchor, qreal factor);

// The preset zoom level `steps` notches above (positive) or below (negative) zoom, within 10%-800%.
qreal stepZoom(qreal zoom, int steps);

}
