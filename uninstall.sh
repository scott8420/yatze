#!/usr/bin/env bash
# Remove what install.sh put in place. Your games, players and settings stay:
#   ~/.config/yatze/  ~/.local/share/yatze/  ~/.cache/yatze/
# (delete those folders too for a clean slate).
set -euo pipefail
PREFIX="${PREFIX:-$HOME/.local}"
ID="io.github.scott8420.Yatze"
rm -fv "$PREFIX/bin/yatze" \
       "$PREFIX/share/applications/$ID.desktop" \
       "$PREFIX/share/icons/hicolor/scalable/apps/$ID.svg" \
       "$PREFIX/share/icons/hicolor/symbolic/apps/$ID-symbolic.svg" \
       "$PREFIX/share/metainfo/$ID.metainfo.xml"
command -v gtk4-update-icon-cache >/dev/null && gtk4-update-icon-cache -q -t -f "$PREFIX/share/icons/hicolor" || true
command -v update-desktop-database >/dev/null && update-desktop-database -q "$PREFIX/share/applications" || true
echo "Yatzé! removed from $PREFIX."
