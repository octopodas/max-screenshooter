#pragma once

#include "annotation.h"
#include "capture.h"

#include <QImage>
#include <QRegion>
#include <QTimer>
#include <QWidget>

class Settings;
class StylePanel;
class Toolbar;

// Full-screen window over one monitor showing its frozen contents, where the user picks
// a region, annotates it and copies or saves it.
class Overlay : public QWidget
{
    Q_OBJECT

public:
    Overlay(const Capture &capture, Settings &settings);

    // Shows the overlay and takes the keyboard.
    void start();

Q_SIGNALS:
    // The finished image; savePath is empty when it should only go to the clipboard.
    void accepted(const QImage &image, const QString &savePath);
    void canceled();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class Drag {
        None,
        Pending, // button down outside the selection: a click snaps to a window, a drag draws
        Creating,
        Moving,
        Resizing,
        Drawing,
    };

    void setSelection(const QRect &selection);
    void updateHover();
    QRect windowAt(const QPoint &pos) const;

    void setTool(Tool tool);
    QColor currentColor() const;
    void setColor(const QColor &color);
    void setSize(int size);
    void toggleStylePanel();
    void beginText(const QPoint &pos);
    bool handleTextKey(QKeyEvent *event);
    void commitDraft();
    void discardDraft();
    void undo();
    void redo();
    void rebuildComposite();

    void accept();
    void saveAs();
    void cancel();
    void expand();
    QImage result() const;
    void rememberRegion();
    void finish();

    void grabInput();
    void tryGrabKeyboard();
    void nudge(const QPoint &delta, bool resize);

    struct LoupeLayout {
        int zoom; // screen pixels per captured pixel
        int side; // the magnified square
        int labelHeight;
    };

    void refreshChrome();
    QRegion chromeRegion() const;
    bool loupeVisible() const;
    LoupeLayout loupeLayout() const;
    QRect loupeRect() const;
    QString sizeText() const;
    QRect sizeLabelRect() const;
    void paintDraft(QPainter &p) const;
    void paintChrome(QPainter &p) const;
    void paintLoupe(QPainter &p) const;
    void placeToolbar();
    void updateCursorShape();

    const Capture m_capture;
    Settings &m_settings;
    QImage m_composite; // capture plus committed annotations

    QRect m_selection; // monitor-local, null when nothing is selected
    QRect m_hoverWindow; // window that a click would snap to
    QPoint m_cursor;
    bool m_pointerInside = false; // over the overlay itself, not the toolbar or another monitor

    Tool m_tool = Tool::Select;
    int m_size;
    QColor m_penColor;
    QColor m_highlighterColor;
    QVector<Annotation> m_annotations;
    QVector<Annotation> m_redo;
    Annotation m_draft;
    bool m_hasDraft = false;
    bool m_editingText = false;
    int m_wheelDelta = 0;

    Drag m_drag = Drag::None;
    int m_handle = 0;
    QPoint m_pressPos;
    QPoint m_moveOffset;

    QRegion m_chrome; // what the last paint drew on top of the image, for partial repaints
    Toolbar *m_toolbar;
    StylePanel *m_stylePanel;
    QTimer m_grabTimer;
    int m_grabAttempts = 0;
};
