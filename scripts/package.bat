@echo off
REM Package an Anchor game into a single distributable executable
REM Usage: package.bat <game-folder> [output-name]
REM
REM This script:
REM 1. Creates a zip archive of all game assets from the specified folder
REM 2. Copies the Anchor engine executable
REM 3. Appends the zip to the exe (LOVE-style distribution)
REM 4. Outputs a single self-contained game.exe

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Usage: package.bat ^<game-folder^> [output-name]
    echo.
    echo Examples:
    echo   package.bat framework                    # Package framework as framework.exe
    echo   package.bat ..\my-game                   # Package game folder
    echo   package.bat ..\my-game my-awesome-game   # Package with custom name
    exit /b 1
)

set "GAME_FOLDER=%~1"
set "OUTPUT_NAME=%~2"
if "%OUTPUT_NAME%"=="" (
    for %%I in ("%GAME_FOLDER%") do set "OUTPUT_NAME=%%~nxI"
)

REM Get script directory
set "SCRIPT_DIR=%~dp0"
set "ENGINE_EXE=%SCRIPT_DIR%..\engine\build\anchor.exe"
set "OUTPUT_DIR=%SCRIPT_DIR%..\release"
set "TEMP_ZIP=%TEMP%\anchor_game_%RANDOM%.zip"
set "OUTPUT_EXE=%OUTPUT_DIR%\%OUTPUT_NAME%.exe"

REM Verify game folder exists
if not exist "%GAME_FOLDER%" (
    echo Error: Game folder not found: %GAME_FOLDER%
    exit /b 1
)

REM Verify engine exe exists
if not exist "%ENGINE_EXE%" (
    echo Error: Engine not built. Run: cd engine ^&^& build.bat
    exit /b 1
)

REM Verify main.lua exists
if not exist "%GAME_FOLDER%\main.lua" (
    echo Error: No main.lua found in %GAME_FOLDER%
    echo Make sure the game folder contains compiled Lua files.
    exit /b 1
)

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Create zip using PowerShell
echo Creating game archive...
pushd "%GAME_FOLDER%"
powershell -NoProfile -Command "Compress-Archive -Path * -DestinationPath '%TEMP_ZIP%' -Force"
popd

if not exist "%TEMP_ZIP%" (
    echo Error: Failed to create zip archive
    exit /b 1
)

REM Get zip size
for %%A in ("%TEMP_ZIP%") do set "ZIP_SIZE=%%~zA"
echo Archive size: %ZIP_SIZE% bytes

REM Concatenate engine exe + zip into output
echo Creating distributable: %OUTPUT_EXE%
copy /b "%ENGINE_EXE%"+"%TEMP_ZIP%" "%OUTPUT_EXE%" >nul

REM Get final size
for %%A in ("%OUTPUT_EXE%") do set "FINAL_SIZE=%%~zA"
echo Final size: %FINAL_SIZE% bytes

REM Cleanup
del "%TEMP_ZIP%"

echo.
echo Success! Distributable created: %OUTPUT_EXE%
echo This executable is self-contained and can be distributed without additional files.
