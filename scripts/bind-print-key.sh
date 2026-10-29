#!/usr/bin/env bash
# Moves the Print key from Spectacle to Max Screenshooter (Plasma 5 on X11).
# Spectacle's other shortcuts (Shift+Print, Meta+Print, ...) are left alone.
# Undo with restore-spectacle-print-key.sh.
set -euo pipefail

app=max-screenshooter.desktop
data="${XDG_DATA_HOME:-$HOME/.local/share}"
print_key=16777225 # Qt::Key_Print

call() {
    gdbus call --session --dest org.kde.kglobalaccel --object-path /kglobalaccel \
        --method "org.kde.KGlobalAccel.$1" "${@:2}" > /dev/null
}

if [[ ! -f "$data/applications/$app" ]]; then
    echo "Install the app first: cmake --install build" >&2
    exit 1
fi

# Free Print while kglobalaccel runs, so it saves the change itself; it refuses a key
# that another component still holds.
call setForeignShortcut "['org.kde.spectacle.desktop', '_launch', 'Spectacle', 'Launch Spectacle']" "@ai []"
# It writes its config on a short timer; restarting before that would lose the change.
sleep 1.5

# kglobalaccel loads launchers from kglobalaccel/ data directories when it starts, taking the
# default key from the desktop file's X-KDE-Shortcuts.
mkdir -p "$data/kglobalaccel"
ln -sf "$data/applications/$app" "$data/kglobalaccel/$app"
systemctl --user restart plasma-kglobalaccel.service

for _ in $(seq 20); do
    call allMainComponents 2> /dev/null && break
    sleep 0.25
done
# Also covers a stale config entry from an earlier install that left the key unset.
call setForeignShortcut "['$app', '_launch', 'Max Screenshooter', 'Max Screenshooter']" "[$print_key]"
sleep 1.5
echo "Print now launches Max Screenshooter."
