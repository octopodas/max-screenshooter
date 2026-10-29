#include "output.h"

#include "settings.h"

#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageWriter>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>

namespace Output
{

QString timestampedName(const QDateTime &time)
{
    return QStringLiteral("Screenshot_") + time.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
}

QString uniquePath(const QString &directory, const QString &baseName, const QString &suffix)
{
    const QDir dir(directory);
    QString path = dir.filePath(baseName + QLatin1Char('.') + suffix);
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = dir.filePath(QStringLiteral("%1_%2.%3").arg(baseName).arg(n).arg(suffix));
    }
    return path;
}

QString askSavePath(Settings &settings, QWidget *parent)
{
    // With Plasma's platform theme this is the native KDE dialog.
    QFileDialog dialog(parent, QStringLiteral("Save Screenshot"), settings.saveDirectory());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters({
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp)"),
        QStringLiteral("PNG image (*.png)"),
        QStringLiteral("JPEG image (*.jpg *.jpeg)"),
        QStringLiteral("WebP image (*.webp)"),
    });
    dialog.setDefaultSuffix(QStringLiteral("png"));
    dialog.selectFile(timestampedName(QDateTime::currentDateTime()) + QStringLiteral(".png"));
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return QString();
    }
    const QString chosen = dialog.selectedFiles().constFirst();
    const QString path = stripAddedExtension(chosen);
    // The dialog only checked the name it built for overwriting, not the one we keep.
    if (path != chosen && QFileInfo::exists(path)
        && QMessageBox::question(parent, QStringLiteral("Save Screenshot"),
                                 QStringLiteral("%1 already exists. Replace it?").arg(QFileInfo(path).fileName()))
            != QMessageBox::Yes) {
        return QString();
    }
    settings.setSaveDirectory(QFileInfo(path).absolutePath());
    return path;
}

QString stripAddedExtension(const QString &path)
{
    static const QRegularExpression doubled(QStringLiteral("\\.(png|jpe?g|webp)\\.(png|jpe?g|webp)$"),
                                            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = doubled.match(path);
    return match.hasMatch() ? path.left(match.capturedEnd(1)) : path;
}

bool writeImage(const QImage &image, const QString &path, QString *error)
{
    QImageWriter writer(path);
    const QByteArray format = writer.format().toLower();
    if (format == "jpg" || format == "jpeg" || format == "webp") {
        writer.setQuality(92);
    }
    if (writer.write(image)) {
        return true;
    }
    if (error) {
        *error = writer.errorString();
    }
    return false;
}

void copyToClipboard(const QImage &image)
{
    auto *data = new QMimeData;
    data->setImageData(image);
    // Tells Klipper to keep the image in its history even if it is set to ignore images.
    data->setData(QStringLiteral("x-kde-force-image-copy"), QByteArray());
    QGuiApplication::clipboard()->setMimeData(data, QClipboard::Clipboard);
}

QString saveToHistory(const QImage &image, const Settings &settings, const QDateTime &time, QString *error)
{
    const QString directory = settings.historyDirectory();
    if (!QDir().mkpath(directory)) {
        if (error) {
            *error = QStringLiteral("Cannot create %1").arg(directory);
        }
        return QString();
    }
    const QString path = uniquePath(directory, timestampedName(time), QStringLiteral("png"));
    return writeImage(image, path, error) ? path : QString();
}

}
