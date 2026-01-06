@echo off
cd /d "%~dp0build-web"
C:\emsdk\upstream\emscripten\emrun.bat --browser chrome anchor.html
