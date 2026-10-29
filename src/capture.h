#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

// A frozen copy of one monitor plus the window frames on it, taken before any UI is shown.
struct Capture {
    QString screenName;
    QRect geometry; // the monitor in virtual-desktop coordinates
    QImage image; // monitor contents, monitor-local pixels
    QVector<QRect> windows; // monitor-local frames of visible windows, topmost first
};

// Captures the monitor under the mouse cursor (X11 only).
Capture captureCurrentMonitor();
