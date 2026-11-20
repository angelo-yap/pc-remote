#!/bin/bash
# play.sh: one click to game.
#   PC already on  -> stream straight away
#   PC off         -> send "wake" via ntfy -> wait until it's up -> stream
# Logi Options+ is paused during the stream (it turns the MX Master side
# buttons into Back/Forward shortcuts, so MB4/MB5 never reach the game)
# and comes back when Moonlight closes.

set -u
# Launchers (Dock app, Shortcuts, Raycast) start with a minimal PATH; ping lives in /sbin.
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

notify() { osascript -e "display notification \"$1\" with title \"PC Remote\"" >/dev/null 2>&1; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG="$SCRIPT_DIR/config.sh"
if [[ ! -f "$CONFIG" ]]; then
  echo "Missing $CONFIG (copy config.example.sh and fill it in)."
  exit 1
fi
source "$CONFIG"   # NTFY_TOPIC, PC_TS_IP, MOONLIGHT_HOST

MOONLIGHT="/Applications/Moonlight.app/Contents/MacOS/Moonlight"
WAIT_TIMEOUT=180       # seconds to wait for the PC to boot
APOLLO_PORT=47989      # Apollo answers here once it's ready for Moonlight
OPTIONS_LABEL="com.logi.cp-dev-mgr"
OPTIONS_PLIST="/Library/LaunchAgents/com.logi.optionsplus.plist"

pc_answers_ping() { ping -c 1 -t 1 "$PC_TS_IP" >/dev/null 2>&1; }
apollo_ready()    { nc -z -G 2 "$PC_TS_IP" "$APOLLO_PORT" >/dev/null 2>&1; }

# Wait until a check passes, or give up after WAIT_TIMEOUT seconds total.
wait_for() {
  local check="$1" what="$2"
  printf "Waiting for %s" "$what"
  until "$check"; do
    if (( SECONDS > WAIT_TIMEOUT )); then
      echo
      echo "Gave up after ${WAIT_TIMEOUT}s waiting for $what."
      echo "Try: send 'status' to ntfy, or check the PC."
      notify "Gave up waiting for $what."
      exit 1
    fi
    printf "."
    sleep 2
  done
  echo " ok"
}

pause_options() {
  launchctl bootout "gui/$(id -u)/$OPTIONS_LABEL" 2>/dev/null
  pkill -f "/Applications/logioptionsplus.app" 2>/dev/null
}

restore_options() {
  # Error 5 just means it's already loaded, so it's safe to ignore.
  launchctl bootstrap "gui/$(id -u)" "$OPTIONS_PLIST" 2>/dev/null
}

# --- 1. Is the PC already on? -----------------------------------------------
if apollo_ready; then
  echo "PC is on."
else
  if pc_answers_ping; then
    echo "PC is on, Apollo is still starting."
  else
    echo "PC is off. Sending wake..."
    if ! curl -s -f -d "wake" "https://ntfy.sh/$NTFY_TOPIC" >/dev/null; then
      echo "Couldn't reach ntfy.sh. Check your internet connection."
      notify "Couldn't reach ntfy.sh."
      exit 1
    fi
    notify "PC is off. Waking it up..."
    wait_for pc_answers_ping "the PC to boot"
  fi
  wait_for apollo_ready "Apollo"
fi

# --- 2. Stream ---------------------------------------------------------------
pause_options
trap restore_options EXIT   # runs when the script ends, even on Ctrl-C

echo "Starting stream..."
notify "PC is ready. Starting stream..."
"$MOONLIGHT" stream "$MOONLIGHT_HOST" "Desktop"
