#!/bin/bash
# File: research/ui-screenshots.sh
# Screenshot one page of the editor from the Standalone build, under a virtual X display.
# Needs: Xvfb running on :77 (Xvfb :77 -screen 0 1400x900x24 &), xdotool, ImageMagick.
# usage: ui-screenshots.sh <stage|kick|snare|new|show|curtain|advanced|about|shadow> <out.png> [seconds]
# BUILD = the CMake build directory (default: build)
APP="${BUILD:-build}/DrumMaster_artefacts/Release/Standalone/The Drum Master"
export DISPLAY=:77
DRUM_MASTER_DEMO="$1" "$APP" > /dev/null 2>&1 &
PID=$!
sleep "${3:-3}"
WID=$(xdotool search --pid $PID --onlyvisible 2>/dev/null | tail -1)
if [ -n "$WID" ]; then import -window "$WID" "$2.raw.png"; else import -window root "$2.raw.png"; fi
convert "$2.raw.png" -crop 1100x688+4+60 +repage "$2"; rm -f "$2.raw.png"   # drop the standalone title bar + banner
kill $PID 2>/dev/null; wait $PID 2>/dev/null
