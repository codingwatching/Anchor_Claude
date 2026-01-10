# File Organization Walkthrough

Step-by-step through the build process, showing exactly where files are at each stage.

---

## Starting Point: Source Files

```
Anchor/
├── engine/
│   ├── src/
│   │   └── anchor.c
│   ├── yue/                      # Framework SOURCE (.yue)
│   │   ├── object.yue
│   │   ├── timer.yue
│   │   └── spring.yue
│   ├── build.bat
│   └── build/
│       └── anchor.exe            # Compiled engine
│
└── games/
    └── emoji-ball-battles/
        ├── main.yue              # Game SOURCE (.yue)
        ├── player.yue
        └── assets/
            └── emoji.png
```

At this point:
- Framework source is `.yue` files in `engine/yue/`
- Game source is `.yue` files in `games/emoji-ball-battles/`
- Engine executable exists at `engine/build/anchor.exe`

---

## Step 1: Compile YueScript → Lua

Run `yue -r` on both framework and game:

```bash
# Compile framework
yue -r engine/yue/           # Compiles .yue → .lua in place

# Compile game
yue -r games/emoji-ball-battles/   # Compiles .yue → .lua in place
```

**After compilation:**

```
Anchor/
├── engine/
│   ├── src/
│   │   └── anchor.c
│   ├── yue/
│   │   ├── object.yue            # Source (kept)
│   │   ├── object.lua            # Compiled (new)
│   │   ├── timer.yue
│   │   ├── timer.lua             # Compiled (new)
│   │   ├── spring.yue
│   │   └── spring.lua            # Compiled (new)
│   └── build/
│       └── anchor.exe
│
└── games/
    └── emoji-ball-battles/
        ├── main.yue              # Source (kept)
        ├── main.lua              # Compiled (new)
        ├── player.yue
        ├── player.lua            # Compiled (new)
        └── assets/
            └── emoji.png
```

Now we have `.lua` files alongside `.yue` files.

---

## Development Mode: Running the Game

To run the game during development:

```bash
cd games/emoji-ball-battles
../../engine/build/anchor.exe .
```

The engine loads `main.lua` from the current directory.

**Problem:** When `main.lua` does `require 'yue.object'`, Lua looks for:
- `./yue/object.lua`  ← Does NOT exist here!

The framework `.lua` files are in `engine/yue/`, not in `games/emoji-ball-battles/yue/`.

**The game folder looks like:**
```
games/emoji-ball-battles/       # Current working directory
├── main.lua
├── player.lua
└── assets/
    └── emoji.png
                                 # NO yue/ folder here!
```

**Lua's require search (default package.path):**
```
./yue/object.lua                 ← Doesn't exist
/usr/share/lua/5.4/yue/object.lua   ← Doesn't exist
...etc
```

**`require 'yue.object'` FAILS.**

---

## Solution A: Symlink During Development

Create a symlink so the game folder has access to framework:

```bash
cd games/emoji-ball-battles
ln -s ../../engine/yue yue      # Creates symlink
```

**After symlink:**
```
games/emoji-ball-battles/
├── main.lua
├── player.lua
├── yue -> ../../engine/yue     # Symlink!
└── assets/
    └── emoji.png
```

Now `require 'yue.object'` finds `./yue/object.lua` through the symlink.

**Game code:**
```lua
local object = require 'yue.object'
```

---

## Solution C: Modify package.path in C

Instead of symlinks, modify `anchor.c` to tell Lua where to find framework files:

```c
// In anchor.c, after luaL_openlibs(L)
luaL_dostring(L, "package.path = '../../engine/yue/?.lua;' .. package.path");
```

**Lua's require search (modified package.path):**
```
../../engine/yue/object.lua      ← EXISTS! Found it.
./object.lua
...etc
```

Now `require 'object'` works (no `yue.` prefix needed).

**Game code:**
```lua
local object = require 'object'
```

---

## Bundled Mode: Single .exe

When shipping, we bundle everything into one executable using zip-append.

**Step 1: Create a staging folder with everything needed:**

```
staging/
├── main.lua                     # From game
├── player.lua                   # From game
├── yue/                         # COPIED from engine/yue/
│   ├── object.lua
│   ├── timer.lua
│   └── spring.lua
└── assets/
    └── emoji.png
```

**Step 2: Zip this folder and append to exe:**

```bash
cd staging
zip -r game.zip *
cat ../engine/build/anchor.exe game.zip > emoji-ball-battles.exe
```

**Step 3: When the bundled exe runs:**

The engine detects the appended zip, mounts it as a virtual filesystem at `/`:

```
Virtual filesystem (inside exe):
/
├── main.lua
├── player.lua
├── yue/
│   ├── object.lua
│   ├── timer.lua
│   └── spring.lua
└── assets/
    └── emoji.png
```

Now `require 'yue.object'` finds `/yue/object.lua` — it works!

---

## Summary: Development vs Bundled

| Mode | Framework Location | How require finds it |
|------|-------------------|---------------------|
| Development (no setup) | `engine/yue/` | **FAILS** - not in path |
| Development + Symlink | `game/yue/` → symlink | Works via `./yue/object.lua` |
| Development + C change | `engine/yue/` | Works via modified package.path |
| Bundled exe | Inside zip at `/yue/` | Works via `./yue/object.lua` |

---

## The Core Question

Both solutions work for bundled mode. The question is: **what do we do during development?**

**Option A (Symlink):**
- Each game folder needs a `yue` symlink pointing to `engine/yue/`
- No C changes needed
- Game code: `require 'yue.object'`
- Setup: One symlink per game

**Option C (package.path):**
- Modify C to add `engine/yue/` to Lua's search path
- No symlinks needed
- Game code: `require 'object'` (cleaner)
- Setup: One-time C change

---

## My Recommendation

**Option A is simpler** because:
1. No C code changes
2. Symlinks are a one-time setup per game
3. The `yue.` prefix makes it clear these are framework classes
4. Bundled mode naturally has `yue/` folder, so it "just works"

**Setup script for new game:**
```bash
#!/bin/bash
# new-game.sh <game-name>
mkdir -p games/$1
ln -s ../../engine/yue games/$1/yue
echo "-- Game entry point" > games/$1/main.yue
```
