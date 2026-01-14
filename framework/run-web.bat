@echo off
REM Web build and run - compiles YueScript, rebuilds with Emscripten, launches browser

cd /d "%~dp0"

REM Compile YueScript files
.\yue.exe -r main.yue
.\yue.exe -r anchor/

REM Build with Emscripten and run
cd ..\engine
call bash -c "./build-web.sh ../framework"
cd build-web
C:\emsdk\upstream\emscripten\emrun.bat --browser chrome anchor.html
cd ..\..
