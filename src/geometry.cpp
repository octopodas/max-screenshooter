#include "geometry.h"

#include <algorithm>

namespace Geometry
{

const QVector<int> &resizeHandles()
{
    static const QVector<int> handles = {
        Top | Left, Top | Right, Bottom | Right, Bottom | Left, Top, Right, Bottom, Left,
    };
    return handles;
}

QRect rectFromPoints(const QPoint &a, const QPoint &b)
{
    return QRect(QPoint(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                 QPoint(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
}

QPoint handlePosition(const QRect &rect, int handle)
{
    // The boundary after the last pixel, so handles sit on the drawn border.
    const int right = rect.left() + rect.width();
    const int bottom = rect.top() + rect.height();
    const int x = (handle & Left) ? rect.left() : (handle & Right) ? right : (rect.left() + right) / 2;
    const int y = (handle & Top) ? rect.top() : (handle & Bottom) ? bottom : (rect.top() + bottom) / 2;
    return QPoint(x, y);
}

int hitTest(const QRect &rect, const QPoint &pos, int radius)
{
    if (rect.isNull()) {
        return NoHandle;
    }
    for (int handle : resizeHandles()) {
        const QPoint d = pos - handlePosition(rect, handle);
        if (d.x() * d.x() + d.y() * d.y() <= radius * radius) {
            return handle;
        }
    }
    return rect.contains(pos) ? Inside : NoHandle;
}

QRect resize(const QRect &rect, int &handle, const QPoint &pos)
{
    int x1 = rect.left();
    int y1 = rect.top();
    int x2 = rect.right();
    int y2 = rect.bottom();
    if (handle & Left) {
        x1 = pos.x();
    } else if (handle & Right) {
        x2 = pos.x();
    }
    if (handle & Top) {
        y1 = pos.y();
    } else if (handle & Bottom) {
        y2 = pos.y();
    }
    if (x1 > x2) {
        std::swap(x1, x2);
        if (handle & (Left | Right)) {
            handle ^= Left | Right;
        }
    }
    if (y1 > y2) {
        std::swap(y1, y2);
        if (handle & (Top | Bottom)) {
            handle ^= Top | Bottom;
        }
    }
    return QRect(QPoint(x1, y1), QPoint(x2, y2));
}

QRect clampInto(const QRect &rect, const QRect &bounds)
{
    QRect r(rect.topLeft(), rect.size().boundedTo(bounds.size()));
    r.moveLeft(std::clamp(r.left(), bounds.left(), bounds.right() - r.width() + 1));
    r.moveTop(std::clamp(r.top(), bounds.top(), bounds.bottom() - r.height() + 1));
    return r;
}

QPoint clampPoint(const QPoint &pos, const QRect &bounds)
{
    return QPoint(std::clamp(pos.x(), bounds.left(), bounds.right()),
                  std::clamp(pos.y(), bounds.top(), bounds.bottom()));
}

QRect placeBeside(const QRect &anchor, const QSize &size, const QRect &bounds, int gap)
{
    QRect r(QPoint(anchor.center().x() - size.width() / 2, 0), size);
    if (anchor.bottom() + gap + size.height() <= bounds.bottom()) {
        r.moveTop(anchor.bottom() + 1 + gap);
    } else if (anchor.top() - gap - size.height() >= bounds.top()) {
        r.moveBottom(anchor.top() - 1 - gap);
    } else {
        r.moveBottom(anchor.bottom() - gap);
    }
    return clampInto(r, bounds);
}

}
