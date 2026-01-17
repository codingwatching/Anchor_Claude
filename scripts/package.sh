#!/bin/bash
# Package an Anchor game into a single distributable executable
# Usage: ./scripts/package.sh <game-folder> [output-name]
#
# This script:
# 1. Creates a zip archive of all game assets from the specified folder
# 2. Copies the Anchor engine executable
# 3. Appends the zip to the exe (LÖVE-style distribution)
# 4. Outputs a single self-contained game.exe

set -e

# Check arguments
if [ -z "$1" ]; then
    echo "Usage: $0 <game-folder> [output-name]"
    echo ""
    echo "Examples:"
    echo "  $0 framework                    # Package framework as framework.exe"
    echo "  $0 ../my-game                   # Package game folder"
    echo "  $0 ../my-game my-awesome-game   # Package with custom name"
    exit 1
fi

GAME_FOLDER="$1"
OUTPUT_NAME="${2:-$(basename "$GAME_FOLDER")}"

# Paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENGINE_DIR="$SCRIPT_DIR/../engine"
ENGINE_EXE="$ENGINE_DIR/build/anchor.exe"
OUTPUT_DIR="$SCRIPT_DIR/../release"
TEMP_ZIP="/tmp/anchor_game_$$.zip"

# Verify game folder exists
if [ ! -d "$GAME_FOLDER" ]; then
    echo "Error: Game folder not found: $GAME_FOLDER"
    exit 1
fi

# Verify engine exe exists
if [ ! -f "$ENGINE_EXE" ]; then
    echo "Error: Engine not built. Run: cd engine && ./build.bat"
    exit 1
fi

# Verify main.lua exists in game folder
if [ ! -f "$GAME_FOLDER/main.lua" ]; then
    echo "Error: No main.lua found in $GAME_FOLDER"
    echo "Make sure the game folder contains compiled Lua files."
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Create zip of game folder contents
# -j = junk (don't store) directory names
# -r = recursive
# We need to cd into the folder so paths are relative
echo "Creating game archive..."
rm -f "$TEMP_ZIP"
(cd "$GAME_FOLDER" && zip -r "$TEMP_ZIP" . -x "*.yue" -x ".git/*" -x "__pycache__/*" -x "*.pyc" -x ".DS_Store")

# Report zip size
ZIP_SIZE=$(stat -c%s "$TEMP_ZIP" 2>/dev/null || stat -f%z "$TEMP_ZIP")
echo "Archive size: $ZIP_SIZE bytes"

# Copy engine exe and append zip
OUTPUT_EXE="$OUTPUT_DIR/${OUTPUT_NAME}.exe"
echo "Creating distributable: $OUTPUT_EXE"
cp "$ENGINE_EXE" "$OUTPUT_EXE"
cat "$TEMP_ZIP" >> "$OUTPUT_EXE"

# Report final size
FINAL_SIZE=$(stat -c%s "$OUTPUT_EXE" 2>/dev/null || stat -f%z "$OUTPUT_EXE")
echo "Final size: $FINAL_SIZE bytes"

# Cleanup
rm -f "$TEMP_ZIP"

echo ""
echo "Success! Distributable created: $OUTPUT_EXE"
echo "This executable is self-contained and can be distributed without additional files."
