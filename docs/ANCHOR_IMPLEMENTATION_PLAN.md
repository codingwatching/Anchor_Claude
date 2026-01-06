# Anchor Implementation Plan

C engine with YueScript scripting, OpenGL rendering, targeting Windows and Web.

---

## Summary of Key Decisions

| Area | Decision | Rationale |
|------|----------|-----------|
| Renderer | OpenGL | Smooth rotation, additive blending, performance headroom, console-portable |
| Audio | TBD (miniaudio or SoLoud) | Need pitch shifting; SDL_mixer insufficient |
| Physics | Box2D 3.1 | Already used, true ball-to-ball collisions needed |
| Scripting | Lua 5.4 + YueScript | Build-time compilation with `-r` flag for line numbers |
| Timestep | Fixed 144 Hz | High simulation rate for responsive feel; determinism for replays |
| Resolution | Per-game configurable | 480×270, 640×360, or custom; aspect-ratio scaling with letterboxing |
| C Structure | Single anchor.c | Monolithic file, easier navigation |
| Resources | Live forever | Games are small enough; no unloading needed |
| Linking | Static | No DLLs; SDL2, Lua, audio all compiled in |
| Distribution | Single executable | Zip-append for game content, extractable by modders |

---

## Build Strategy

### YueScript Compilation

**Build-time compilation** — compile `.yue` → `.lua` during build, not at runtime.

```
game.yue ──► yue -r ──► game.lua ──► embedded in executable
                ↑
                └── -r flag preserves line numbers for debugging
```

### Directory Structure

```
anchor/
├── engine/
│   ├── src/
│   │   └── anchor.c        # Single monolithic C file
│   ├── include/            # Vendored headers (SDL2, Lua, glad, stb)
│   ├── lib/                # Vendored libraries
│   └── build.bat           # Windows build
├── yue/                    # YueScript engine code
│   ├── object.yue
│   ├── timer.yue
│   ├── spring.yue
│   ├── collider.yue
│   └── init.yue
├── lua/                    # Compiled Lua output
├── main.yue                # Test/game entry point
├── main.lua                # Compiled Lua entry point
├── assets/
└── build-web.bat           # Web build (Emscripten)
```

---

## Phase 1: C Skeleton + OpenGL Setup

**Goal:** Window opens, clears to a color, processes input, runs Lua. Works on Windows and Web.

### 1.1 Project Setup
- [x] Create directory structure
- [x] Set up build.bat for Windows (cl.exe + linking)
- [x] Download/configure dependencies:
  - [x] SDL2 (window, input)
  - [x] Lua 5.4
  - [x] glad (OpenGL loading)
  - [x] stb_image (texture loading)
  - [x] stb_truetype (font loading)
- [x] Static linking (no DLLs):
  - [x] Build SDL2 from source as static library (CMake)
  - [x] Compile Lua to static lib
  - [x] Link all Windows system dependencies
  - [x] Verify standalone exe works without DLLs

### 1.2 OpenGL Initialization
- [x] SDL2 window with OpenGL context
- [x] OpenGL 3.3 Core Profile (widely supported, WebGL 2.0 compatible subset)
- [x] Basic shader compilation (vertex + fragment)
- [x] Clear screen to solid color
- [x] Verify OpenGL context on Windows

### 1.3 Main Loop
- [x] Fixed timestep game loop (144 Hz)
- [x] Delta time accumulator pattern
- [x] Basic input polling (SDL_PollEvent inside fixed loop)
- [x] Clean shutdown

### 1.4 Lua Integration
- [x] Initialize Lua state
- [x] Load and run external `.lua` file (command-line argument, like LÖVE)
- [x] Protected call wrapper with stack trace (luaL_traceback)
- [x] Error state: red screen, error message stored, window stays open

### 1.5 Windowing
- [x] Fullscreen toggle (Alt+Enter or F11)
- [x] Aspect-ratio scaling with letterboxing (fills window, black bars when needed)
- [x] Framebuffer at native game resolution (480×270)
- [x] Handle window resize events

### 1.6 Verification
- [x] C calls Lua init, then `update(dt)` each fixed step
- [x] Lua can print to console
- [x] Errors show red screen (error message stored for later text rendering)
- [x] Fullscreen toggle works (F11 or Alt+Enter)
- [x] Window resize maintains aspect ratio with letterboxing

**Deliverable:** OpenGL window running Lua with error handling, fullscreen, and proper scaling. ✓ Complete

---

## Phase 2: Web Build (Emscripten)

**Goal:** Same code compiles and runs in browser via WebGL.

### 2.1 Emscripten Setup
- [x] Install Emscripten SDK to `C:\emsdk`
- [x] Configure Git Bash environment (`~/.bashrc`):
  - `source /c/emsdk/emsdk_env.sh` for PATH
  - Aliases for `emcc` → `emcc.bat` (Git Bash doesn't auto-execute .bat)
- [x] Create `build-web.sh` with emcc flags
- [x] Configure for WebGL 2.0 (OpenGL ES 3.0 subset)

### 2.2 Platform Abstraction
- [x] Conditional compilation with `#ifdef __EMSCRIPTEN__`
- [x] Refactor main loop into `main_loop_iteration()` function
- [x] `emscripten_set_main_loop` integration (yields to browser each frame)
- [x] OpenGL ES context request (`SDL_GL_CONTEXT_PROFILE_ES`)
- [x] Use `<GLES3/gl3.h>` instead of glad on web

### 2.3 Unified Shader System
- [x] Platform-specific headers via preprocessor defines:
  - Desktop: `#version 330 core`
  - Web: `#version 300 es` + `precision mediump float;`
- [x] Shader sources written without version line
- [x] `compile_shader()` auto-prepends correct header based on shader type
- [x] Same approach will work for user-authored shaders loaded at runtime

### 2.4 Asset Embedding
- [x] Preload Lua scripts into virtual filesystem
- [x] `--preload-file main.lua` flag
- [x] Asset loading works identically to desktop

### 2.5 HTML Shell
- [x] Create `shell.html` template with canvas
- [x] CSS for letterboxing and pixel-perfect rendering
- [x] Keyboard event capture (prevent default for arrow keys, space)
- [x] Canvas focus for input on load

### 2.6 Development Workflow
- [x] `--emrun` flag for console output piping
- [x] `run-web.bat` launcher script (opens browser + shows printf in terminal)
- [x] Launchy integration for quick testing

### 2.7 Verification
- [x] Same Lua code runs in browser
- [x] Orange test quad renders correctly
- [x] Aspect-ratio scaling with letterboxing works
- [x] No visible differences from Windows build
- [x] Console output visible in terminal via emrun

**Deliverable:** Web build that matches Windows behavior. ✓ Complete

**Critical:** Every subsequent phase must be verified on both Windows and Web before proceeding.

---

## Phase 3: Rendering

**Goal:** Core rendering infrastructure with deferred command queues, layers, transforms, basic shapes, and sprites.

See `docs/SHAPES_PLAN.md` for full technical details on the shapes system (to be implemented incrementally in later phases).

### Architecture Overview

**Deferred rendering:** Draw calls during update store commands. GPU work happens at frame end.

```
During update:
  game:circle(...)  → stores DrawCommand in game.commands[]
  game:rectangle(...)  → stores DrawCommand in game.commands[]

At frame end:
  For each layer:
    Process commands in order → build vertices → batch → flush
  Composite layers to screen
```

---

### 3.1 Layer System & Command Queue
- [ ] Layer struct: FBO, color texture, command array, transform stack
- [ ] DrawCommand struct: type, blend_mode, color, transform, params
- [ ] `an:layer(name)` creates layer with FBO at game resolution
- [ ] Per-layer transform stack (mat3 array, depth 32)
- [ ] Per-layer current blend mode
- [ ] Command array with dynamic growth
- [ ] Maximum 16 layers

### 3.2 Frame-End Renderer
- [ ] `render_frame()` — process all layers, then composite to screen
- [ ] `render_layer()` — iterate commands, build vertices, batch, flush
- [ ] Batch state tracking: current texture, current blend mode
- [ ] Flush batch on: texture change, blend change, buffer full
- [ ] Vertex building from DrawCommand (apply captured transform)
- [ ] Reset command queue after rendering each layer
- [ ] Composite layers to screen (fullscreen quads with layer textures)

### 3.3 Transform Stack
- [ ] `layer:push(x, y, r, sx, sy)` — push transform onto layer's stack
- [ ] `layer:pop()` — pop transform
- [ ] Current transform captured into DrawCommand at record time
- [ ] Transform applied when building vertices at frame end

### 3.4 Basic SDF Shader
- [ ] Vertex format: position, UV, color, mode, params
- [ ] Mode branching: CIRCLE, RECTANGLE, SPRITE
- [ ] Circle SDF: `length(uv - 0.5) - radius`
- [ ] Rectangle SDF: box distance function

### 3.5 Basic Shapes
- [ ] `layer:circle(x, y, radius, color)` — filled circle via SDF
- [ ] `layer:rectangle(x, y, w, h, color)` — filled rectangle via SDF

### 3.6 Sprite System
- [ ] Texture loading via stb_image (`an:texture_load(path)`)
- [ ] `layer:draw_image(img, x, y, r, sx, sy, ox, oy, color)`
- [ ] Textured quads (mode = SPRITE, sample texture, multiply by color)
- [ ] Smooth rotation via transformed quad vertices

### 3.7 Blend Modes
- [ ] Alpha: `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
- [ ] Additive: `glBlendFunc(GL_SRC_ALPHA, GL_ONE)`
- [ ] `layer:set_blend_mode('alpha' | 'additive')`
- [ ] Blend mode stored per-command, batch flush on change

### 3.8 Lua Bindings
```lua
-- Layers
game = an:layer('game')
effects = an:layer('effects')

-- Shapes
game:circle(x, y, radius, color)
game:rectangle(x, y, w, h, color)

-- Sprites
local img = an:texture_load('player.png')
game:draw_image(img, x, y, r, sx, sy, ox, oy, color)

-- Transforms
game:push(x, y, r, sx, sy)
game:pop()

-- Blend modes
game:set_blend_mode('additive')
game:set_blend_mode('alpha')
```

### 3.9 Verification
- [ ] Circle and rectangle render correctly
- [ ] Sprites load and render with transforms
- [ ] Transform stack works (rotation, scale, nesting)
- [ ] Multiple layers composite correctly
- [ ] Blend modes (alpha, additive)
- [ ] Web build verification (WebGL 2.0)

**Deliverable:** Working layer system with deferred rendering, basic shapes (circle, rectangle), sprites, transforms, and blend modes.

---

## Phase 4: Effects (Shaders)

**Goal:** Post-processing effects on layers via fragment shaders.

### 4.1 Effect Shader Framework
- [ ] Post-process shader pipeline (render layer to texture, apply shader, output)
- [ ] Effect parameter passing (uniforms)
- [ ] Per-layer effect configuration

### 4.2 Built-in Effects
- [ ] **Outline** — edge detection, configurable color and thickness
- [ ] **Tint** — multiply or blend toward a color
- [ ] **Brightness** — multiply RGB values

### 4.3 Custom Shaders
- [ ] Load custom fragment shaders from file
- [ ] Expose to Lua: `layer:set_shader(shader, params)`
- [ ] Shader hot-reload during development (optional, nice-to-have)

### 4.4 Lua Bindings
```lua
game:set_effect('outline', {color = 0x000000FF, thickness = 1})
game:set_effect('tint', {color = 0xFF0000FF, mix = 0.5})
game:set_effect('brightness', {factor = 1.5})
game:clear_effect()

-- Custom shader
local custom = an:shader_load('effects/custom.frag')
game:set_shader(custom, {time = t, intensity = 0.5})
```

**Deliverable:** Post-processing effects including custom shaders.

---

## Phase 5: Input

**Goal:** Action-based input with keyboard and mouse.

### 5.1 Input State Tracking
- [ ] Keyboard state (down, pressed this frame, released this frame)
- [ ] Mouse state (position, buttons, wheel)
- [ ] Per-frame state transitions

### 5.2 Binding System
- [ ] Action → input mapping
- [ ] Multiple inputs per action
- [ ] Input string parsing: `key:a`, `key:space`, `mouse:1`, `mouse:wheel_up`

### 5.3 Lua Bindings
```lua
an:input_bind('move_left', {'key:a', 'key:left'})
an:input_bind('move_right', {'key:d', 'key:right'})
an:input_bind('shoot', {'mouse:1', 'key:space'})

if an:is_pressed('shoot') then ... end   -- just pressed this frame
if an:is_down('move_left') then ... end  -- currently held
if an:is_released('shoot') then ... end  -- just released this frame

local mx, my = an:mouse_position()  -- screen coordinates
-- World position computed in YueScript using camera transform
```

**Deliverable:** Keyboard and mouse input with action bindings.

---

## Phase 6: Audio

**Goal:** Sound effects with pitch control, music playback.

### 6.1 Library Selection (Research Required)

**Options to evaluate:**

| Library | Pitch Shifting | Effects | Complexity | Web Support |
|---------|---------------|---------|------------|-------------|
| miniaudio | Yes (resampling) | Basic | Single header | Yes (Emscripten) |
| SoLoud | Yes (native) | Filters, 3D | Medium | Yes |
| SDL_mixer | Limited | None | Simple | Yes |

**Decision criteria:**
- Must support pitch shifting immediately
- Should support additional effects eventually (reverb, filters)
- Must work with Emscripten for web builds

### 6.2 Core Audio
- [ ] Audio device initialization
- [ ] Sound loading (WAV, OGG)
- [ ] Music loading (OGG)
- [ ] Web: handle audio context unlock (requires user interaction)

### 6.3 Playback Features
- [ ] Sound playback with volume
- [ ] **Pitch shifting** (essential)
- [ ] Music playback (loop, stop, fade)
- [ ] Master volume control

### 6.4 Lua Bindings
```lua
local hit = an:sound_load('hit.ogg')
an:sound_play(hit)
an:sound_play(hit, {volume = 0.5, pitch = 1.2})

local bgm = an:music_load('bgm.ogg')
an:music_play(bgm, {loop = true})
an:music_stop()
an:music_volume(0.5)
an:music_fade_out(1.0)  -- fade over 1 second
```

**Deliverable:** Audio with pitch shifting.

---

## Phase 7: Physics

**Goal:** Box2D 3.1 with sensor/contact events and spatial queries.

### 7.1 World Setup
- [ ] Box2D world creation
- [ ] Gravity configuration
- [ ] Fixed timestep stepping (synced with game loop)

### 7.2 Body Management
- [ ] Body creation: static, dynamic, kinematic
- [ ] Shape types: circle, rectangle, polygon
- [ ] Return body ID to Lua (Lua manages lifetime)

### 7.3 Collision Configuration
- [ ] Collision tag system
- [ ] Enable/disable contact between tag pairs
- [ ] Enable/disable sensor between tag pairs

### 7.4 Event System
Box2D 3.1 provides:
- **Sensor events** — overlap detection, no physics response
- **Contact events** — physical collision with response
- **Hit events** — high-speed impact data

Implementation:
- [ ] Buffer sensor enter/exit events per frame
- [ ] Buffer contact enter/exit events per frame
- [ ] Buffer contact hit events per frame
- [ ] Query functions return arrays of events

### 7.5 Spatial Queries
- [ ] `query_circle(x, y, radius, tags)` — find bodies in circle
- [ ] `query_aabb(x, y, w, h, tags)` — find bodies in rectangle
- [ ] `raycast_closest(x1, y1, x2, y2, tags)` — first hit
- [ ] `raycast(x1, y1, x2, y2, tags)` — all hits

### 7.6 Lua Bindings
```lua
an:physics_set_gravity(0, 500)
an:physics_enable_contact_between('ball', {'wall', 'paddle'})
an:physics_enable_sensor_between('player', {'pickup', 'trigger'})

-- Per-frame event queries
for _, c in ipairs(an:physics_get_contact_enter('ball', 'wall')) do
    local ball, wall = c.a, c.b
    -- Handle collision
end

for _, s in ipairs(an:physics_get_sensor_enter('player', 'pickup')) do
    local player, pickup = s.a, s.b
    pickup:collect()
end

for _, h in ipairs(an:physics_get_contact_hit('ball', 'wall')) do
    local speed = h.approach_speed
    an:sound_play(bounce, {volume = speed / 100})
end

-- Spatial queries
local nearby = an:physics_query_circle(x, y, 50, {'enemy'})
local hit = an:physics_raycast_closest(x1, y1, x2, y2, {'wall'})

-- Current sensor overlaps (not just enter/exit events)
local overlaps = an:physics_get_sensor_overlaps(sensor_collider)

-- Body management
local body = an:physics_create_body('ball', 'dynamic', 'circle', 10)
an:physics_set_position(body, x, y)
an:physics_set_velocity(body, vx, vy)
an:physics_apply_impulse(body, ix, iy)
an:physics_destroy_body(body)
```

**Deliverable:** Full physics with events and queries.

---

## Phase 8: Random

**Goal:** Seedable PRNG for deterministic gameplay.

### 8.1 Implementation
- [ ] PCG or xorshift PRNG (fast, good quality)
- [ ] Seed function
- [ ] State can be saved/restored for replays

### 8.2 Lua Bindings
```lua
an:random_seed(12345)
local x = an:random_float(0, 100)      -- [0, 100)
local i = an:random_int(1, 10)         -- [1, 10] inclusive
local r = an:random_angle()            -- [0, 2π)
local s = an:random_sign()             -- -1 or 1
local b = an:random_bool()             -- true or false
local item = an:random_choice(array)   -- random element
```

**Deliverable:** Deterministic random.

---

## Phase 9: Text Rendering

**Goal:** TTF fonts with per-character effects (handled in YueScript).

### 9.1 Font Loading (C Side)
- [ ] Load TTF via stb_truetype
- [ ] Bake glyphs to texture atlas at specified size
- [ ] Support multiple sizes per font (separate atlases)
- [ ] Store glyph metrics (advance, bearing, size)

### 9.2 Glyph Rendering (C Side)
- [ ] Draw single glyph at position with transform and color
- [ ] Return glyph metrics to Lua for layout

### 9.3 Text Effects (YueScript Side)
Text effects are computed in YueScript, which calls C to draw individual glyphs:

```lua
-- C provides:
an:font_load('default', 'font.ttf', 24)
local metrics = an:font_get_metrics('default', 'A')  -- {width, height, advance, ...}
layer:draw_glyph('default', 'A', x, y, r, sx, sy, color)

-- YueScript builds text effect system on top:
-- Per-character positioning, wave, shake, color, timing, etc.
```

### 9.4 Lua Bindings (Simple Text)
```lua
-- Simple text (no effects)
layer:draw_text('Hello', 'default', x, y, color)
layer:draw_text('Score: 100', 'large', x, y, r, sx, sy, color)
an:font_get_text_width('default', 'Hello')
```

**Deliverable:** Font loading and glyph rendering. Text effects built in YueScript.

---

## Phase 10: YueScript Object System

**Goal:** Full object tree with operators, timers, springs, colliders.

### 10.1 YueScript Build Integration
- [ ] Install YueScript compiler
- [ ] Add compilation step to build.bat
- [ ] Use `-r` flag for line number preservation
- [ ] Verify errors report correct .yue line numbers

### 10.2 Base Object Class
Implement in `yue/object.yue`:
- Constructor with name/args
- Tree operations: `add`, `remove`, `kill`
- Tagging: `tag`, `is`, `all` / `A`
- Links: `link` for horizontal dependencies
- Operators: `^`, `/`, `+`, `>>`
- Phase helpers: `U` (early), `L` (late), `X` (named)
- `__inherited` hook to propagate operators to child classes

**Ask developer before implementing details.**

### Engine Object Pattern

Timer, spring, and collider are examples of **engine objects** — classes that wrap C-side resources. All engine objects follow this pattern:

1. They extend the base `object` class
2. They're instantiated and added as children: `@ + timer()`
3. They're named after themselves by default (e.g., `timer()` has `name = 'timer'`)
4. Parent accesses them via that name: `@timer:after(...)`
5. They implement `destroy()` to clean up C-side resources when killed

This pattern applies to any future engine objects (particles, audio sources, etc.).

### 10.3 Timer Object
- Constructor: `timer([name])` — name optional, defaults to 'timer'
- `after(delay, [name], callback)` — one-shot
- `every(interval, [name], callback)` — repeating
- `tween(duration, target, props, easing, [callback])` — interpolation
- Named callbacks can be cancelled/replaced

### 10.4 Spring Object
- Constructor: `spring([name], initial, stiffness, damping)` — name optional, defaults to 'spring'
- `pull(amount)` — displace from target
- `.x` property — current value (updated each frame)

### 10.5 Collider Object
- Constructor: `collider([name], tag, body_type, shape_type, ...)` — name optional, defaults to 'collider'
- Wraps Box2D body
- Methods: `get_position`, `set_velocity`, `apply_impulse`, etc.
- `destroy()` cleans up Box2D body when killed

### 10.6 Update Loop Integration
C calls into Lua each frame:
1. Early phase — all `early_update` and `/ U` actions
2. Main phase — all `update` and `/` actions
3. Late phase — all `late_update` and `/ L` actions
4. Cleanup — remove dead objects, call `destroy` hooks

### 10.7 Aliases
```yuescript
E = object
U = (name_or_fn, fn) -> if fn then {__early: name_or_fn, __fn: fn} else {__early: name_or_fn}
L = (name_or_fn, fn) -> if fn then {__late: name_or_fn, __fn: fn} else {__late: name_or_fn}
X = (name, fn) -> {[name]: fn}
-- A is method alias for all()
```

**Deliverable:** Full object system matching ANCHOR.md specification.

---

## Phase Summary

| Phase | Focus | Key Deliverable |
|-------|-------|-----------------|
| 1 | C Skeleton | OpenGL window + Lua + error handling |
| 2 | Web Build | Emscripten/WebGL parity |
| 3 | Rendering | Shapes, sprites, layers, blend modes |
| 4 | Effects | Post-process shaders (outline, tint, custom) |
| 5 | Input | Keyboard/mouse with action bindings |
| 6 | Audio | Sound/music with pitch shifting |
| 7 | Physics | Box2D 3.1 with events and queries |
| 8 | Random | Seedable PRNG |
| 9 | Text | TTF fonts, glyph rendering |
| 10 | YueScript | Object tree, timers, springs, colliders |

---

## Open Decisions

### Audio Library
**Status:** Research required

Compare miniaudio vs SoLoud:
- Pitch shifting capability
- Effect support (for future)
- Emscripten compatibility
- API simplicity

### Particle Optimization
**Status:** Deferred

If object-based particles become slow for dense effects (hundreds/thousands), consider:
- Instanced particle renderer in C
- Particle pools
- GPU particle system

### Gamepad Support
**Status:** Deferred to Steam release prep

SDL2 gamepad API is straightforward when needed.

---

## Deferred Features

Not implementing now, add later if needed:

- **Gamepad input** — Steam release prep
- **Steam Input** — Steam release prep
- **Hot reloading** — nice for iteration but not essential
- **Debug console/inspector** — print debugging is sufficient
- **Advanced audio effects** — reverb, filters (after basic audio works)
- **Save/load system** — core gameplay first
- **Networking** — too early to consider
- **Full UI system** — existing layout system sufficient for now

---

## Technical Notes

### Threading
Single-threaded game loop. Audio runs on its own thread (handled by audio library). No explicit threading in game code.

### Memory
Resources (textures, sounds, fonts) live forever once loaded. No unloading mechanism — games are small enough.

### Error Handling
LÖVE-style: errors display stacktrace on screen, application continues in error state. No automatic restart.

### Resolution
Per-game configurable base resolution (default 480×270). Aspect-ratio scaling to fill window with letterboxing when needed. Nearest-neighbor filtering preserves pixel look.

### Distribution
Single executable with all assets embedded. No external files needed.
