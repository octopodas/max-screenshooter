#include "pinwindow.h"

#include "geometry.h"
#include "output.h"
#include "settings.h"

#include <KWindowSystem>

#include <QApplication>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#include <algorithm>

namespace
{

constexpr qreal kMinOpacity = 0.2;
constexpr int kMinSide = 16;
const QColor kHintBackground(0, 0, 0, 190);

}

PinWindow::PinWindow(const QImage &image, const QPoint &position, Settings &settings)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_image(image)
    , m_settings(settings)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("Pinned screenshot"));
    setCursor(Qt::SizeAllCursor);
    setGeometry(QRect(position, image.size()));

    m_hintTimer.setSingleShot(true);
    m_hintTimer.setInterval(900);
    connect(&m_hintTimer, &QTimer::timeout, this, [this] {
        m_hint.clear();
        update();
    });

    // A whole-monitor pin at full size would cover everything; start it at half size instead.
    if (const QScreen *screen = QGuiApplication::screenAt(geometry().center())) {
        const QSize monitor = screen->geometry().size();
        if (image.width() * 10 >= monitor.width() * 9 && image.height() * 10 >= monitor.height() * 9) {
            setZoom(0.5, screen->geometry().center());
        }
    }

    // Pins are reference material, not tasks: keep them out of the taskbar, pager and Alt+Tab.
    KWindowSystem::setState(winId(), NET::SkipTaskbar | NET::SkipPager | NET::SkipSwitcher);
}

void PinWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    // Smooth when shrunk; enlarged screenshots read better as crisp pixels.
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
    p.drawImage(rect(), m_image);
    p.setPen(palette().color(QPalette::Highlight));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    if (!m_hint.isEmpty()) {
        QRect box = fontMetrics().boundingRect(m_hint).adjusted(-8, -4, 8, 4);
        box.moveCenter(rect().center());
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(kHintBackground);
        p.drawRoundedRect(box, 4, 4);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, m_hint);
    }
}

void PinWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_movingByHand = false;
        m_pressPos = event->globalPos();
        m_dragOffset = event->globalPos() - geometry().topLeft();
    }
}

void PinWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed || !(event->buttons() & Qt::LeftButton)) {
        return;
    }
    if (!m_movingByHand) {
        // Only hand over once it is clearly a drag, so plain clicks still make up a double-click.
        if ((event->globalPos() - m_pressPos).manhattanLength() < QApplication::startDragDistance()) {
            return;
        }
        // KWin moves the window itself, with its usual edge snapping.
        if (windowHandle() && windowHandle()->startSystemMove()) {
            m_pressed = false;
            return;
        }
        m_movingByHand = true;
    }
    move(event->globalPos() - m_dragOffset);
}

void PinWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        m_movingByHand = false;
    }
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        close();
    }
}

void PinWindow::wheelEvent(QWheelEvent *event)
{
    m_wheelDelta += event->angleDelta().y();
    const int steps = m_wheelDelta / 120;
    if (steps == 0) {
        return;
    }
    m_wheelDelta -= steps * 120;
    if (event->modifiers() & Qt::ControlModifier) {
        setOpacity(windowOpacity() + 0.1 * steps);
    } else {
        setZoom(Geometry::stepZoom(m_zoom, steps), event->globalPosition().toPoint());
    }
}

void PinWindow::keyPressEvent(QKeyEvent *event)
{
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    switch (event->key()) {
    case Qt::Key_Escape:
        close();
        return;
    case Qt::Key_C:
        if (ctrl) {
            copy();
        }
        return;
    case Qt::Key_S:
        if (ctrl) {
            saveAs();
        }
        return;
    case Qt::Key_0:
        if (ctrl) {
            setZoom(1.0, geometry().topLeft());
        }
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        setZoom(Geometry::stepZoom(m_zoom, 1), geometry().center());
        return;
    case Qt::Key_Minus:
        setZoom(Geometry::stepZoom(m_zoom, -1), geometry().center());
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

void PinWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), QStringLiteral("Copy"), this, &PinWindow::copy, QKeySequence::Copy);
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-save-as")), QStringLiteral("Save As…"), this, &PinWindow::saveAs,
                   QKeySequence(Qt::CTRL | Qt::Key_S));
    menu.addSeparator();
    QAction *actualSize = menu.addAction(QIcon::fromTheme(QStringLiteral("zoom-original")), QStringLiteral("Actual Size"), this, [this] {
        setZoom(1.0, geometry().topLeft());
    });
    actualSize->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    actualSize->setEnabled(!qFuzzyCompare(m_zoom, 1.0));
    QMenu *opacity = menu.addMenu(QStringLiteral("Opacity"));
    for (int percent : {100, 80, 60, 40}) {
        QAction *action = opacity->addAction(QStringLiteral("%1%").arg(percent), this, [this, percent] {
            setOpacity(percent / 100.0);
        });
        action->setCheckable(true);
        action->setChecked(qRound(windowOpacity() * 100) == percent);
    }
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("window-close")), QStringLiteral("Close"), this, &QWidget::close, Qt::Key_Escape);
    menu.exec(event->globalPos());
}

void PinWindow::setZoom(qreal zoom, const QPoint &anchor)
{
    if (qFuzzyCompare(zoom, m_zoom)) {
        return;
    }
    const QSize size = (QSizeF(m_image.size()) * zoom).toSize().expandedTo(QSize(kMinSide, kMinSide));
    const QPoint topLeft = Geometry::scaleAround(geometry().topLeft(), anchor, zoom / m_zoom);
    m_zoom = zoom;
    setGeometry(QRect(topLeft, size));
    showHint(QStringLiteral("%1%").arg(qRound(zoom * 100)));
}

void PinWindow::setOpacity(qreal opacity)
{
    setWindowOpacity(std::clamp(opacity, kMinOpacity, 1.0));
    showHint(QStringLiteral("Opacity %1%").arg(qRound(windowOpacity() * 100)));
}

void PinWindow::showHint(const QString &text)
{
    m_hint = text;
    m_hintTimer.start();
    update();
}

void PinWindow::copy()
{
    Output::copyToClipboard(m_image);
    showHint(QStringLiteral("Copied"));
}

void PinWindow::saveAs()
{
    const QString path = Output::askSavePath(m_settings, this);
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (Output::writeImage(m_image, path, &error)) {
        showHint(QStringLiteral("Saved"));
    } else {
        QMessageBox::warning(this, QGuiApplication::applicationDisplayName(), QStringLiteral("Could not save %1: %2").arg(path, error));
    }
    m_settings.sync();
}
