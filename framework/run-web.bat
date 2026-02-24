@echo off
REM Web build and run - rebuilds with Emscripten, launches browser

cd /d "%~dp0"

REM Build with Emscripten and run
cd ..\engine
call bash -c "./build-web.sh ../framework"
cd build-web
C:\emsdk\upstream\emscripten\emrun.bat --browser chrome anchor.html
cd ..\..
