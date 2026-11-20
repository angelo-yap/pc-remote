#!/bin/bash
# launch.sh: start play.sh in the background and return right away.
# Used by the Dock app, the macOS Shortcut and Raycast, so none of them
# have to stay open while the PC boots or while you're streaming.

DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$HOME/Library/Logs/pc-remote-play.log"

if pgrep -f "$DIR/play.sh" >/dev/null; then
  echo "Already starting or streaming."
  exit 0
fi

nohup "$DIR/play.sh" >"$LOG" 2>&1 &
echo "Starting PC stream..."
