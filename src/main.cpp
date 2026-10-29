#include "capture.h"
#include "output.h"
#include "overlay.h"
#include "pinwindow.h"
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

// Saves the capture to savePath and the history folder; copy is false for pinned captures.
void deliver(const QImage &image, const QString &savePath, bool copy, const Settings &settings)
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
    if (copy) {
        Output::copyToClipboard(image);
    }
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
    // After the overlay the process stays alive, without windows if need be, while pins are
    // open or while it owns the clipboard: X11 clipboard contents live in the owning process.
    QClipboard *clipboard = QGuiApplication::clipboard();
    bool overlayOpen = true;
    int pins = 0;
    const auto quitIfIdle = [&] {
        if (!overlayOpen && pins == 0 && !clipboard->ownsClipboard()) {
            settings.sync();
            QCoreApplication::quit();
        }
    };
    QObject::connect(clipboard, &QClipboard::changed, &app, [&](QClipboard::Mode mode) {
        if (mode == QClipboard::Clipboard) {
            quitIfIdle();
        }
    });
    // Frees the overlay's full-monitor images and lets the next Print start a new capture.
    const auto finishOverlay = [&] {
        overlayOpen = false;
        overlay->deleteLater();
        if (bus.isConnected()) {
            bus.unregisterService(kServiceName);
        }
    };
    const auto lingerWhileOwningClipboard = [&] {
        settings.sync();
        quitIfIdle();
    };

    QObject::connect(overlay, &Overlay::accepted, &app, [&](const QImage &image, const QString &savePath) {
        finishOverlay();
        deliver(image, savePath, true, settings);
        lingerWhileOwningClipboard();
    });
    QObject::connect(overlay, &Overlay::pinned, &app, [&](const QImage &image, const QPoint &position) {
        finishOverlay();
        deliver(image, QString(), false, settings);
        auto *pin = new PinWindow(image, position, settings);
        ++pins;
        QObject::connect(pin, &QObject::destroyed, &app, [&] {
            --pins;
            quitIfIdle();
        });
        pin->show();
        settings.sync();
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
