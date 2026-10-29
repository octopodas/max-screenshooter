#!/usr/bin/env bash
# Gives the Print key back to Spectacle; reverses bind-print-key.sh.
set -euo pipefail

app=max-screenshooter.desktop
data="${XDG_DATA_HOME:-$HOME/.local/share}"
print_key=16777225 # Qt::Key_Print

call() {
    gdbus call --session --dest org.kde.kglobalaccel --object-path /kglobalaccel \
        --method "org.kde.KGlobalAccel.$1" "${@:2}" > /dev/null
}

call setForeignShortcut "['$app', '_launch', 'Max Screenshooter', 'Max Screenshooter']" "@ai []"
call setForeignShortcut "['org.kde.spectacle.desktop', '_launch', 'Spectacle', 'Launch Spectacle']" "[$print_key]"
rm -f "$data/kglobalaccel/$app"
sleep 1.5 # let kglobalaccel write its config before anything restarts it
echo "Print launches Spectacle again."
