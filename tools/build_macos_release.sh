#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/build/macos-release"
PACKAGE_DIR="$BUILD_DIR/Round Display Helper"
APP_DIR="$PACKAGE_DIR/Round Display Helper.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
DIST_DIR="$ROOT_DIR/dist"
ARCHIVE="$DIST_DIR/RoundDisplayHelper-macOS.zip"

rm -rf "$BUILD_DIR"
mkdir -p "$MACOS_DIR" "$DIST_DIR"

clang \
  -arch arm64 \
  -arch x86_64 \
  -mmacosx-version-min=12.0 \
  -fobjc-arc \
  -Wall -Wextra -Werror \
  -framework Carbon \
  -framework CoreFoundation \
  -framework Foundation \
  -framework CoreBluetooth \
  "$ROOT_DIR/host/InputLanguageBridge.m" \
  -o "$MACOS_DIR/round-display-helper"

cp "$ROOT_DIR/packaging/macos/Info.plist" "$CONTENTS_DIR/Info.plist"
cp "$ROOT_DIR/packaging/macos/com.rounddisplay.input-language-helper.plist" \
  "$PACKAGE_DIR/com.rounddisplay.input-language-helper.plist"
cp "$ROOT_DIR/packaging/macos/Install Round Display Helper.command" \
  "$PACKAGE_DIR/Install Round Display Helper.command"
cp "$ROOT_DIR/packaging/macos/Uninstall Round Display Helper.command" \
  "$PACKAGE_DIR/Uninstall Round Display Helper.command"
chmod +x "$PACKAGE_DIR"/*.command "$MACOS_DIR/round-display-helper"

if [ -n "${MACOS_SIGNING_IDENTITY:-}" ]; then
  codesign --force --timestamp --options runtime \
    --sign "$MACOS_SIGNING_IDENTITY" "$APP_DIR"
else
  codesign --force --sign - "$APP_DIR"
fi

rm -f "$ARCHIVE"
ditto -c -k --sequesterRsrc --keepParent "$PACKAGE_DIR" "$ARCHIVE"

file "$MACOS_DIR/round-display-helper"
codesign --verify --deep --strict --verbose=2 "$APP_DIR"
echo "$ARCHIVE"
