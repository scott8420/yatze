#!/usr/bin/env bash
# Install Yatzé! for this user (no root): ~/.local/bin/yatze, its .desktop
# entry, icons and metainfo, then refresh the caches GNOME reads.
#   ./install.sh            builds Release first if needed
#   PREFIX=/usr/local sudo -E ./install.sh   (system-wide, if you want it)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
PREFIX="${PREFIX:-$HOME/.local}"
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build-release -- -j"$(nproc)"
cmake --install build-release
command -v gtk4-update-icon-cache >/dev/null && gtk4-update-icon-cache -q -t -f "$PREFIX/share/icons/hicolor" || true
command -v update-desktop-database >/dev/null && update-desktop-database -q "$PREFIX/share/applications" || true
echo ""
echo "Installed to $PREFIX. Yatzé! is in the GNOME overview (search \"Yatze\" or \"dice\")."
case ":$PATH:" in *":$PREFIX/bin:"*) ;; *) echo "Note: $PREFIX/bin is not on PATH (the overview launcher still works).";; esac
