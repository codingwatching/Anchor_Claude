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

:: Build Box2D library if it doesn't exist
if not exist lib\box2d.lib (
    echo Building Box2D library...
    if not exist build\box2d_obj mkdir build\box2d_obj
    cl.exe /nologo /O2 /W3 /c /std:c17 /DNDEBUG /I"include" /I"include/box2d" ^
        include/box2d/aabb.c include/box2d/arena_allocator.c include/box2d/array.c ^
        include/box2d/bitset.c include/box2d/body.c include/box2d/broad_phase.c ^
        include/box2d/constraint_graph.c include/box2d/contact.c include/box2d/contact_solver.c ^
        include/box2d/core.c include/box2d/distance.c include/box2d/distance_joint.c ^
        include/box2d/dynamic_tree.c include/box2d/geometry.c include/box2d/hull.c ^
        include/box2d/id_pool.c include/box2d/island.c include/box2d/joint.c ^
        include/box2d/manifold.c include/box2d/math_functions.c include/box2d/motor_joint.c ^
        include/box2d/mover.c include/box2d/physics_world.c include/box2d/prismatic_joint.c ^
        include/box2d/revolute_joint.c include/box2d/sensor.c include/box2d/shape.c ^
        include/box2d/solver.c include/box2d/solver_set.c include/box2d/table.c ^
        include/box2d/timer.c include/box2d/types.c include/box2d/weld_joint.c ^
        include/box2d/wheel_joint.c ^
        /Fo"build\box2d_obj\\"
    if %ERRORLEVEL% neq 0 (
        echo Box2D build failed!
        exit /b 1
    )
    lib.exe /nologo /out:lib\box2d.lib build\box2d_obj\*.obj
    if %ERRORLEVEL% neq 0 (
        echo Box2D library creation failed!
        exit /b 1
    )
    rmdir /s /q build\box2d_obj
    echo Box2D library built.
)

:: Build anchor (static linking - no DLLs needed)
cl.exe /nologo /O2 /W3 ^
    /I"include" /I"include/SDL2" /I"include/lua" /I"include/glad" /I"include/KHR" /I"include/stb" /I"include/box2d" /I"include/freetype" ^
    src/anchor.c include/glad/gl.c ^
    /Fe"build/anchor.exe" ^
    /link /LIBPATH:"lib" ^
    lua.lib box2d.lib freetype.lib SDL2-static.lib SDL2main.lib ^
    opengl32.lib kernel32.lib user32.lib gdi32.lib winmm.lib imm32.lib ^
    ole32.lib oleaut32.lib version.lib uuid.lib advapi32.lib setupapi.lib shell32.lib ^
    /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build successful: build/anchor.exe

:: Copy to emoji-ball-battles if it exists
if exist "E:\a327ex\emoji-ball-battles\tools" (
    copy /Y "build\anchor.exe" "E:\a327ex\emoji-ball-battles\tools\anchor.exe" >nul
    echo Copied to emoji-ball-battles/tools/
)
