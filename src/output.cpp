#include "output.h"

#include "settings.h"

#include <QBuffer>
#include <QClipboard>
#include <QProcess>
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

void copyTextToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

QStringList installedOcrLanguages()
{
    QProcess process;
    process.start(QStringLiteral("tesseract"), {QStringLiteral("--list-langs")});
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        return {};
    }
    // The first line is a header: "List of available languages in "/usr/share/...": (2)"
    QStringList lines = QString::fromUtf8(process.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!lines.isEmpty()) {
        lines.removeFirst();
    }
    for (QString &line : lines) {
        line = line.trimmed();
    }
    lines.removeAll(QStringLiteral("osd")); // orientation detection, not a language
    return lines;
}

QString ocrLanguageArgument(const QString &configured, const QStringList &installed)
{
    if (!configured.isEmpty()) {
        return configured;
    }
    QStringList chosen;
    for (const QString &language : {QStringLiteral("eng"), QStringLiteral("pol"), QStringLiteral("rus")}) {
        if (installed.contains(language)) {
            chosen << language;
        }
    }
    if (chosen.isEmpty() && !installed.isEmpty()) {
        chosen << installed.constFirst();
    }
    return chosen.join(QLatin1Char('+'));
}

QString recognizeText(const QImage &image, const QString &languages, QString *error)
{
    const QStringList installed = installedOcrLanguages();
    if (installed.isEmpty()) {
        *error = QStringLiteral("Copying text needs Tesseract. Install it with:\n\n"
                                "sudo apt install tesseract-ocr tesseract-ocr-eng tesseract-ocr-pol");
        return QString();
    }

    // Tesseract is tuned for ~300 DPI scans; doubling screen text roughly matches that.
    const QImage input = image.width() * image.height() <= 4000 * 4000 / 4
        ? image.scaled(image.size() * 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        : image;
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    input.save(&buffer, "PNG");

    QProcess process;
    process.start(QStringLiteral("tesseract"),
                  {QStringLiteral("stdin"), QStringLiteral("stdout"), QStringLiteral("-l"), ocrLanguageArgument(languages, installed)});
    process.write(png);
    process.closeWriteChannel();
    if (!process.waitForFinished(60000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = QStringLiteral("Tesseract failed: %1").arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
        return QString();
    }
    // Tesseract ends pages with a form feed and pads with blank lines.
    QString text = QString::fromUtf8(process.readAllStandardOutput());
    text.remove(QLatin1Char('\f'));
    return text.trimmed();
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
