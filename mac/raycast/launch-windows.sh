#!/bin/bash

# Required parameters:
# @raycast.schemaVersion 1
# @raycast.title Launch Windows
# @raycast.mode silent

# Optional parameters:
# @raycast.icon images/launch-windows.png
# @raycast.packageName PC Remote
# @raycast.description Wake the Windows PC if needed, then open it in Moonlight.

DIR="$(cd "$(dirname "$0")/.." && pwd)"
"$DIR/launch.sh"
