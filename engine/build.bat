@echo off
setlocal

cd /d "%~dp0"

:: Find and run vcvarsall.bat for Visual Studio
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
) else (
    echo ERROR: Could not find Visual Studio
    exit /b 1
)

if not exist build mkdir build

:: Build Lua library if it doesn't exist
if not exist lib\lua.lib (
    echo Building Lua library...
    if not exist build\lua_obj mkdir build\lua_obj
    cl.exe /nologo /O2 /W3 /c /I"include/lua" ^
        include/lua/lapi.c include/lua/lauxlib.c include/lua/lbaselib.c ^
        include/lua/lcode.c include/lua/lcorolib.c include/lua/lctype.c ^
        include/lua/ldblib.c include/lua/ldebug.c include/lua/ldo.c ^
        include/lua/ldump.c include/lua/lfunc.c include/lua/lgc.c ^
        include/lua/linit.c include/lua/liolib.c include/lua/llex.c ^
        include/lua/lmathlib.c include/lua/lmem.c include/lua/loadlib.c ^
        include/lua/lobject.c include/lua/lopcodes.c include/lua/loslib.c ^
        include/lua/lparser.c include/lua/lstate.c include/lua/lstring.c ^
        include/lua/lstrlib.c include/lua/ltable.c include/lua/ltablib.c ^
        include/lua/ltm.c include/lua/lundump.c include/lua/lutf8lib.c ^
        include/lua/lvm.c include/lua/lzio.c ^
        /Fo"build\lua_obj\\"
    if %ERRORLEVEL% neq 0 (
        echo Lua build failed!
        exit /b 1
    )
    lib.exe /nologo /out:lib\lua.lib build\lua_obj\*.obj
    if %ERRORLEVEL% neq 0 (
        echo Lua library creation failed!
        exit /b 1
    )
    rmdir /s /q build\lua_obj
    echo Lua library built.
)

:: Build anchor (static linking - no DLLs needed)
cl.exe /nologo /O2 /W3 ^
    /I"include" /I"include/SDL2" /I"include/lua" /I"include/glad" /I"include/KHR" /I"include/stb" ^
    src/anchor.c include/glad/gl.c ^
    /Fe"build/anchor.exe" ^
    /link /LIBPATH:"lib" ^
    lua.lib SDL2-static.lib SDL2main.lib ^
    opengl32.lib kernel32.lib user32.lib gdi32.lib winmm.lib imm32.lib ^
    ole32.lib oleaut32.lib version.lib uuid.lib advapi32.lib setupapi.lib shell32.lib ^
    /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build successful: build/anchor.exe
