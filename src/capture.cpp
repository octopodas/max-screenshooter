#include "capture.h"

#include <KWindowInfo>
#include <KX11Extras>

#include <QCursor>
#include <QGuiApplication>
#include <QMargins>
#include <QPixmap>
#include <QScreen>

#include <xcb/xcb.h>

#include <cstdlib>
#include <cstring>

namespace
{

// GTK client-side decorations put invisible shadows inside the window; this is their width.
QMargins gtkFrameExtents(xcb_connection_t *connection, xcb_atom_t atom, WId window)
{
    QMargins margins;
    if (!connection || atom == XCB_ATOM_NONE) {
        return margins;
    }
    const auto cookie = xcb_get_property(connection, false, xcb_window_t(window), atom, XCB_ATOM_CARDINAL, 0, 4);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, nullptr);
    if (reply && reply->format == 32 && xcb_get_property_value_length(reply) == 16) {
        uint32_t v[4];
        std::memcpy(v, xcb_get_property_value(reply), sizeof(v));
        margins = QMargins(int(v[0]), int(v[2]), int(v[1]), int(v[3])); // stored as left, right, top, bottom
    }
    std::free(reply);
    return margins;
}

xcb_atom_t internAtom(xcb_connection_t *connection, const char *name)
{
    const auto cookie = xcb_intern_atom(connection, true, uint16_t(std::strlen(name)), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    const xcb_atom_t atom = reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
    std::free(reply);
    return atom;
}

bool isSnappable(const KWindowInfo &info)
{
    if (!info.valid() || info.mappingState() != NET::Visible || !info.isOnCurrentDesktop() || info.hasState(NET::Hidden)) {
        return false;
    }
    switch (info.windowType(NET::AllTypesMask)) {
    case NET::Desktop:
    case NET::Notification:
    case NET::CriticalNotification:
    case NET::OnScreenDisplay:
    case NET::Tooltip:
    case NET::Splash:
        return false;
    default:
        return true;
    }
}

QVector<QRect> visibleWindows(const QRect &monitor)
{
    QVector<QRect> windows;
    xcb_connection_t *connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(connection)) {
        xcb_disconnect(connection);
        connection = nullptr;
    }
    const xcb_atom_t gtkExtents = connection ? internAtom(connection, "_GTK_FRAME_EXTENTS") : xcb_atom_t(XCB_ATOM_NONE);

    const QList<WId> stacking = KX11Extras::stackingOrder(); // bottom to top
    for (auto it = stacking.crbegin(); it != stacking.crend(); ++it) {
        const KWindowInfo info(*it, NET::WMFrameExtents | NET::WMWindowType | NET::WMState | NET::XAWMState | NET::WMDesktop);
        if (!isSnappable(info)) {
            continue;
        }
        const QRect frame = info.frameGeometry().marginsRemoved(gtkFrameExtents(connection, gtkExtents, *it));
        const QRect local = frame.intersected(monitor).translated(-monitor.topLeft());
        if (!local.isEmpty()) {
            windows.append(local);
        }
    }
    if (connection) {
        xcb_disconnect(connection);
    }
    return windows;
}

}

Capture captureCurrentMonitor()
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    Capture capture;
    capture.screenName = screen->name();
    capture.geometry = screen->geometry();
    // Qt5's xcb backend reads grab coordinates relative to the monitor, so the defaults grab exactly this one.
    capture.image = screen->grabWindow(0).toImage().convertToFormat(QImage::Format_RGB32);
    capture.windows = visibleWindows(capture.geometry);
    return capture;
}
