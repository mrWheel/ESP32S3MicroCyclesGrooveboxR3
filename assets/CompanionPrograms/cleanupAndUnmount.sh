#!/bin/zsh

SDCARD="/Volumes/SDCARD"

cd "$HOME" || exit 1

dot_clean -m "$SDCARD"

find "$SDCARD" \
  \( -name "._*" -o -name ".DS_Store" -o -name ".Spotlight-*" -o \
     -name ".Trashes" -o -name ".fseventsd" -o -name ".setGaini.json.*" \) \
  -exec rm -rf {} +

sync

diskutil unmount "$SDCARD"

