@echo off
REM Run engine with framework/ as game folder (no YueScript compilation)
REM Use this for testing engine-only changes with existing .lua files

cd /d "%~dp0"
build\anchor.exe ..\framework
