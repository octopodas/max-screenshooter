#pragma once

#include <QDateTime>
#include <QString>

class QImage;
class QWidget;
class Settings;

// Where finished captures go: the clipboard, the history folder and user-chosen files.
namespace Output
{

// "Screenshot_2026-10-29_14-03-22"
QString timestampedName(const QDateTime &time);

// dir/base.suffix, or dir/base_2.suffix, base_3... when that file already exists.
QString uniquePath(const QString &directory, const QString &baseName, const QString &suffix);

// Shows the KDE save dialog; returns the chosen path, or an empty string if cancelled.
QString askSavePath(Settings &settings, QWidget *parent);

// KDE's "automatically select filename extension" appends the filter's extension even to a
// name that already has one ("shot.jpg" becomes "shot.jpg.png"); this undoes that.
QString stripAddedExtension(const QString &path);

bool writeImage(const QImage &image, const QString &path, QString *error);

void copyToClipboard(const QImage &image);

// Writes a PNG into the history folder; returns its path, or an empty string on failure.
QString saveToHistory(const QImage &image, const Settings &settings, const QDateTime &time, QString *error);

}
