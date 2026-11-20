#!/bin/bash
# make-app.sh: build "Launch Windows.app" in /Applications (drag it to the Dock).
# Re-run this if you move the project folder, since the app stores its path.
# Rebuilding replaces the app in place, so the Dock icon keeps working.

DIR="$(cd "$(dirname "$0")" && pwd)"
APP="/Applications/Launch Windows.app"

rm -rf "$APP"
osacompile -o "$APP" -e "do shell script quoted form of \"$DIR/launch.sh\""

# Custom icon (regenerate with: swift icon/draw-icon.swift icon/icon-1024.png, then iconutil).
cp "$DIR/icon/LaunchWindows.icns" "$APP/Contents/Resources/applet.icns"
# Newer macOS reads the icon from Assets.car (via CFBundleIconName) before applet.icns,
# so remove both or the default script icon wins.
rm -f "$APP/Contents/Resources/Assets.car"
/usr/libexec/PlistBuddy -c "Delete :CFBundleIconName" "$APP/Contents/Info.plist" 2>/dev/null
codesign --force --sign - "$APP" >/dev/null 2>&1   # re-sign after changing the bundle
touch "$APP"                                        # nudge Finder/Dock to refresh the icon

echo "Built $APP"
