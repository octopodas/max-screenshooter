#include "capture.h"
#include "output.h"
#include "overlay.h"
#include "settings.h"

#include <KWindowSystem>

#include <QApplication>
#include <QClipboard>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>

namespace
{

const QString kServiceName = QStringLiteral("local.maxscreenshooter");

void deliver(const QImage &image, const QString &savePath, const Settings &settings)
{
    QStringList errors;
    QString error;
    if (!savePath.isEmpty() && !Output::writeImage(image, savePath, &error)) {
        errors << QStringLiteral("Could not save %1: %2").arg(savePath, error);
    }
    const bool savedIntoHistory = !savePath.isEmpty() && QFileInfo(savePath).absolutePath() == QDir(settings.historyDirectory()).absolutePath();
    if (settings.historyEnabled() && !savedIntoHistory && Output::saveToHistory(image, settings, QDateTime::currentDateTime(), &error).isEmpty()) {
        errors << QStringLiteral("Could not add the capture to the history folder: %1").arg(error);
    }
    Output::copyToClipboard(image);
    if (!errors.isEmpty()) {
        QMessageBox::warning(nullptr, QGuiApplication::applicationDisplayName(), errors.join(QLatin1Char('\n')));
    }
}

}

int main(int argc, char **argv)
{
    // Widget pixels must map one to one onto monitor pixels.
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("max-screenshooter"));
    app.setApplicationDisplayName(QStringLiteral("Max Screenshooter"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setDesktopFileName(QStringLiteral("max-screenshooter"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("max-screenshooter")));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Select a region of the monitor under the cursor and copy it to the clipboard."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    if (!KWindowSystem::isPlatformX11()) {
        qCritical("max-screenshooter: only X11 sessions are supported");
        return 1;
    }

    // Pressing Print again while the overlay is open must not screenshot the overlay.
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && !bus.registerService(kServiceName)) {
        return 0;
    }

    Settings settings;
    auto *overlay = new Overlay(captureCurrentMonitor(), settings);

    QObject::connect(overlay, &Overlay::canceled, &app, [&] {
        settings.sync();
        QCoreApplication::quit();
    });
    // Runs once the overlay is done; frees its full-monitor images before we linger.
    const auto finishOverlay = [&] {
        overlay->deleteLater();
        if (bus.isConnected()) {
            bus.unregisterService(kServiceName);
        }
    };
    // X11 clipboard contents live in the process that owns them, so stay alive without
    // windows until another application takes the clipboard over.
    const auto lingerWhileOwningClipboard = [&] {
        settings.sync();
        QClipboard *clipboard = QGuiApplication::clipboard();
        if (!clipboard->ownsClipboard()) {
            QCoreApplication::quit();
            return;
        }
        QObject::connect(clipboard, &QClipboard::changed, &app, [clipboard](QClipboard::Mode mode) {
            if (mode == QClipboard::Clipboard && !clipboard->ownsClipboard()) {
                QCoreApplication::quit();
            }
        });
    };

    QObject::connect(overlay, &Overlay::accepted, &app, [&](const QImage &image, const QString &savePath) {
        finishOverlay();
        deliver(image, savePath, settings);
        lingerWhileOwningClipboard();
    });
    QObject::connect(overlay, &Overlay::textRequested, &app, [&](const QImage &image) {
        finishOverlay();
        QString error;
        const QString text = Output::recognizeText(image, settings.ocrLanguages(), &error);
        if (text.isEmpty()) {
            QMessageBox::warning(nullptr, QGuiApplication::applicationDisplayName(),
                                 error.isEmpty() ? QStringLiteral("No text was found in the selection.") : error);
            settings.sync();
            QCoreApplication::quit();
            return;
        }
        Output::copyTextToClipboard(text);
        lingerWhileOwningClipboard();
    });

    overlay->start();
    return app.exec();
}
