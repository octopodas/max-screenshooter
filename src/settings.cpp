#include "settings.h"

#include <KConfigGroup>

#include <QDir>
#include <QStandardPaths>

namespace
{

QString picturesDirectory()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return pictures.isEmpty() ? QDir::homePath() : pictures;
}

QColor readColor(const KConfigGroup &group, const char *key, const QColor &fallback)
{
    const QColor color(group.readEntry(key, fallback.name()));
    return color.isValid() ? color : fallback;
}

}

Settings::Settings()
    : m_config(KSharedConfig::openConfig(QStringLiteral("max-screenshooterrc")))
{
}

QRect Settings::lastRegion(const QString &screenName) const
{
    return m_config->group("LastRegion").readEntry(screenName.toUtf8().constData(), QRect());
}

void Settings::setLastRegion(const QString &screenName, const QRect &region)
{
    m_config->group("LastRegion").writeEntry(screenName.toUtf8().constData(), region);
}

QString Settings::saveDirectory() const
{
    return m_config->group("General").readEntry("SaveDirectory", picturesDirectory());
}

void Settings::setSaveDirectory(const QString &directory)
{
    m_config->group("General").writeEntry("SaveDirectory", directory);
}

bool Settings::historyEnabled() const
{
    return m_config->group("History").readEntry("Enabled", true);
}

QString Settings::historyDirectory() const
{
    return m_config->group("History").readEntry("Directory", picturesDirectory() + QStringLiteral("/Screenshots"));
}

QColor Settings::penColor() const
{
    return readColor(m_config->group("Annotations"), "PenColor", QColor(0xe5, 0x39, 0x35));
}

void Settings::setPenColor(const QColor &color)
{
    m_config->group("Annotations").writeEntry("PenColor", color.name());
}

QColor Settings::highlighterColor() const
{
    return readColor(m_config->group("Annotations"), "HighlighterColor", QColor(0xfd, 0xd8, 0x35));
}

void Settings::setHighlighterColor(const QColor &color)
{
    m_config->group("Annotations").writeEntry("HighlighterColor", color.name());
}

int Settings::annotationSize() const
{
    return m_config->group("Annotations").readEntry("Size", 1);
}

void Settings::setAnnotationSize(int size)
{
    m_config->group("Annotations").writeEntry("Size", size);
}

QString Settings::ocrLanguages() const
{
    return m_config->group("OCR").readEntry("Languages", QString());
}

void Settings::sync()
{
    m_config->sync();
}
