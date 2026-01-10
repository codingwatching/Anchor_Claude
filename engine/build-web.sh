#!/bin/bash
# Anchor Engine - Web Build (Emscripten)
# Usage: ./build-web.sh <game-folder>
# Example: ./build-web.sh ../test

cd "$(dirname "$0")"

# Check for game folder argument
if [ -z "$1" ]; then
    echo "Usage: ./build-web.sh <game-folder>"
    echo "Example: ./build-web.sh ../test"
    exit 1
fi

GAME_FOLDER="$1"

if [ ! -d "$GAME_FOLDER" ]; then
    echo "Error: Game folder not found: $GAME_FOLDER"
    exit 1
fi

if [ ! -f "$GAME_FOLDER/main.lua" ]; then
    echo "Error: main.lua not found in $GAME_FOLDER"
    exit 1
fi

echo "Building with game folder: $GAME_FOLDER"

# Create build directory
mkdir -p build-web

# Build with Emscripten
# -s USE_SDL=2: Use Emscripten's SDL2 port
# -s USE_WEBGL2=1: Enable WebGL 2.0 (OpenGL ES 3.0)
# -s FULL_ES3=1: Full ES3 emulation
# -s WASM=1: Output WebAssembly
# -s ALLOW_MEMORY_GROWTH=1: Allow heap to grow
# --preload-file: Bundle game folder into virtual filesystem at root

/c/emsdk/upstream/emscripten/emcc.bat \
    -O2 \
    -DNDEBUG \
    -DBOX2D_DISABLE_SIMD \
    -I"include" -I"include/SDL2" -I"include/lua" -I"include/stb" -I"include/box2d" -I"include/freetype" \
    src/anchor.c \
    include/lua/*.c \
    include/box2d/*.c \
    -o build-web/anchor.html \
    -s USE_SDL=2 \
    -s USE_FREETYPE=1 \
    -s USE_WEBGL2=1 \
    -s FULL_ES3=1 \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    --preload-file "$GAME_FOLDER"@/ \
    --shell-file shell.html \
    --emrun

if [ $? -eq 0 ]; then
    echo "Build successful: build-web/anchor.html"
    echo "To test: cd build-web && python -m http.server 8000"
    echo "Then open: http://localhost:8000/anchor.html"
else
    echo "Build failed!"
    exit 1
fi
