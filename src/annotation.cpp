#include "annotation.h"

#include "geometry.h"

#include <QFontMetrics>
#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <array>

namespace
{

constexpr std::array<int, Annotations::kSizeCount> kPenWidths = {2, 4, 7};
constexpr std::array<int, Annotations::kSizeCount> kHighlighterWidths = {12, 20, 32};
constexpr std::array<int, Annotations::kSizeCount> kPixelBlocks = {6, 10, 16};
constexpr std::array<int, Annotations::kSizeCount> kFontSizes = {20, 32, 48};
constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignTop | Qt::TextExpandTabs;
const QSize kUnboundedText(1 << 20, 1 << 20);

int clampSize(int size)
{
    return std::clamp(size, 0, Annotations::kSizeCount - 1);
}

qreal arrowHeadLength(const Annotation &a)
{
    return 4.0 * Annotations::penWidth(a.size) + 10.0;
}

void paintArrow(QPainter &p, const Annotation &a)
{
    const QLineF line(a.from, a.to);
    const qreal length = line.length();
    if (length < 1.0) {
        return;
    }
    const qreal width = Annotations::penWidth(a.size);
    const qreal head = std::min(length, arrowHeadLength(a));
    const QPointF dir = (line.p2() - line.p1()) / length;
    const QPointF normal(-dir.y(), dir.x());
    const QPointF base = line.p2() - dir * head;

    // The shaft stops inside the head so its round cap never pokes out past the tip.
    p.setPen(QPen(a.color, width, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(line.p1(), base + dir * (head * 0.3));
    p.setPen(Qt::NoPen);
    p.setBrush(a.color);
    p.drawPolygon(QPolygonF(QVector<QPointF>{line.p2(), base + normal * head * 0.45, base - normal * head * 0.45}));
}

void paintHighlighter(QPainter &p, const Annotation &a)
{
    if (a.points.size() < 2) {
        return;
    }
    QPainterPath path(a.points.first());
    for (int i = 1; i < a.points.size(); ++i) {
        path.lineTo(a.points[i]);
    }
    // Multiply keeps dark text readable under the marker, and one path never darkens where it overlaps itself.
    p.setCompositionMode(QPainter::CompositionMode_Multiply);
    p.setPen(QPen(a.color, Annotations::highlighterWidth(a.size), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

}

QRect Annotation::bounds() const
{
    switch (tool) {
    case Tool::Arrow: {
        const int margin = int(arrowHeadLength(*this)) + Annotations::penWidth(size);
        return Geometry::rectFromPoints(from, to).adjusted(-margin, -margin, margin, margin);
    }
    case Tool::Rectangle: {
        const int margin = Annotations::penWidth(size);
        return Geometry::rectFromPoints(from, to).adjusted(-margin, -margin, margin, margin);
    }
    case Tool::Pixelate:
        return Geometry::rectFromPoints(from, to);
    case Tool::Highlighter: {
        if (points.isEmpty()) {
            return QRect();
        }
        QRect r(points.first(), QSize(1, 1));
        for (const QPoint &p : points) {
            r = r.united(QRect(p, QSize(1, 1)));
        }
        const int margin = Annotations::highlighterWidth(size);
        return r.adjusted(-margin, -margin, margin, margin);
    }
    case Tool::Text:
        return Annotations::textRect(*this).adjusted(-2, -2, 4, 2);
    case Tool::Select:
        break;
    }
    return QRect();
}

bool Annotation::isMeaningful() const
{
    switch (tool) {
    case Tool::Arrow:
        return (to - from).manhattanLength() >= 4;
    case Tool::Rectangle:
    case Tool::Pixelate: {
        const QRect r = Geometry::rectFromPoints(from, to);
        return r.width() >= 3 && r.height() >= 3;
    }
    case Tool::Highlighter:
        return points.size() >= 2;
    case Tool::Text:
        return !text.trimmed().isEmpty();
    case Tool::Select:
        break;
    }
    return false;
}

namespace Annotations
{

int penWidth(int size)
{
    return kPenWidths[clampSize(size)];
}

int highlighterWidth(int size)
{
    return kHighlighterWidths[clampSize(size)];
}

int pixelBlock(int size)
{
    return kPixelBlocks[clampSize(size)];
}

QFont font(int size)
{
    QFont f;
    f.setPixelSize(kFontSizes[clampSize(size)]);
    f.setWeight(QFont::DemiBold);
    return f;
}

QRect textRect(const Annotation &a)
{
    // An empty text still occupies one line so the caret has somewhere to go.
    const QString text = a.text.isEmpty() ? QStringLiteral(" ") : a.text;
    return QFontMetrics(font(a.size)).boundingRect(QRect(a.from, kUnboundedText), kTextFlags, text);
}

void paint(QPainter &p, const Annotation &a)
{
    p.save();
    switch (a.tool) {
    case Tool::Arrow:
        paintArrow(p, a);
        break;
    case Tool::Rectangle:
        p.setPen(QPen(a.color, penWidth(a.size), Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(Geometry::rectFromPoints(a.from, a.to)));
        break;
    case Tool::Highlighter:
        paintHighlighter(p, a);
        break;
    case Tool::Text:
        p.setFont(font(a.size));
        p.setPen(a.color);
        p.drawText(QRect(a.from, kUnboundedText), kTextFlags, a.text);
        break;
    case Tool::Pixelate:
    case Tool::Select:
        break;
    }
    p.restore();
}

void pixelate(QImage &image, const QRect &rect, int block)
{
    const QRect r = rect & image.rect();
    if (r.isEmpty() || block < 2) {
        return;
    }
    const QSize cells((r.width() + block - 1) / block, (r.height() + block - 1) / block);
    // Smooth downscaling averages each cell's pixels; nearest-neighbour upscaling turns them back into blocks.
    const QImage averaged = image.copy(r).scaled(cells, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const QImage blocks = averaged.scaled(r.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    QPainter p(&image);
    p.drawImage(r.topLeft(), blocks);
}

void render(QImage &image, const QVector<Annotation> &annotations)
{
    for (const Annotation &a : annotations) {
        if (a.tool == Tool::Pixelate) {
            pixelate(image, Geometry::rectFromPoints(a.from, a.to), pixelBlock(a.size));
            continue;
        }
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        paint(p, a);
    }
}

}
