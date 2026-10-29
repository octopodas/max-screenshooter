# Max Screenshooter

Region screenshots for KDE Plasma 5 on X11. Press **Print**: the monitor under the cursor freezes, you pick a region, and **Enter** copies it to the clipboard.

## Using it

| Action | What it does |
| --- | --- |
| Print | Open on the monitor under the cursor. The screen is frozen as it was at that moment. |
| Drag | Draw a region. Drag the handles to resize it, or drag inside it to move it. |
| Click a window | Select exactly that window. Double-click it to capture it straight away. |
| Enter, Ctrl+C, double-click inside, Copy | Copy to the clipboard and close. With nothing selected, copies the whole monitor. |
| Ctrl+T, Copy text button | Recognize the text in the selection (OCR) and copy it as plain text. Annotations are ignored. |
| Ctrl+P, Pin button | Float the capture above other windows, right where it was. See [Pinning](#pinning). |
| Ctrl+S, Save button | KDE save dialog (PNG, JPEG or WebP, chosen by extension). The image is copied too. |
| F, whole-screen button | Expand the selection to the whole monitor. |
| Right-click | Clear the selection; with nothing selected, cancel. |
| Esc | Cancel. |
| Arrow keys | Move the selection 1 px (Ctrl: 10 px). With nothing selected, or while dragging, they move the pointer instead. |
| Shift+arrows | Resize the selection from its bottom-right corner. |

The loupe next to the cursor magnifies the pixels around it and shows the position, or the size while you drag. The last region you used on each monitor is drawn again the next time you open it, so Enter repeats the same capture. A whole-monitor selection is not remembered.

### Annotations

| Key | Tool |
| --- | --- |
| V | Adjust selection (default) |
| A | Arrow |
| R | Rectangle |
| T | Text: click, type, Enter to finish (Shift+Enter for a new line, Esc to drop it) |
| H | Highlighter |
| P | Pixelate. The block averaging cannot be reversed, unlike a light blur. |

The colour-and-size button picks from eight colours and three sizes; the mouse wheel also changes the size while a drawing tool is active. Ctrl+Z undoes and Ctrl+Shift+Z (or Ctrl+Y) redoes. The highlighter keeps its own colour, yellow by default.

### Copying text

Ctrl+T runs the selection through [Tesseract](https://github.com/tesseract-ocr/tesseract) on your machine and copies the recognized text; nothing leaves the computer. It needs:

```sh
sudo apt install tesseract-ocr tesseract-ocr-eng tesseract-ocr-pol
```

By default it reads English and Polish, whichever of them are installed. Set `[OCR] Languages=eng+deu` (any installed Tesseract codes joined with `+`) to change that. Layout is not kept: columns and tables come out as plain lines.

### Pinning

Ctrl+P (or the pin button) closes the overlay and leaves the capture, annotations included, floating on top of other windows at the spot you selected. Pins stay out of the taskbar and Alt+Tab, and you can open as many as you like.

| On a pin | What it does |
| --- | --- |
| Drag | Move it. |
| Mouse wheel, + and − | Zoom from 10% to 800%, around the cursor. Ctrl+0 returns to actual size. |
| Ctrl+wheel | Opacity, down to 20%. |
| Ctrl+C, Ctrl+S | Copy it, or save it with the KDE dialog. |
| Double-click, Esc | Close it. |
| Right-click | All of the above as a menu. |

Pinning does not touch the clipboard. A whole-monitor capture starts at half size, centred, so it doesn't cover the screen.

### History

Every copied, saved or pinned capture is also written to `~/Pictures/Screenshots/Screenshot_YYYY-MM-DD_HH-MM-SS.png`.

## Settings

`~/.config/max-screenshooterrc`. The app writes most of these itself; the history keys can be edited by hand:

```ini
[History]
Enabled=true             # false turns the automatic copies off
Directory=/home/max/Pictures/Screenshots

[General]
SaveDirectory=...        # last folder used in the save dialog

[Annotations]
PenColor=#e53935
HighlighterColor=#fdd835
Size=1                   # 0 thin, 1 medium, 2 thick

[OCR]
Languages=eng+pol       # empty: English and Polish if installed

[LastRegion]
DP-0=x,y,width,height    # per monitor
```

## Building and installing

Needs Qt 5.15, KF5 Config and WindowSystem, ECM and xcb. On Kubuntu:

```sh
sudo apt install cmake extra-cmake-modules qtbase5-dev libkf5config-dev libkf5windowsystem-dev libxcb1-dev
```

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build
cmake --install build
scripts/bind-print-key.sh
```

`bind-print-key.sh` takes Print from Spectacle and gives it to this app. Spectacle's other shortcuts (Shift+Print, Meta+Print and so on) are left alone. `scripts/restore-spectacle-print-key.sh` gives Print back to Spectacle. You can also change the key later in System Settings → Shortcuts → Max Screenshooter.

## How it works

- `src/capture.cpp` grabs the monitor under the cursor before any window is shown, and lists the visible window frames for click-to-snap. GTK client-side shadows are trimmed using `_GTK_FRAME_EXTENTS`.
- `src/overlay.cpp` is the full-screen selection window. It bypasses the window manager, so panels can't cover it and no open animation plays. It grabs the keyboard and retries until kglobalaccel lets go of the Print key.
- On X11 the clipboard contents live in the process that set them. After copying, the app closes its window but keeps running until another application takes over the clipboard. It also keeps running while any of its pins (`src/pinwindow.cpp`) are open.
- Qt's high-DPI scaling is turned off so that widget pixels match monitor pixels exactly.

## Limitations

- X11 only. Wayland would need KWin's ScreenShot2 D-Bus interface or the desktop portal.
- Annotations can't be selected or moved once drawn; use undo.
