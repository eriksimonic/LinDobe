#!/usr/bin/env bash
# Add the installed Adobe apps to your desktop's application menu (any freedesktop desktop:
# KDE, GNOME, Xfce, ...). Remove them again with --remove.
set -euo pipefail
source "$(cd "$(dirname "$0")/.." && pwd)/bin/env.sh"
APPS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps"
if [ "${1:-}" = --remove ]; then
  rm -f "$APPS"/adobewine-*.desktop "$ICONS"/adobewine-*.png
  update-desktop-database "$APPS" 2>/dev/null || true
  echo "launchers removed"; exit 0
fi
mkdir -p "$APPS" "$ICONS"
# What the menu entries run: this checkout's launcher, or the Flatpak's ("flatpak run --command=adobewine <id>").
EXEC="${ADOBEWINE_EXEC:-$ADOBEWINE_SRC/bin/adobewine}"
desktop_arg() {
  local arg="$1"
  arg="${arg//\\/\\\\}"
  arg="${arg//\"/\\\"}"
  arg="${arg//\`/\\\`}"
  arg="${arg//\$/\\$}"
  arg="${arg//\\/\\\\}"
  arg="${arg//%/%%}"
  arg="${arg//$'\n'/\\n}"
  arg="${arg//$'\r'/\\r}"
  arg="${arg//$'\t'/\\t}"
  printf '"%s"' "$arg"
}
source "$ADOBEWINE_SRC/bin/app-catalog.sh"
icon() {  # icon <name> <exe>: default icon group, retaining color depth and alpha
  python3 "$ADOBEWINE_SRC/scripts/extract-app-icon.py" "$2" "$ICONS/adobewine-$1.png" || true
}
entry() {  # entry <name> <title> <categories> <mime> <wmclass> <exe>
  [ -f "$6" ] || return 0
  icon "$1" "$6"
  cat > "$APPS/adobewine-$1.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$2 (LinDobe)
Comment=$2 through the LinDobe Wine build
Exec=$EXEC $1 %F
Icon=adobewine-$1
Terminal=false
Categories=$3
MimeType=$4
StartupWMClass=$5
EOF
  echo "added $2"
}
while IFS='|' read -r command title folder exe categories mime; do
  wmclass="${exe##*/}"
  entry "$command" "$title" "$categories" "$mime" "${wmclass,,}" "$ADOBE_PROGRAM_FILES/$(app_folder "$folder")/$exe"
done < "$ADOBE_APP_CATALOG"
cat > "$APPS/adobewine-off.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Stop all Adobe apps (LinDobe)
Exec=$EXEC off
Icon=process-stop
Terminal=false
Categories=Utility;
EOF
# Register before Product Manager is installed, so a later browser login needs no manual association.
PM_EXEC="${ADOBEWINE_EXEC:-$(desktop_arg "$ADOBEWINE_SRC/bin/adobewine")}"
cat > "$APPS/adobewine-misterhorse.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Mister Horse Product Manager (LinDobe)
Exec=env $(desktop_arg "WINEPREFIX=$WINEPREFIX") $(desktop_arg "ADOBEWINE_HOME=$ADOBEWINE_HOME") $PM_EXEC run $(desktop_arg "$WINEPREFIX/drive_c/Program Files/Mister Horse Product Manager/ProductManager.exe") %u
Icon=application-x-executable
Terminal=false
NoDisplay=true
MimeType=x-scheme-handler/misterhorsepm;
StartupWMClass=productmanager.exe
EOF
update-desktop-database "$APPS" 2>/dev/null || true
echo "launchers installed in $APPS"
