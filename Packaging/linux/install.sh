#!/usr/bin/env bash
# Hip-Hop BeatMaker – Installation für den aktuellen Benutzer (kein root nötig)
#   ./install.sh            installieren
#   ./install.sh --remove   deinstallieren
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="Hip-Hop BeatMaker"
BIN="$HOME/.local/bin/hiphop-beatmaker"
OPT="$HOME/.local/opt/hiphop-beatmaker"
DESK="$HOME/.local/share/applications/hiphop-beatmaker.desktop"
ICON="$HOME/.local/share/icons/hicolor/512x512/apps/hiphop-beatmaker.png"

if [ "${1:-}" = "--remove" ]; then
  rm -rf "$HOME/.vst3/$APP.vst3" "$HOME/.lv2/$APP.lv2" "$OPT" "$BIN" "$DESK" "$ICON"
  echo "Hip-Hop BeatMaker wurde entfernt."
  exit 0
fi

mkdir -p "$HOME/.vst3" "$HOME/.lv2" "$OPT" "$HOME/.local/bin" "$(dirname "$DESK")" "$(dirname "$ICON")"
rm -rf "$HOME/.vst3/$APP.vst3" "$HOME/.lv2/$APP.lv2"
cp -R "$HERE/$APP.vst3" "$HOME/.vst3/"
cp -R "$HERE/$APP.lv2" "$HOME/.lv2/"
cp "$HERE/$APP" "$OPT/hiphop-beatmaker"
chmod +x "$OPT/hiphop-beatmaker"
ln -sf "$OPT/hiphop-beatmaker" "$BIN"
cp "$HERE/hiphop-beatmaker.png" "$ICON"
sed "s|@EXEC@|$OPT/hiphop-beatmaker|" "$HERE/hiphop-beatmaker.desktop" > "$DESK"
command -v update-desktop-database >/dev/null && update-desktop-database "$(dirname "$DESK")" || true

echo "Installiert:"
echo "  VST3 -> ~/.vst3/$APP.vst3"
echo "  LV2  -> ~/.lv2/$APP.lv2"
echo "  App  -> Startmenü \"Hip-Hop BeatMaker\" bzw. Befehl: hiphop-beatmaker"
