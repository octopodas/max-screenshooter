#pragma once

#include <QImage>
#include <QTimer>
#include <QWidget>

class Settings;

// A capture floating above other windows, for reference while working. Drag to move, wheel to
// zoom, Ctrl+wheel for opacity, double-click or Esc to close, right-click for more.
class PinWindow : public QWidget
{
    Q_OBJECT

public:
    // position: where the captured area was on the desktop, so the pin appears right on top of it.
    PinWindow(const QImage &image, const QPoint &position, Settings &settings);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    // anchor, in desktop coordinates, stays over the same image point.
    void setZoom(qreal zoom, const QPoint &anchor);
    void setOpacity(qreal opacity);
    void showHint(const QString &text);
    void copy();
    void saveAs();

    const QImage m_image;
    Settings &m_settings;
    qreal m_zoom = 1.0;
    bool m_pressed = false;
    bool m_movingByHand = false; // the window manager could not take over the drag
    QPoint m_pressPos;
    QPoint m_dragOffset;
    int m_wheelDelta = 0;
    QString m_hint;
    QTimer m_hintTimer;
};
