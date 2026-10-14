#!/bin/bash
# File: research/ui-screenshots.sh
# Screenshot one page of the editor from the Standalone build, under a virtual X display.
# Needs: Xvfb running on :77 (Xvfb :77 -screen 0 1400x900x24 &), xdotool, ImageMagick.
# usage: ui-screenshots.sh <stage|listen|kick|snare|new|show|curtain|advanced|about|midi|shadow> <out.png> [seconds]
# BUILD = the CMake build directory (default: build)
# The capture is taken from the root window at the editor's position (capturing the app's own
# window returns an empty image without a window manager). Before the shot, a click on an empty
# spot at the top gives the window focus and parks the pointer, so no control shows a hover state.
APP="${BUILD:-build}/DrumMaster_artefacts/Release/Standalone/The Drum Master"
export DISPLAY=:77
xdotool mousemove 5 5
DRUM_MASTER_DEMO="$1" "$APP" > /dev/null 2>&1 &
PID=$!
sleep "${3:-3}"
WID=$(xdotool search --pid $PID 2>/dev/null | tail -1)
eval "$(xdotool getwindowgeometry --shell "$WID")"
OX=$((X + 4)); OY=$((Y + 60))   # skip the standalone's frame, title bar and audio banner
if [ "$1" != "shadow" ]; then   # (a click would close Shadow Play)
    xdotool mousemove $((OX + 300)) $((OY + 4)) click 1 mousemove $((OX + 310)) $((OY + 6))
    sleep 0.6
fi
import -window root -crop 1100x688+$OX+$OY +repage "$2"
kill $PID 2>/dev/null; wait $PID 2>/dev/null
true
