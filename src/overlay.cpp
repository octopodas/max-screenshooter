#include "overlay.h"

#include "geometry.h"
#include "output.h"
#include "settings.h"
#include "toolbar.h"

#include <QCursor>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QToolButton>
#include <QWindow>

#include <algorithm>
#include <cstdlib>

namespace
{

constexpr int kHandleRadius = 5;
constexpr int kHandleHitRadius = 10;
constexpr int kDragThreshold = 4;
constexpr int kLoupeCells = 17; // odd, so the pixel under the cursor is in the middle
constexpr int kLoupeMinZoom = 8;
constexpr int kLoupeOffset = 24;
constexpr int kEdgeMargin = 8;
constexpr int kMaxGrabAttempts = 100;
const QColor kDim(0, 0, 0, 120);
const QColor kHoverDim(0, 0, 0, 45);
const QColor kLabelBackground(0, 0, 0, 190);

Qt::CursorShape cursorForHandle(int handle)
{
    using namespace Geometry;
    switch (handle) {
    case Top | Left:
    case Bottom | Right:
        return Qt::SizeFDiagCursor;
    case Top | Right:
    case Bottom | Left:
        return Qt::SizeBDiagCursor;
    case Left:
    case Right:
        return Qt::SizeHorCursor;
    case Top:
    case Bottom:
        return Qt::SizeVerCursor;
    case Inside:
        return Qt::SizeAllCursor;
    default:
        return Qt::CrossCursor;
    }
}

}

Overlay::Overlay(const Capture &capture, Settings &settings)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint)
    , m_capture(capture)
    , m_settings(settings)
    , m_composite(capture.image)
    , m_size(std::clamp(settings.annotationSize(), 0, Annotations::kSizeCount - 1))
    , m_penColor(settings.penColor())
    , m_highlighterColor(settings.highlighterColor())
    , m_toolbar(new Toolbar(this))
    , m_stylePanel(new StylePanel(this))
{
    // Bypassing the window manager keeps panels from covering the overlay and skips open animations.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setGeometry(capture.geometry);

    const QRect last = settings.lastRegion(capture.screenName);
    if (rect().contains(last) && last != rect()) {
        m_selection = last;
    }

    connect(m_toolbar, &Toolbar::toolChosen, this, &Overlay::setTool);
    connect(m_toolbar, &Toolbar::styleRequested, this, &Overlay::toggleStylePanel);
    connect(m_toolbar, &Toolbar::undoRequested, this, &Overlay::undo);
    connect(m_toolbar, &Toolbar::redoRequested, this, &Overlay::redo);
    connect(m_toolbar, &Toolbar::expandRequested, this, &Overlay::expand);
    // Queued, so the overlay is not hidden while the clicked button is still handling its event.
    connect(m_toolbar, &Toolbar::saveRequested, this, &Overlay::saveAs, Qt::QueuedConnection);
    connect(m_toolbar, &Toolbar::copyRequested, this, &Overlay::accept, Qt::QueuedConnection);
    connect(m_toolbar, &Toolbar::cancelRequested, this, &Overlay::cancel, Qt::QueuedConnection);
    connect(m_toolbar, &Toolbar::copyTextRequested, this, &Overlay::copyText, Qt::QueuedConnection);
    connect(m_toolbar, &Toolbar::pinRequested, this, &Overlay::pin, Qt::QueuedConnection);
    connect(m_stylePanel, &StylePanel::colorChosen, this, [this](const QColor &color) {
        setColor(color);
        m_stylePanel->hide();
    });
    connect(m_stylePanel, &StylePanel::sizeChosen, this, [this](int size) {
        setSize(size);
        m_stylePanel->hide();
    });
    m_toolbar->installEventFilter(this);
    m_stylePanel->installEventFilter(this);

    m_grabTimer.setInterval(50);
    connect(&m_grabTimer, &QTimer::timeout, this, &Overlay::tryGrabKeyboard);

    m_toolbar->setTool(m_tool);
    m_toolbar->setStyle(currentColor(), m_size);
    placeToolbar();
}

void Overlay::start()
{
    m_cursor = mapFromGlobal(QCursor::pos());
    m_pointerInside = rect().contains(m_cursor);
    updateHover();
    show();
    grabInput();
    m_chrome = chromeRegion();
}

// Selection

void Overlay::setSelection(const QRect &selection)
{
    m_selection = selection.isNull() ? QRect() : selection.intersected(rect());
    if (m_drag == Drag::None) {
        placeToolbar();
    }
    refreshChrome();
}

void Overlay::updateHover()
{
    const bool canSnap = m_tool == Tool::Select && m_drag == Drag::None && m_pointerInside && !m_selection.contains(m_cursor);
    m_hoverWindow = canSnap ? windowAt(m_cursor) : QRect();
}

QRect Overlay::windowAt(const QPoint &pos) const
{
    for (const QRect &window : m_capture.windows) {
        if (window.contains(pos)) {
            return window;
        }
    }
    return QRect();
}

// Tools and annotations

void Overlay::setTool(Tool tool)
{
    if (m_editingText) {
        commitDraft();
    }
    m_tool = tool;
    m_stylePanel->hide();
    m_toolbar->setTool(tool);
    m_toolbar->setStyle(currentColor(), m_size);
    updateHover();
    updateCursorShape();
    refreshChrome();
}

QColor Overlay::currentColor() const
{
    return m_tool == Tool::Highlighter ? m_highlighterColor : m_penColor;
}

void Overlay::setColor(const QColor &color)
{
    if (m_tool == Tool::Highlighter) {
        m_highlighterColor = color;
        m_settings.setHighlighterColor(color);
    } else {
        m_penColor = color;
        m_settings.setPenColor(color);
    }
    if (m_editingText) {
        m_draft.color = color;
    }
    m_toolbar->setStyle(color, m_size);
    refreshChrome();
}

void Overlay::setSize(int size)
{
    m_size = std::clamp(size, 0, Annotations::kSizeCount - 1);
    m_settings.setAnnotationSize(m_size);
    if (m_editingText) {
        m_draft.size = m_size;
    }
    m_toolbar->setStyle(currentColor(), m_size);
    m_stylePanel->setStyle(currentColor(), m_size);
    refreshChrome();
}

void Overlay::toggleStylePanel()
{
    if (m_stylePanel->isVisible()) {
        m_stylePanel->hide();
        return;
    }
    m_stylePanel->setStyle(currentColor(), m_size);
    m_stylePanel->adjustSize();
    const QToolButton *button = m_toolbar->styleButton();
    const QRect anchor(button->mapTo(this, QPoint(0, 0)), button->size());
    const QRect bounds = rect().adjusted(kEdgeMargin, kEdgeMargin, -kEdgeMargin, -kEdgeMargin);
    m_stylePanel->setGeometry(Geometry::placeBeside(anchor, m_stylePanel->sizeHint(), bounds, kEdgeMargin));
    m_stylePanel->show();
    m_stylePanel->raise();
}

void Overlay::beginText(const QPoint &pos)
{
    m_draft = Annotation();
    m_draft.tool = Tool::Text;
    m_draft.color = currentColor();
    m_draft.size = m_size;
    // Centre the first line on the click.
    m_draft.from = pos - QPoint(0, QFontMetrics(Annotations::font(m_size)).height() / 2);
    m_hasDraft = true;
    m_editingText = true;
}

bool Overlay::handleTextKey(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        discardDraft();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (event->modifiers() & Qt::ShiftModifier) {
            m_draft.text += QLatin1Char('\n');
            refreshChrome();
        } else {
            commitDraft();
        }
        return true;
    case Qt::Key_Backspace:
        m_draft.text.chop(1);
        refreshChrome();
        return true;
    default:
        break;
    }
    if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) {
        // Finish the text, then let the shortcut (Ctrl+C, Ctrl+Z...) act on it.
        commitDraft();
        return false;
    }
    const QString text = event->text();
    if (!text.isEmpty() && text.at(0).isPrint()) {
        m_draft.text += text;
        refreshChrome();
    }
    return true;
}

void Overlay::commitDraft()
{
    const bool keep = m_hasDraft && m_draft.isMeaningful();
    m_hasDraft = false;
    m_editingText = false;
    if (keep) {
        m_annotations.append(m_draft);
        m_redo.clear();
        Annotations::render(m_composite, {m_draft});
        m_toolbar->setHistoryState(true, false);
        update(m_draft.bounds());
    }
    refreshChrome();
}

void Overlay::discardDraft()
{
    m_hasDraft = false;
    m_editingText = false;
    refreshChrome();
}

void Overlay::undo()
{
    if (m_editingText) {
        discardDraft();
        return;
    }
    if (!m_annotations.isEmpty()) {
        m_redo.append(m_annotations.takeLast());
        rebuildComposite();
    }
}

void Overlay::redo()
{
    if (!m_redo.isEmpty()) {
        m_annotations.append(m_redo.takeLast());
        rebuildComposite();
    }
}

void Overlay::rebuildComposite()
{
    m_composite = m_capture.image;
    Annotations::render(m_composite, m_annotations);
    m_toolbar->setHistoryState(!m_annotations.isEmpty(), !m_redo.isEmpty());
    update();
}

// Finishing

void Overlay::accept()
{
    if (m_editingText) {
        commitDraft();
    }
    const QImage image = result();
    rememberRegion();
    finish();
    Q_EMIT accepted(image, QString());
}

void Overlay::saveAs()
{
    if (m_editingText) {
        commitDraft();
    }
    const QImage image = result();
    // The dialog is a normal window and could not appear above an override-redirect overlay.
    finish();
    const QString path = Output::askSavePath(m_settings, nullptr);
    if (path.isEmpty()) {
        show();
        grabInput();
        return;
    }
    rememberRegion();
    Q_EMIT accepted(image, path);
}

void Overlay::copyText()
{
    if (m_editingText) {
        commitDraft();
    }
    // Recognize the bare capture: arrows and highlights would only confuse the OCR.
    const QImage image = m_capture.image.copy(area());
    rememberRegion();
    finish();
    Q_EMIT textRequested(image);
}

void Overlay::pin()
{
    if (m_editingText) {
        commitDraft();
    }
    const QImage image = result();
    const QPoint position = m_capture.geometry.topLeft() + area().topLeft();
    rememberRegion();
    finish();
    Q_EMIT pinned(image, position);
}

void Overlay::cancel()
{
    finish();
    Q_EMIT canceled();
}

void Overlay::expand()
{
    setSelection(rect());
}

QRect Overlay::area() const
{
    return m_selection.isNull() ? rect() : m_selection;
}

QImage Overlay::result() const
{
    return m_composite.copy(area());
}

void Overlay::rememberRegion()
{
    // A remembered whole-monitor selection would leave no outside area to start a new drag from.
    if (!m_selection.isNull() && m_selection != rect()) {
        m_settings.setLastRegion(m_capture.screenName, m_selection);
    }
}

void Overlay::finish()
{
    m_grabTimer.stop();
    releaseKeyboard();
    hide();
}

// Input

void Overlay::grabInput()
{
    raise();
    // Qt sets the input focus directly for override-redirect windows.
    activateWindow();
    // Routes keys to the overlay even when Qt thinks a child should get them.
    grabKeyboard();
    m_grabAttempts = 0;
    tryGrabKeyboard();
}

void Overlay::tryGrabKeyboard()
{
    // The X grab fails while the window is not mapped yet or kglobalaccel still holds the Print key.
    if (windowHandle() && windowHandle()->setKeyboardGrabEnabled(true)) {
        m_grabTimer.stop();
        activateWindow();
        return;
    }
    if (++m_grabAttempts >= kMaxGrabAttempts) {
        m_grabTimer.stop();
        qWarning("max-screenshooter: could not grab the keyboard");
        return;
    }
    if (!m_grabTimer.isActive()) {
        m_grabTimer.start();
    }
}

void Overlay::nudge(const QPoint &delta, bool resize)
{
    if (m_drag != Drag::None || m_selection.isNull()) {
        // Pixel-precise pointer movement, for example while dragging an edge with the loupe open.
        QCursor::setPos(QCursor::pos() + delta);
        return;
    }
    if (resize) {
        QRect r = m_selection;
        r.setRight(std::max(r.left(), r.right() + delta.x()));
        r.setBottom(std::max(r.top(), r.bottom() + delta.y()));
        setSelection(r);
    } else {
        setSelection(Geometry::clampInto(m_selection.translated(delta), rect()));
    }
}

bool Overlay::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Enter && (watched == m_toolbar || watched == m_stylePanel)) {
        m_pointerInside = false;
        updateHover();
        refreshChrome();
    }
    return QWidget::eventFilter(watched, event);
}

void Overlay::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = Geometry::clampPoint(event->pos(), rect());
    m_cursor = pos;
    m_stylePanel->hide();

    if (event->button() == Qt::RightButton) {
        if (m_drag != Drag::None) {
            return;
        }
        if (m_hasDraft) {
            discardDraft();
        } else if (!m_selection.isNull()) {
            setSelection(QRect());
        } else {
            cancel();
        }
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (m_editingText) {
        commitDraft();
    }

    switch (m_tool) {
    case Tool::Select: {
        const int hit = Geometry::hitTest(m_selection, pos, kHandleHitRadius);
        if (hit == Geometry::Inside) {
            m_drag = Drag::Moving;
            m_moveOffset = pos - m_selection.topLeft();
        } else if (hit != Geometry::NoHandle) {
            m_drag = Drag::Resizing;
            m_handle = hit;
        } else {
            m_drag = Drag::Pending;
            m_pressPos = pos;
            return;
        }
        m_hoverWindow = QRect();
        m_toolbar->hide();
        break;
    }
    case Tool::Text:
        beginText(pos);
        break;
    case Tool::Arrow:
    case Tool::Rectangle:
    case Tool::Highlighter:
    case Tool::Pixelate:
        m_draft = Annotation();
        m_draft.tool = m_tool;
        m_draft.color = currentColor();
        m_draft.size = m_size;
        m_draft.from = pos;
        m_draft.to = pos;
        m_draft.points = {pos};
        m_hasDraft = true;
        m_drag = Drag::Drawing;
        break;
    }
    updateCursorShape();
    refreshChrome();
}

void Overlay::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = Geometry::clampPoint(event->pos(), rect());
    m_cursor = pos;
    m_pointerInside = true;

    switch (m_drag) {
    case Drag::Pending:
        if ((pos - m_pressPos).manhattanLength() < kDragThreshold) {
            break;
        }
        m_drag = Drag::Creating;
        m_hoverWindow = QRect();
        m_toolbar->hide();
        Q_FALLTHROUGH();
    case Drag::Creating:
        setSelection(Geometry::rectFromPoints(m_pressPos, pos));
        break;
    case Drag::Moving:
        setSelection(Geometry::clampInto(QRect(pos - m_moveOffset, m_selection.size()), rect()));
        break;
    case Drag::Resizing:
        setSelection(Geometry::resize(m_selection, m_handle, pos));
        break;
    case Drag::Drawing:
        if (m_draft.tool == Tool::Highlighter) {
            m_draft.points.append(pos);
        } else {
            m_draft.to = pos;
        }
        break;
    case Drag::None:
        updateHover();
        break;
    }
    updateCursorShape();
    refreshChrome();
}

void Overlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_cursor = Geometry::clampPoint(event->pos(), rect());
    const Drag drag = m_drag;
    m_drag = Drag::None;
    if (drag == Drag::Pending) {
        const QRect window = windowAt(m_cursor);
        if (!window.isNull()) {
            m_selection = window;
        }
    } else if (drag == Drag::Drawing) {
        commitDraft();
    }
    updateHover();
    placeToolbar();
    updateCursorShape();
    refreshChrome();
}

void Overlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    // A double-click never draws anything worth keeping, so it copies with every tool but text,
    // where clicks place the cursor. With the select tool, the first click of a double-click on
    // a window has already snapped the selection to it.
    if (event->button() == Qt::LeftButton && m_tool != Tool::Text && m_selection.contains(event->pos())) {
        accept();
        return;
    }
    mousePressEvent(event);
}

void Overlay::wheelEvent(QWheelEvent *event)
{
    if (m_tool == Tool::Select) {
        return;
    }
    m_wheelDelta += event->angleDelta().y();
    if (std::abs(m_wheelDelta) >= 120) {
        setSize(m_size + (m_wheelDelta > 0 ? 1 : -1));
        m_wheelDelta = 0;
    }
}

void Overlay::keyPressEvent(QKeyEvent *event)
{
    if (m_editingText && handleTextKey(event)) {
        return;
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    const bool ctrl = modifiers & Qt::ControlModifier;
    const bool shift = modifiers & Qt::ShiftModifier;
    const int step = ctrl ? 10 : 1;

    switch (event->key()) {
    case Qt::Key_Escape:
        cancel();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        accept();
        return;
    case Qt::Key_Left:
        nudge(QPoint(-step, 0), shift);
        return;
    case Qt::Key_Right:
        nudge(QPoint(step, 0), shift);
        return;
    case Qt::Key_Up:
        nudge(QPoint(0, -step), shift);
        return;
    case Qt::Key_Down:
        nudge(QPoint(0, step), shift);
        return;
    default:
        break;
    }

    if (modifiers == Qt::ControlModifier || modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
        switch (event->key()) {
        case Qt::Key_C:
            accept();
            return;
        case Qt::Key_S:
            saveAs();
            return;
        case Qt::Key_T:
            copyText();
            return;
        case Qt::Key_P:
            pin();
            return;
        case Qt::Key_Z:
            shift ? redo() : undo();
            return;
        case Qt::Key_Y:
            redo();
            return;
        default:
            return;
        }
    }
    if (modifiers != Qt::NoModifier) {
        return;
    }
    switch (event->key()) {
    case Qt::Key_V:
        setTool(Tool::Select);
        break;
    case Qt::Key_A:
        setTool(Tool::Arrow);
        break;
    case Qt::Key_R:
        setTool(Tool::Rectangle);
        break;
    case Qt::Key_T:
        setTool(Tool::Text);
        break;
    case Qt::Key_H:
        setTool(Tool::Highlighter);
        break;
    case Qt::Key_P:
        setTool(Tool::Pixelate);
        break;
    case Qt::Key_F:
        expand();
        break;
    default:
        break;
    }
}

void Overlay::leaveEvent(QEvent *)
{
    m_pointerInside = false;
    updateHover();
    refreshChrome();
}

// Painting

void Overlay::refreshChrome()
{
    // Only repaint what changed: on a 4K monitor a full repaint per mouse move is noticeable.
    const QRegion now = chromeRegion();
    update(now.united(m_chrome));
    m_chrome = now;
}

QRegion Overlay::chromeRegion() const
{
    QRegion region;
    if (!m_selection.isNull()) {
        const int margin = kHandleRadius + 3;
        region += m_selection.adjusted(-margin, -margin, margin, margin);
        region += sizeLabelRect();
    }
    if (!m_hoverWindow.isNull()) {
        region += m_hoverWindow;
    }
    if (m_hasDraft) {
        region += m_draft.bounds().adjusted(-6, -4, 6, 4);
    }
    if (loupeVisible()) {
        region += loupeRect().adjusted(-1, -1, 1, 1);
    }
    return region;
}

bool Overlay::loupeVisible() const
{
    if (m_tool != Tool::Select || !m_pointerInside) {
        return false;
    }
    switch (m_drag) {
    case Drag::Creating:
    case Drag::Resizing:
        return true;
    case Drag::None:
    case Drag::Pending:
        return !m_selection.contains(m_cursor);
    case Drag::Moving:
    case Drag::Drawing:
        break;
    }
    return false;
}

Overlay::LoupeLayout Overlay::loupeLayout() const
{
    // Grow the zoom until the widest label fits underneath; fonts are large on high-DPI setups.
    const QFontMetrics metrics = fontMetrics();
    const int labelWidth = metrics.horizontalAdvance(QStringLiteral("8888 × 8888")) + 12;
    const int zoom = std::max(kLoupeMinZoom, (labelWidth + kLoupeCells - 1) / kLoupeCells);
    return {zoom, zoom * kLoupeCells, metrics.height() + 6};
}

QRect Overlay::loupeRect() const
{
    const LoupeLayout layout = loupeLayout();
    const QSize size(layout.side, layout.side + layout.labelHeight);
    QPoint topLeft = m_cursor + QPoint(kLoupeOffset, kLoupeOffset);
    if (topLeft.x() + size.width() > width()) {
        topLeft.setX(m_cursor.x() - kLoupeOffset - size.width());
    }
    if (topLeft.y() + size.height() > height()) {
        topLeft.setY(m_cursor.y() - kLoupeOffset - size.height());
    }
    return QRect(topLeft, size);
}

QString Overlay::sizeText() const
{
    return QStringLiteral("%1 × %2").arg(m_selection.width()).arg(m_selection.height());
}

QRect Overlay::sizeLabelRect() const
{
    const QSize size = fontMetrics().size(Qt::TextSingleLine, sizeText()) + QSize(12, 6);
    QRect label(m_selection.topLeft() - QPoint(0, size.height() + 4), size);
    if (label.top() < 0 || (m_toolbar->isVisible() && label.intersects(m_toolbar->geometry()))) {
        label.moveTopLeft(m_selection.topLeft() + QPoint(6, 6));
    }
    return Geometry::clampInto(label, rect());
}

void Overlay::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    const QRegion dirty = event->region();
    p.setClipRegion(dirty);
    p.drawImage(0, 0, m_composite);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    if (m_hasDraft) {
        paintDraft(p);
    }

    // Dim everything except the selection. The window a click would snap to is only dimmed
    // lightly, so the screen still looks frozen when a maximized window is under the cursor.
    QRegion lit;
    if (!m_selection.isNull()) {
        lit = m_selection;
    } else if (!m_hoverWindow.isNull()) {
        lit = m_hoverWindow;
        p.setClipRegion(dirty.intersected(lit));
        p.fillRect(rect(), kHoverDim);
    }
    p.setClipRegion(dirty.subtracted(lit));
    p.fillRect(rect(), kDim);
    p.setClipRegion(dirty);

    paintChrome(p);
    if (loupeVisible()) {
        paintLoupe(p);
    }
}

void Overlay::paintDraft(QPainter &p) const
{
    if (m_draft.tool != Tool::Pixelate) {
        Annotations::paint(p, m_draft);
        return;
    }
    const QRect area = Geometry::rectFromPoints(m_draft.from, m_draft.to).intersected(rect());
    QImage preview = m_composite.copy(area);
    Annotations::pixelate(preview, preview.rect(), Annotations::pixelBlock(m_draft.size));
    p.drawImage(area.topLeft(), preview);
}

void Overlay::paintChrome(QPainter &p) const
{
    const QColor accent = palette().color(QPalette::Highlight);
    p.setBrush(Qt::NoBrush);

    if (!m_hoverWindow.isNull()) {
        p.setPen(QPen(accent, 2, m_selection.isNull() ? Qt::SolidLine : Qt::DashLine));
        p.drawRect(QRectF(m_hoverWindow).adjusted(1, 1, -1, -1));
    }

    if (m_editingText) {
        const QFontMetrics metrics(Annotations::font(m_draft.size));
        const QStringList lines = m_draft.text.split(QLatin1Char('\n'));
        const int x = m_draft.from.x() + metrics.horizontalAdvance(lines.constLast());
        const int y = m_draft.from.y() + (lines.size() - 1) * metrics.lineSpacing();
        p.fillRect(QRect(x + 1, y, 2, metrics.height()), m_draft.color);
        p.setPen(QPen(accent, 1, Qt::DashLine));
        p.drawRect(Annotations::textRect(m_draft).adjusted(-3, -2, 3, 2));
    }

    if (m_selection.isNull()) {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(accent, 1));
    p.drawRect(m_selection.adjusted(-1, -1, 0, 0)); // just outside the selected pixels
    p.restore();

    if (m_tool == Tool::Select && m_drag != Drag::Moving) {
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(accent);
        for (int handle : Geometry::resizeHandles()) {
            p.drawEllipse(QPointF(Geometry::handlePosition(m_selection, handle)), kHandleRadius, kHandleRadius);
        }
    }

    const QRect label = sizeLabelRect();
    p.setPen(Qt::NoPen);
    p.setBrush(kLabelBackground);
    p.drawRoundedRect(label, 4, 4);
    p.setPen(Qt::white);
    p.drawText(label, Qt::AlignCenter, sizeText());
}

void Overlay::paintLoupe(QPainter &p) const
{
    const QRect box = loupeRect();
    const LoupeLayout layout = loupeLayout();
    const int half = kLoupeCells / 2;
    const QRect source(m_cursor - QPoint(half, half), QSize(kLoupeCells, kLoupeCells));
    QImage cells(kLoupeCells, kLoupeCells, QImage::Format_RGB32);
    cells.fill(Qt::black);
    {
        QPainter cellPainter(&cells);
        const QRect valid = source.intersected(m_composite.rect());
        cellPainter.drawImage(valid.topLeft() - source.topLeft(), m_composite, valid);
    }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false); // keep pixels square
    const QRect zoomed(box.topLeft(), QSize(layout.side, layout.side));
    p.drawImage(zoomed, cells);

    const QRect centre(zoomed.topLeft() + QPoint(half, half) * layout.zoom, QSize(layout.zoom, layout.zoom));
    p.setBrush(Qt::NoBrush);
    p.setPen(Qt::black);
    p.drawRect(centre.adjusted(-1, -1, 0, 0));
    p.setPen(Qt::white);
    p.drawRect(centre.adjusted(-2, -2, 1, 1));

    const QRect label(zoomed.bottomLeft() + QPoint(0, 1), QSize(layout.side, layout.labelHeight - 1));
    p.fillRect(label, kLabelBackground);
    p.setPen(Qt::white);
    const bool sizing = m_drag == Drag::Creating || m_drag == Drag::Resizing;
    p.drawText(label, Qt::AlignCenter, sizing ? sizeText() : QStringLiteral("%1, %2").arg(m_cursor.x()).arg(m_cursor.y()));

    p.setPen(palette().color(QPalette::Highlight));
    p.drawRect(box.adjusted(0, 0, -1, -1));
    p.restore();
}

void Overlay::placeToolbar()
{
    const int margin = kHandleRadius + 4;
    const QRect anchor = m_selection.isNull() ? rect() : m_selection.adjusted(-margin, -margin, margin, margin);
    const QRect bounds = rect().adjusted(kEdgeMargin, kEdgeMargin, -kEdgeMargin, -kEdgeMargin);
    m_toolbar->setGeometry(Geometry::placeBeside(anchor, m_toolbar->sizeHint(), bounds, kEdgeMargin));
    m_toolbar->show();
    m_toolbar->raise();
}

void Overlay::updateCursorShape()
{
    Qt::CursorShape shape = Qt::CrossCursor;
    if (m_tool == Tool::Text) {
        shape = Qt::IBeamCursor;
    } else if (m_tool == Tool::Select) {
        switch (m_drag) {
        case Drag::None:
            shape = cursorForHandle(Geometry::hitTest(m_selection, m_cursor, kHandleHitRadius));
            break;
        case Drag::Moving:
            shape = cursorForHandle(Geometry::Inside);
            break;
        case Drag::Resizing:
            shape = cursorForHandle(m_handle);
            break;
        default:
            break;
        }
    }
    if (cursor().shape() != shape) {
        setCursor(shape);
    }
}
