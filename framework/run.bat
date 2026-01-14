@echo off
cd /d "%~dp0"
.\yue.exe -r main.yue
.\yue.exe -r anchor/
..\engine\build\anchor.exe .
