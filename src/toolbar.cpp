#include "toolbar.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QToolButton>

#include <algorithm>

namespace
{

// Icons follow the font size, so the strip matches the rest of a high-DPI desktop.
int iconExtent(const QWidget *widget)
{
    return std::clamp(widget->fontMetrics().height(), 22, 48);
}

const QVector<QColor> &swatchColors()
{
    static const QVector<QColor> colors = {
        QColor(0xe5, 0x39, 0x35), // red
        QColor(0xfb, 0x8c, 0x00), // orange
        QColor(0xfd, 0xd8, 0x35), // yellow
        QColor(0x43, 0xa0, 0x47), // green
        QColor(0x1e, 0x88, 0xe5), // blue
        QColor(0x8e, 0x24, 0xaa), // purple
        QColor(0x00, 0x00, 0x00), // black
        QColor(0xff, 0xff, 0xff), // white
    };
    return colors;
}

// A dot whose diameter grows with the stroke size.
QIcon dotIcon(const QColor &color, int size, int extent)
{
    static constexpr qreal kDiameters[] = {6, 11, 16}; // at a 22 px icon
    QPixmap pixmap(extent, extent);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x7f, 0x8c, 0x8d), 1));
    p.setBrush(color);
    const qreal d = kDiameters[std::clamp(size, 0, Annotations::kSizeCount - 1)] * extent / 22.0;
    p.drawEllipse(QRectF((extent - d) / 2.0, (extent - d) / 2.0, d, d));
    return QIcon(pixmap);
}

QIcon swatchIcon(const QColor &color, int extent)
{
    QPixmap pixmap(extent, extent);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x7f, 0x8c, 0x8d), 1));
    p.setBrush(color);
    p.drawRoundedRect(QRectF(2.5, 2.5, extent - 5, extent - 5), 3, 3);
    return QIcon(pixmap);
}

QToolButton *makeButton(QWidget *parent, const QIcon &icon, const QString &toolTip)
{
    auto *button = new QToolButton(parent);
    button->setIcon(icon);
    button->setIconSize(QSize(iconExtent(parent), iconExtent(parent)));
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    // Keys go to the overlay; a focused button would swallow Enter and Space.
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

void setupPanel(QFrame *frame)
{
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setAutoFillBackground(true);
    frame->setCursor(Qt::ArrowCursor);
}

}

Toolbar::Toolbar(QWidget *parent)
    : QFrame(parent)
    , m_tools(new QButtonGroup(this))
{
    setupPanel(this);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    const struct {
        Tool tool;
        const char *icon;
        const char *toolTip;
    } tools[] = {
        {Tool::Select, "select-rectangular", "Adjust selection (V)"},
        {Tool::Arrow, "draw-arrow", "Arrow (A)"},
        {Tool::Rectangle, "draw-rectangle", "Rectangle (R)"},
        {Tool::Text, "draw-text", "Text (T)"},
        {Tool::Highlighter, "draw-highlight", "Highlighter (H)"},
        {Tool::Pixelate, "pixelate", "Pixelate (P)"},
    };
    for (const auto &t : tools) {
        QToolButton *button = addButton(QString::fromLatin1(t.icon), QString::fromLatin1(t.toolTip));
        button->setCheckable(true);
        m_tools->addButton(button, int(t.tool));
    }
    connect(m_tools, &QButtonGroup::idClicked, this, [this](int id) {
        Q_EMIT toolChosen(Tool(id));
    });

    addSeparator();
    m_style = addButton(QString(), QStringLiteral("Colour and size (mouse wheel changes size)"));
    connect(m_style, &QToolButton::clicked, this, &Toolbar::styleRequested);
    m_undo = addButton(QStringLiteral("edit-undo"), QStringLiteral("Undo (Ctrl+Z)"));
    connect(m_undo, &QToolButton::clicked, this, &Toolbar::undoRequested);
    m_redo = addButton(QStringLiteral("edit-redo"), QStringLiteral("Redo (Ctrl+Shift+Z)"));
    connect(m_redo, &QToolButton::clicked, this, &Toolbar::redoRequested);

    addSeparator();
    QToolButton *expand = addButton(QStringLiteral("view-fullscreen"), QStringLiteral("Whole screen (F)"));
    connect(expand, &QToolButton::clicked, this, &Toolbar::expandRequested);
    QToolButton *pin = addButton(QStringLiteral("window-pin"), QStringLiteral("Pin to screen (Ctrl+P)"));
    connect(pin, &QToolButton::clicked, this, &Toolbar::pinRequested);
    QToolButton *save = addButton(QStringLiteral("document-save-as"), QStringLiteral("Save as… (Ctrl+S)"));
    connect(save, &QToolButton::clicked, this, &Toolbar::saveRequested);
    QToolButton *copy = addButton(QStringLiteral("edit-copy"), QStringLiteral("Copy to clipboard (Enter)"));
    copy->setText(QStringLiteral("Copy"));
    copy->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(copy, &QToolButton::clicked, this, &Toolbar::copyRequested);
    QToolButton *copyText = addButton(QStringLiteral("insert-text"), QStringLiteral("Copy the text in the selection (Ctrl+T)"));
    connect(copyText, &QToolButton::clicked, this, &Toolbar::copyTextRequested);
    QToolButton *cancel = addButton(QStringLiteral("window-close"), QStringLiteral("Cancel (Esc)"));
    connect(cancel, &QToolButton::clicked, this, &Toolbar::cancelRequested);

    setHistoryState(false, false);
}

void Toolbar::setTool(Tool tool)
{
    if (QAbstractButton *button = m_tools->button(int(tool))) {
        button->setChecked(true);
    }
}

void Toolbar::setStyle(const QColor &color, int size)
{
    m_style->setIcon(dotIcon(color, size, iconExtent(this)));
}

void Toolbar::setHistoryState(bool canUndo, bool canRedo)
{
    m_undo->setEnabled(canUndo);
    m_redo->setEnabled(canRedo);
}

QToolButton *Toolbar::styleButton() const
{
    return m_style;
}

QToolButton *Toolbar::addButton(const QString &icon, const QString &toolTip)
{
    QToolButton *button = makeButton(this, icon.isEmpty() ? QIcon() : QIcon::fromTheme(icon), toolTip);
    layout()->addWidget(button);
    return button;
}

void Toolbar::addSeparator()
{
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    layout()->addWidget(line);
}

StylePanel::StylePanel(QWidget *parent)
    : QFrame(parent)
{
    setupPanel(this);
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    const QVector<QColor> &colors = swatchColors();
    for (int i = 0; i < colors.size(); ++i) {
        const QColor color = colors[i];
        QToolButton *button = makeButton(this, swatchIcon(color, iconExtent(this)), color.name());
        button->setCheckable(true);
        connect(button, &QToolButton::clicked, this, [this, color] {
            Q_EMIT colorChosen(color);
        });
        layout->addWidget(button, 0, i);
        m_colors.append(button);
    }

    static const char *const kSizeNames[] = {"Thin", "Medium", "Thick"};
    for (int size = 0; size < Annotations::kSizeCount; ++size) {
        QToolButton *button = makeButton(this, dotIcon(palette().color(QPalette::WindowText), size, iconExtent(this)),
                                         QString::fromLatin1(kSizeNames[size]));
        button->setCheckable(true);
        connect(button, &QToolButton::clicked, this, [this, size] {
            Q_EMIT sizeChosen(size);
        });
        layout->addWidget(button, 1, size);
        m_sizes.append(button);
    }
    hide();
}

void StylePanel::setStyle(const QColor &color, int size)
{
    const QVector<QColor> &colors = swatchColors();
    for (int i = 0; i < m_colors.size(); ++i) {
        m_colors[i]->setChecked(colors[i].rgb() == color.rgb());
    }
    for (int i = 0; i < m_sizes.size(); ++i) {
        m_sizes[i]->setChecked(i == size);
    }
}
