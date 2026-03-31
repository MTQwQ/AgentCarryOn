#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPDIR="$SCRIPT_DIR/AppDir"
OUTPUT="$SCRIPT_DIR/AgentCarryOn-x86_64.AppImage"
BUILD_DIR="$SCRIPT_DIR/build"
TOOL_DIR="${HOME}/tools/appimage"
APPIMAGETOOL_BIN="${APPIMAGETOOL_BIN:-$TOOL_DIR/appimagetool}"
GTK_PLUGIN_BIN="${GTK_PLUGIN_BIN:-$TOOL_DIR/linuxdeploy-plugin-gtk.sh}"

if [[ ! -x "$GTK_PLUGIN_BIN" ]]; then
  echo "linuxdeploy-plugin-gtk.sh is not available in the current Linux environment."
  exit 1
fi

if [[ ! -x "$APPIMAGETOOL_BIN" ]] && ! command -v appimagetool >/dev/null 2>&1; then
  echo "appimagetool is not available in the current Linux environment."
  exit 1
fi

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j

rm -rf "$APPDIR/usr/lib" "$APPDIR/apprun-hooks"
mkdir -p "$APPDIR/usr/bin"
cp "$BUILD_DIR/aimonitor" "$APPDIR/usr/bin/aimonitor"
chmod +x "$APPDIR/AppRun" "$APPDIR/usr/bin/aimonitor"
cp "$APPDIR/usr/share/icons/hicolor/256x256/apps/agentcarryon.svg" "$APPDIR/agentcarryon.svg"

mkdir -p "$APPDIR/etc/fonts/conf.d" "$APPDIR/usr/share/fonts" "$APPDIR/usr/share/fontconfig/conf.avail"
cp /etc/fonts/fonts.conf "$APPDIR/etc/fonts/fonts.conf"
cp -a /etc/fonts/conf.d/. "$APPDIR/etc/fonts/conf.d/"
cp -a /usr/share/fontconfig/conf.avail/. "$APPDIR/usr/share/fontconfig/conf.avail/"
cp -a /usr/share/fonts/google-noto-sans-cjk-fonts "$APPDIR/usr/share/fonts/"

export ARCH=x86_64
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$TOOL_DIR:$PATH"
export LINUXDEPLOY="/home/fedora/tools/appimage/linuxdeploy"
"$GTK_PLUGIN_BIN" --appdir "$APPDIR" || true

copy_deps() {
  local target_lib_dir="$APPDIR/usr/lib"
  mkdir -p "$target_lib_dir"
  local -a queue=("$@")
  local -A seen=()

  while ((${#queue[@]})); do
    local item="${queue[0]}"
    queue=("${queue[@]:1}")
    [[ -e "$item" ]] || continue

    while IFS= read -r lib; do
      [[ -n "$lib" ]] || continue
      if [[ -n "${seen[$lib]:-}" ]]; then
        continue
      fi
      seen["$lib"]=1

      local base
      base="$(basename "$lib")"
      cp -Lf "$lib" "$target_lib_dir/$base"
      queue+=("$lib")
    done < <(ldd "$item" | awk '/=> \// {print $3} /^\/.*\.so/ {print $1}')
  done
}

mapfile -t seed_files < <(
  {
    echo "$APPDIR/usr/bin/aimonitor"
    find "$APPDIR/usr/lib" -type f \( -name '*.so' -o -name '*.so.*' \)
  } | sort -u
)

copy_deps "${seed_files[@]}"

fc-cache -r "$APPDIR/usr/share/fonts" >/dev/null 2>&1 || true

rm -f "$OUTPUT"
APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL_BIN" "$APPDIR" "$OUTPUT"

echo "Built: $OUTPUT"
