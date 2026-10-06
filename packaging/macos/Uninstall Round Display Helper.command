#!/bin/sh
set -eu

LABEL="com.rounddisplay.input-language-helper"
DOMAIN="gui/$(id -u)"
TARGET_PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
TARGET_APP="$HOME/Applications/Round Display Helper.app"

launchctl bootout "$DOMAIN/$LABEL" >/dev/null 2>&1 || true
rm -f "$TARGET_PLIST"
rm -rf "$TARGET_APP"

echo "Round Display Helper was removed."
printf "Press Return to close."
read -r _
