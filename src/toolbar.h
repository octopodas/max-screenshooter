#pragma once

#include "annotation.h"

#include <QFrame>
#include <QVector>

class QButtonGroup;
class QToolButton;

// The strip of buttons shown next to the selection. It is a child of the overlay window
// rather than a popup, so it never fights the overlay's keyboard grab.
class Toolbar : public QFrame
{
    Q_OBJECT

public:
    explicit Toolbar(QWidget *parent);

    void setTool(Tool tool);
    void setStyle(const QColor &color, int size);
    void setHistoryState(bool canUndo, bool canRedo);
    QToolButton *styleButton() const;

Q_SIGNALS:
    void toolChosen(Tool tool);
    void styleRequested();
    void undoRequested();
    void redoRequested();
    void expandRequested();
    void saveRequested();
    void copyRequested();
    void cancelRequested();

private:
    QToolButton *addButton(const QString &icon, const QString &toolTip);
    void addSeparator();

    QButtonGroup *m_tools;
    QToolButton *m_style;
    QToolButton *m_undo;
    QToolButton *m_redo;
};

// Colour swatches and stroke sizes, opened from the toolbar's style button.
class StylePanel : public QFrame
{
    Q_OBJECT

public:
    explicit StylePanel(QWidget *parent);

    void setStyle(const QColor &color, int size);

Q_SIGNALS:
    void colorChosen(const QColor &color);
    void sizeChosen(int size);

private:
    QVector<QToolButton *> m_colors;
    QVector<QToolButton *> m_sizes;
};
