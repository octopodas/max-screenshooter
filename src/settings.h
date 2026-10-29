#pragma once

#include <KSharedConfig>

#include <QColor>
#include <QRect>
#include <QString>

// Persistent preferences in ~/.config/max-screenshooterrc.
class Settings
{
public:
    Settings();

    // Last confirmed selection on the named monitor, in monitor-local pixels.
    QRect lastRegion(const QString &screenName) const;
    void setLastRegion(const QString &screenName, const QRect &region);

    QString saveDirectory() const;
    void setSaveDirectory(const QString &directory);

    // Every confirmed capture is also written here unless disabled.
    bool historyEnabled() const;
    QString historyDirectory() const;

    QColor penColor() const;
    void setPenColor(const QColor &color);
    QColor highlighterColor() const;
    void setHighlighterColor(const QColor &color);
    int annotationSize() const;
    void setAnnotationSize(int size);

    void sync();

private:
    KSharedConfigPtr m_config;
};
