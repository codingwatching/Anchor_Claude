#!/bin/bash
# Anchor Engine - Web Build (Emscripten)

cd "$(dirname "$0")"

# Create build directory
mkdir -p build-web

# Build with Emscripten
# -s USE_SDL=2: Use Emscripten's SDL2 port
# -s USE_WEBGL2=1: Enable WebGL 2.0 (OpenGL ES 3.0)
# -s FULL_ES3=1: Full ES3 emulation
# -s WASM=1: Output WebAssembly
# -s ALLOW_MEMORY_GROWTH=1: Allow heap to grow
# --preload-file: Bundle assets into virtual filesystem

/c/emsdk/upstream/emscripten/emcc.bat \
    -O2 \
    -I"include" -I"include/SDL2" -I"include/lua" -I"include/stb" \
    src/anchor.c \
    include/lua/*.c \
    -o build-web/anchor.html \
    -s USE_SDL=2 \
    -s USE_WEBGL2=1 \
    -s FULL_ES3=1 \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    --preload-file main.lua \
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
