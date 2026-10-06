#!/bin/sh
set -eu

LABEL="com.rounddisplay.input-language-helper"
LEGACY_LABEL="com.codex.round-display-input-language"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_APP="$SCRIPT_DIR/Round Display Helper.app"
INSTALL_ROOT="$HOME/Applications"
TARGET_APP="$INSTALL_ROOT/Round Display Helper.app"
TARGET_BINARY="$TARGET_APP/Contents/MacOS/round-display-helper"
LAUNCH_AGENTS="$HOME/Library/LaunchAgents"
TARGET_PLIST="$LAUNCH_AGENTS/$LABEL.plist"
LOG_DIR="$HOME/Library/Logs/RoundDisplay"
LOG_FILE="$LOG_DIR/helper.log"
DOMAIN="gui/$(id -u)"

if [ ! -d "$SOURCE_APP" ]; then
  echo "Round Display Helper.app was not found next to this installer."
  printf "Press Return to close."
  read -r _
  exit 1
fi

launchctl bootout "$DOMAIN/$LABEL" >/dev/null 2>&1 || true
launchctl bootout "$DOMAIN/$LEGACY_LABEL" >/dev/null 2>&1 || true
mkdir -p "$INSTALL_ROOT" "$LAUNCH_AGENTS" "$LOG_DIR"
rm -rf "$TARGET_APP"
ditto "$SOURCE_APP" "$TARGET_APP"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
  -f "$TARGET_APP"
rm -f "$LAUNCH_AGENTS/$LEGACY_LABEL.plist"
cp "$SCRIPT_DIR/com.rounddisplay.input-language-helper.plist" "$TARGET_PLIST"
plutil -replace ProgramArguments -json "[\"$TARGET_BINARY\"]" "$TARGET_PLIST"
plutil -replace StandardOutPath -string "$LOG_FILE" "$TARGET_PLIST"
plutil -replace StandardErrorPath -string "$LOG_FILE" "$TARGET_PLIST"
plutil -lint "$TARGET_PLIST"
launchctl bootstrap "$DOMAIN" "$TARGET_PLIST"
launchctl kickstart -k "$DOMAIN/$LABEL"
# Launch the registered app once through LaunchServices as well.  This makes
# macOS present the Bluetooth privacy prompt in the foreground on first
# install instead of silently leaving a background-only LaunchAgent denied.
/usr/bin/open "$TARGET_APP"

echo
echo "Round Display Helper is installed and running."
echo "macOS may ask once for Bluetooth access; choose Allow."
echo "Log: $LOG_FILE"
echo
printf "Press Return to close."
read -r _
