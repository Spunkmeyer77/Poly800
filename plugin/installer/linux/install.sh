#!/bin/sh
# Installs EX-800 for the current user (VST3 and LV2).
# Run as root (sudo ./install.sh --system) to install for all users instead.
set -e
cd "$(dirname "$0")"

if [ "$1" = "--system" ]; then
    VST3_DIR=/usr/local/lib/vst3
    LV2_DIR=/usr/local/lib/lv2
else
    VST3_DIR="$HOME/.vst3"
    LV2_DIR="$HOME/.lv2"
fi

mkdir -p "$VST3_DIR" "$LV2_DIR"
# Also removes the bundles from before the rename (P-800, up to 0.5.2).
rm -rf "$VST3_DIR/P-800.vst3" "$LV2_DIR/EX-800.lv2" "$VST3_DIR/EX-800.vst3" "$LV2_DIR/EX-800.lv2"
cp -R "EX-800.vst3" "$VST3_DIR/"
cp -R "EX-800.lv2" "$LV2_DIR/"

echo "Installed EX-800:"
echo "  VST3 -> $VST3_DIR/EX-800.vst3"
echo "  LV2  -> $LV2_DIR/EX-800.lv2"
echo "Rescan plug-ins in your host. The manual is 'EX-800 Manual.pdf' in this folder."
