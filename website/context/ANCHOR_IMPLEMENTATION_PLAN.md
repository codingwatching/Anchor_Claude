# Anchor Implementation Plan

C engine with YueScript scripting, OpenGL rendering, targeting Windows and Web.

---

## Summary of Key Decisions

| Area | Decision | Rationale |
|------|----------|-----------|
| Renderer | OpenGL | Smooth rotation, additive blending, performance headroom, console-portable |
| Audio | miniaudio + stb_vorbis | Single-header, pitch shifting, OGG support |
| Physics | Box2D 3.1 | Already used, true ball-to-ball collisions needed |
| Scripting | Lua 5.4 + YueScript | Build-time compilation with `-r` flag for line numbers |
| Timestep | Fixed 120Hz physics, 60Hz render | Decoupled for pixel-perfect visuals with responsive input |
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
Anchor/
├── .claude/                # Claude Code config
├── docs/                   # Documentation (ANCHOR.md, etc.)
├── engine/                 # Engine code + builds
│   ├── src/
│   │   └── anchor.c        # Single monolithic C file
│   ├── include/            # Vendored headers (SDL2, Lua, glad, stb)
│   ├── lib/                # Vendored libraries (SDL2.lib)
│   ├── build/              # Windows build output (anchor.exe)
│   ├── build-web/          # Web build output (anchor.html, etc.)
│   ├── build.bat           # Windows build script
│   ├── build-web.sh        # Web build script (takes game folder arg)
│   ├── run-web.bat         # Run web build locally
│   └── shell.html          # Emscripten HTML template
├── test/                   # Test game folder
│   ├── main.lua            # Test entry point
│   └── assets/             # Test assets (images, sounds)
├── reference/              # Reference materials
│   ├── love-compare/       # LÖVE comparison project
│   └── *.md, *.yue         # Notes and examples
├── scripts/                # Utility scripts
└── website/                # Blog/website (pushed to Blot)
```

The engine takes a game folder as argument (like LÖVE):
- Windows: `./engine/build/anchor.exe test`
- Web: `./engine/build-web.sh ../test` (bundles at build time)

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
- [x] Decoupled timestep: 120Hz physics/input, 60Hz rendering
- [x] Delta time accumulator pattern (separate accumulators for physics and rendering)
- [x] Event polling once per frame iteration (before physics loop)
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
  layer_circle(game, ...)      → stores DrawCommand in game.commands[]
  layer_rectangle(game, ...)   → stores DrawCommand in game.commands[]

At frame end:
  For each layer:
    Process commands in order → build vertices → batch → flush
  Composite layers to screen
```

---

### Implementation Steps

**Step 1: Read existing code** ✓
- [x] Understand Phase 1 & 2 code (window, GL context, shaders, Lua)

**Step 2: Layer struct + single FBO** ✓
- [x] Layer struct: FBO, color texture, width/height, transform stack (32 deep)
- [x] `layer_create()` / `layer_destroy()` C functions
- [x] Replace hardcoded fbo/fbo_texture with `game_layer`
- [x] Initialize transform stack to identity at depth 0

**Step 3: DrawCommand + command queue (C only)** ✓
- [x] DrawCommand struct: type, blend_mode, color, transform (2x3), params
- [x] Command queue with dynamic growth (`layer_add_command`)
- [x] C functions: `layer_add_rectangle`, `layer_add_circle`, `layer_clear_commands`

**Step 4: Rectangle rendering + Lua bindings** ✓
- [x] Batch rendering system (`batch_flush`, `batch_add_vertex`, `batch_add_quad`)
- [x] `process_rectangle()` — transform vertices, add to batch
- [x] `layer_render()` — iterate commands, build vertices, flush
- [x] Lua bindings: `layer_create()`, `layer_rectangle()`, `rgba()`
- [x] Verified on Windows and Web

**Step 5: Circle with SDF uber-shader** ✓
- [x] Expand vertex format to 13 floats: x, y, u, v, r, g, b, a, type, shape[4]
- [x] Update VAO with 5 vertex attributes (pos, uv, color, type, shape)
- [x] Uber-shader with SDF functions and type branching (RECT, CIRCLE, SPRITE)
- [x] Rectangle SDF: `sdf_rect(p, center, half_size)`
- [x] Circle SDF: `sdf_circle(p, center, radius)`
- [x] `process_circle()` and Lua binding `layer_circle()`
- [x] Verified on Windows

**Step 5b: Shape filter modes (smooth/rough)** ✓
- [x] Global filter mode: `FILTER_SMOOTH` (anti-aliased) vs `FILTER_ROUGH` (pixel-perfect)
- [x] Shader uniform `u_aa_width`: 1.0 for smooth (smoothstep), 0.0 for rough (step)
- [x] Lua binding: `set_shape_filter("smooth")` / `set_shape_filter("rough")`
- [x] Rough mode features for pixel-art aesthetic:
  - Fragment position snapped to pixel centers: `floor(vPos) + 0.5`
  - Circle center snapped to pixel grid: `floor(center) + 0.5`
  - Circle radius snapped to integer: `floor(radius + 0.5)`
  - Superellipse SDF (n=1.95) for characteristic pixel-circle "cardinal bumps"

**Step 5c: Pixel-perfect screen scaling** ✓
- [x] Integer-only scaling for screen blit (no fractional scaling)
- [x] Ensures each game pixel maps to exactly NxN screen pixels
- [x] Prevents interpolation artifacts at non-integer scales

**Step 5d: Main loop fixes** ✓
- [x] Event processing moved outside fixed timestep loop (always responsive)
- [x] Command queue cleared at START of update, not end of render
- [x] Fixes flickering when no fixed update runs in a frame (previous commands persist)

**Step 6: Transform stack (push/pop)** ✓
- [x] `mat3_multiply()` — 3x3 matrix multiplication for composing transforms
- [x] `layer_push(layer, x, y, r, sx, sy)` — build TRS matrix, multiply with current
  - Order is Scale → Rotate → Translate when applied to points (standard non-shearing order)
  - All parameters optional with defaults (x=0, y=0, r=0, sx=1, sy=1)
- [x] `layer_pop(layer)` — decrement depth with underflow warning
- [x] Lua bindings: `layer_push()`, `layer_pop()`
- [x] UV-space SDF approach for rotation support:
  - Instead of world-space center, pass quad size to shader
  - Compute local position from UV: `local_p = vUV * quad_size`
  - Center always at `quad_size * 0.5` (shape centered in quad)
  - Rotation handled implicitly by UV interpolation (no extra vertex data needed)
- [x] Verified with comprehensive test (nested transforms, orbits, non-uniform scale, corner pivots)
- [x] Matching LÖVE test created for visual comparison
- [x] Fixed LÖVE `push_trs` to use same transform order (TRS, not TSR)

**Step 7: Sprites (texture loading, draw_image)** ✓
- [x] Texture loading via stb_image: `texture_load(path)`
- [x] `texture_get_width(tex)`, `texture_get_height(tex)` — query texture dimensions
- [x] `layer_draw_texture(layer, tex, x, y)` — draws sprite centered at position
- [x] SPRITE mode in shader (sample texture at texel centers, multiply by color)
- [x] Texel center snapping for pixel-perfect sprite rendering
- [x] Batch flush on texture change
- [x] Verified with bouncing emoji + orbiting stars test (transforms work with sprites)
- [x] Matching LÖVE comparison test created

**Step 8: Blend modes** ✓
- [x] `layer_set_blend_mode(layer, mode)` — 'alpha' or 'additive'
- [x] Blend mode stored per-command (via layer's current_blend)
- [x] Batch flush on blend mode change
- [x] Apply blend state before drawing batch
- [x] `apply_blend_mode()` helper function for GL state management
- [x] Verified on Windows and Web

**Step 9: Multiple layers + composition** ✓
- [x] Layer registry (max 16 layers, stored with names for lookup)
- [x] `layer_create(name)` creates/retrieves named layer (idempotent)
- [x] Layer ordering for composition (creation order: first = bottom, last = top)
- [x] Composite all layers to screen at frame end (each layer rendered to FBO, then blitted with alpha)

**Step 10: Frame timing improvements** ✓
- [x] Analysis against Tyler Glaiel's "How to make your game run at 60fps" article
- [x] Built monitor simulator tool (`tools/monitor_sim.c`) to test timing algorithms
- [x] Changed physics rate from 144Hz to 120Hz (divides evenly into 60/120/240Hz monitors)
- [x] Added VSync snapping (0.2ms tolerance) to eliminate timer jitter drift
- [x] Added delta time clamping before accumulator (handles pause/resume, debugger)
- [x] Added render_lag cap at 2× RENDER_RATE (prevents unbounded growth)
- [x] Render rate limiting at 60Hz for consistent chunky pixel movement on all monitors
- [x] Verified via simulator: consistent 2 physics updates per rendered frame on 60Hz, 59.94Hz, 144Hz, 240Hz monitors
- [x] See `reference/frame-timing-analysis.md` for detailed analysis

### Lua API (C bindings)

```lua
-- Layer management
game = layer_create('game')

-- Shapes
layer_rectangle(game, x, y, w, h, color)
layer_circle(game, x, y, radius, color)

-- Transforms
layer_push(game, x, y, r, sx, sy)
layer_pop(game)

-- Sprites
local img = texture_load('player.png')
layer_draw_image(game, img, x, y, r, sx, sy, ox, oy, color)

-- Blend modes ('alpha' or 'additive')
layer_set_blend_mode(game, 'additive')
layer_set_blend_mode(game, 'alpha')

-- Color helper
local red = rgba(255, 0, 0, 255)
```

### Verification
- [x] Rectangle renders correctly (Step 4)
- [x] Circle renders correctly with SDF (Step 5)
- [x] Transform stack works (rotation, scale, nesting) (Step 6)
- [x] Sprites load and render (Step 7)
- [x] Blend modes work (alpha, additive) (Step 8)
- [x] Multiple layers composite correctly (Step 9)
- [x] Frame timing produces consistent updates across monitor refresh rates (Step 10)
- [x] Steps 1-10 verified on Windows and Web

**Deliverable:** Working layer system with deferred rendering, basic shapes (circle, rectangle), sprites, transforms, blend modes, and rock-solid frame timing. ✓ Complete

---

## Phase 4: Effects (Shaders)

**Goal:** Post-processing effects on layers and per-object flash effect.

See `reference/phase-4-effects-analysis.md` for full technical analysis.

### Architecture Overview

**Lua-controlled pipeline:** C provides shader primitives, Lua orchestrates the rendering pipeline.

**Ping-pong buffer system:** Each layer has two textures (`color_texture` and `effect_texture`). When `layer_apply_shader()` is called, it renders from one to the other and swaps. This allows effect chaining.

**Per-object flash via vertex attributes:** Instead of shader swapping (which breaks batching), flash color is passed as a per-vertex attribute. 50 flashing objects = 1 batched draw call.

---

### Implementation Steps

**Step 1: Shader loading infrastructure** ✓
- [x] `shader_load_file(path)` — load fragment shader from file, pair with screen vertex shader
- [x] `shader_load_string(source)` — load fragment shader from string
- [x] `shader_destroy(shader)` — cleanup
- [x] Auto-prepend platform headers (`#version 330 core` / `#version 300 es`)
- [x] Lua bindings for above

**Step 2: Uniform setting (deferred)** ✓
- [x] `layer_shader_set_float(layer, shader, name, value)` — queues command
- [x] `layer_shader_set_vec2(layer, shader, name, x, y)` — queues command
- [x] `layer_shader_set_vec4(layer, shader, name, x, y, z, w)` — queues command
- [x] `layer_shader_set_int(layer, shader, name, value)` — queues command
- [x] Uniforms processed at frame end (deferred, not immediate)
- [x] Lua bindings for above

**Step 3: Layer ping-pong buffers** ✓
- [x] Add to Layer struct: `effect_fbo`, `effect_texture`, `textures_swapped`
- [x] Create effect FBO on first use (lazy initialization)
- [x] `layer_apply_shader(layer, shader)` — queues command (deferred)
- [x] Ping-pong rendering: color_texture ↔ effect_texture
- [x] `layer_get_texture(layer)` — return whichever texture is current
- [x] Reset `textures_swapped` at start of each frame
- [x] Lua bindings for above

**Step 4: Per-object flash (vertex attribute)** ✓
- [x] Extend vertex format: add `addR, addG, addB` (3 floats, total 16 floats per vertex)
- [x] Update VAO setup with new attribute (location 5)
- [x] Update uber-shader: `FragColor.rgb = texColor.rgb * vColor.rgb + vAddColor`
- [x] Shapes draw with flash = (0, 0, 0) by default

**Step 5: Example shaders** ✓
- [x] Create `test/shaders/` folder with example effect shaders
- [x] `outline.frag` — 5x5 neighbor sampling, detects alpha edges, outputs black outline
- [x] `shadow.frag` — outputs gray (0.5, 0.5, 0.5) with 50% alpha for drop shadow effect
- [x] Test shaders work on both Windows and Web

**Step 6: Manual layer compositing** ✓
- [x] `layer_draw(layer, x, y)` — queue layer for manual compositing with offset
- [x] Screen shader supports `u_offset` uniform for shadow positioning
- [x] If manual queue used, automatic layer compositing is skipped
- [x] Enables shadow offset pattern: `layer_draw(shadow_layer, 4, 4)`

**Step 7: Integration test** ✓
- [x] Test outline effect on game layer (5x5 sampling for thick outline)
- [x] Test shadow effect (draw shadow layer with offset)
- [x] Test multi-layer compositing (bg, shadow, outline, game layers)
- [x] Verify all effects work on Windows and Web

---

### Lua API

```lua
-- Shader loading
local outline_shader = shader_load_file('shaders/outline.frag')
local shadow_shader = shader_load_file('shaders/shadow.frag')

-- Deferred uniform setting (queues command, processed at frame end)
layer_shader_set_vec2(outline_layer, outline_shader, 'u_pixel_size', 1/480, 1/270)

-- Layer post-processing (deferred, ping-pong)
layer_apply_shader(shadow_layer, shadow_shader)
layer_apply_shader(outline_layer, outline_shader)

-- Manual layer compositing with offset (for shadow positioning)
layer_draw(bg_layer)
layer_draw(shadow_layer, 4, 4)  -- offset shadow by 4 pixels
layer_draw(outline_layer)
layer_draw(game_layer)

-- Get layer texture (for use as input to another shader)
local tex = layer_get_texture(game)
```

**Key architecture decisions:**
- All shader operations are deferred (commands queued during update, executed at frame end)
- Uniform commands processed inline during layer rendering
- Manual layer compositing skips automatic compositing when used
- Screen shader accepts `u_offset` uniform for layer positioning

---

### Verification
- [x] Shader loads and compiles on Windows
- [x] Shader loads and compiles on Web (WebGL)
- [x] Deferred uniforms are set correctly
- [x] `layer_apply_shader()` ping-pong works (single effect)
- [x] `layer_apply_shader()` chaining works (multiple effects)
- [x] Per-object flash vertex attribute in uber-shader
- [x] Outline shader produces correct visual result (5x5 neighbor sampling)
- [x] Shadow shader produces correct visual result (gray with offset)
- [x] Manual layer compositing with offset support

**Deliverable:** Post-processing effects (outline, shadow) and per-object flash. ✓ Complete

---

## Phase 5: Input

**Goal:** Action-based input system with keyboard, mouse, and gamepad support. Includes advanced features: chords, sequences, holds, input type detection, and rebinding capture.

See `reference/input-system-research.md` for research on input systems across 17+ game engines.

---

### Architecture Overview

**Action-based input:** Physical inputs (keys, buttons) map to named actions. Game code queries actions, not raw keys.

**Control string format:** `type:key` — e.g., `'key:space'`, `'mouse:1'`, `'button:a'`, `'axis:leftx+'`

**Unified query system:** Actions, chords, sequences, and holds all use the same `is_pressed`/`is_down`/`is_released` functions.

**Edge detection:** Standard previous/current frame comparison for pressed (just this frame) and released (just this frame).

---

### Implementation Steps

**Step 1: Raw keyboard state**
- [x] Internal state arrays: `keys_current[NUM_KEYS]`, `keys_previous[NUM_KEYS]`
- [x] SDL event handling: `SDL_KEYDOWN`, `SDL_KEYUP` update `keys_current`
- [x] End of frame: copy `keys_current` to `keys_previous`
- [x] `key_is_down(key)` — returns `keys_current[scancode]`
- [x] `key_is_pressed(key)` — returns `current && !previous`
- [x] `key_is_released(key)` — returns `!current && previous`
- [x] Key string parsing: `'a'`, `'space'`, `'left'`, `'lshift'`, etc. → SDL scancodes

**Step 2: Mouse state**
- [x] Track: position (x, y), delta (dx, dy), buttons (1-5), wheel
- [x] SDL events: `SDL_MOUSEMOTION`, `SDL_MOUSEBUTTONDOWN/UP`, `SDL_MOUSEWHEEL`
- [x] `mouse_position()` — returns x, y in game coordinates (scaled from window)
- [x] `mouse_delta()` — returns dx, dy this frame
- [x] `mouse_wheel()` — returns wheel x, y delta this frame
- [x] `mouse_is_pressed/down/released(button)` — edge detection for buttons
- [x] `mouse_set_visible(bool)` — show/hide cursor
- [x] `mouse_set_grabbed(bool)` — lock cursor to window (for FPS-style controls)

**Step 3: Basic action binding**
- [x] Action struct: name, array of controls, pressed/down/released state
- [x] Control struct: type (KEY, MOUSE, BUTTON, AXIS), code, sign (for axes)
- [x] `input_bind(action, control_string)` — parse control string, add to action
- [x] `is_pressed(action)` — true if any bound control just pressed
- [x] `is_down(action)` — true if any bound control held
- [x] `is_released(action)` — true if any bound control just released
- [x] Per-frame action state computation from raw input states

**Step 4: Unbinding and bind_all**
- [x] `input_unbind(action, control)` — remove specific control from action
- [x] `input_unbind_all(action)` — remove all controls from action
- [x] `input_bind_all()` — bind every key/button to action with same name
  - All keyboard keys → `'a'`, `'space'`, `'left'`, etc.
  - Mouse buttons → `'mouse_1'`, `'mouse_2'`, `'mouse_3'`
  - Gamepad buttons → `'button_a'`, `'button_b'`, `'button_x'`, etc.

**Step 5: Axis helpers**
- [x] `input_get_axis(negative, positive)` — returns -1 to 1
  - Digital: `is_down(positive) - is_down(negative)`
  - Analog: uses actual axis values if bound to gamepad axis
- [x] `input_get_vector(left, right, up, down)` — returns x, y
  - Normalized to prevent faster diagonal movement
  - `len = sqrt(x*x + y*y); if len > 1 then x, y = x/len, y/len`

**Step 6: Gamepad support**
- [x] SDL GameController initialization and hotplug handling
- [x] `gamepad_is_connected()` — true if at least one gamepad connected
- [x] `gamepad_get_axis(axis)` — raw axis value (-1 to 1) with deadzone
  - Axes: `'leftx'`, `'lefty'`, `'rightx'`, `'righty'`, `'triggerleft'`, `'triggerright'`
- [x] `input_set_deadzone(value)` — threshold for axis→button conversion (default 0.2)
- [x] Gamepad button/axis state tracking (current/previous)
- [x] Control string parsing: `'button:a'`, `'button:start'`, `'axis:leftx+'`, `'axis:lefty-'`

**Step 7: Chords**
- [x] Chord struct: name, array of action names, pressed/down/released state
- [x] `input_bind_chord(name, actions)` — e.g., `('sprint_jump', {'shift', 'space'})`
- [x] Chord is down when ALL actions are down
- [x] Chord pressed when it becomes down (wasn't down last frame)
- [x] Chord released when it stops being down
- [x] Chords queryable via `is_pressed`/`is_down`/`is_released` (same namespace as actions)

**Step 8: Sequences**
- [x] Sequence struct: name, array of {action, delay} pairs, state machine
- [x] `input_bind_sequence(name, sequence)` — e.g., `('dash', {'d', 0.3, 'd'})`
- [x] State machine tracks: current step, last press time
- [x] Advances when next action pressed within time window
- [x] Resets if timeout or wrong action pressed
- [x] Sequence fires (is_pressed returns true) when final action completes in time
- [x] Sequences queryable via `is_pressed`/`is_down`/`is_released`
- [x] Bug fix: use `lua_type() == LUA_TSTRING` instead of `lua_isstring()` (numbers are convertible)

**Step 9: Holds**
- [x] Hold struct: name, source action, duration, state (waiting/triggered)
- [x] `input_bind_hold(name, duration, action)` — e.g., `('charge', 1.0, 'space')`
- [x] Tracks how long source action has been held
- [x] `is_pressed` fires on the frame hold duration is reached
- [x] `is_down` true while held after duration reached
- [x] `is_released` fires when released after duration was reached
- [x] Resets when source action released before duration
- [x] `input_get_hold_duration(name)` — returns current hold progress (for charge bar UI)
- [x] Holds queryable via `is_pressed`/`is_down`/`is_released`

**Step 10: Input type detection**
- [x] Track last input type: `'keyboard'`, `'mouse'`, or `'gamepad'`
- [x] Updated whenever any input is received (key press, mouse move/click, gamepad)
- [x] `input_get_last_type()` — returns current type string
- [x] Useful for UI prompt switching (show keyboard vs gamepad icons)

**Step 11: Rebinding capture**
- [x] Capture mode flag
- [x] `input_start_capture()` — enter capture mode, suppress normal input processing
- [x] While in capture mode: wait for any key/button/axis press
- [x] Store captured control as string (e.g., `'key:w'`, `'button:a'`)
- [x] `input_get_captured()` — returns control string if captured, nil otherwise
- [x] `input_stop_capture()` — exit capture mode, clear captured value

**Step 12: Utility**
- [x] `input_any_pressed()` — true if any bound action was just pressed this frame
- [x] `input_get_pressed_action()` — returns action name that was pressed (or nil)

---

### Lua API

#### Action Binding

```lua
-- Bind controls to actions (call multiple times to add multiple controls)
input_bind('action_name', 'control_string')

-- Control string formats:
--   'key:a', 'key:space', 'key:lshift', 'key:up', 'key:1'
--   'mouse:1' (left), 'mouse:2' (middle), 'mouse:3' (right)
--   'button:a', 'button:b', 'button:x', 'button:y'
--   'button:dpup', 'button:dpdown', 'button:dpleft', 'button:dpright'
--   'button:leftshoulder', 'button:rightshoulder'
--   'button:leftstick', 'button:rightstick', 'button:start', 'button:back'
--   'axis:leftx+', 'axis:leftx-', 'axis:lefty+', 'axis:lefty-'
--   'axis:rightx+', 'axis:rightx-', 'axis:righty+', 'axis:righty-'
--   'axis:triggerleft', 'axis:triggerright'

-- Example: bind movement to WASD + arrows + left stick + dpad
input_bind('move_up', 'key:w')
input_bind('move_up', 'key:up')
input_bind('move_up', 'axis:lefty-')
input_bind('move_up', 'button:dpup')

-- Unbind specific control or all controls
input_unbind('action_name', 'control_string')
input_unbind_all('action_name')

-- Bind all keys/buttons to same-named actions (useful for rebinding UI)
input_bind_all()  -- 'a' -> 'key:a', 'space' -> 'key:space', etc.
                  -- Also binds mouse_1, mouse_2, mouse_3
                  -- Also binds button_a, button_b, button_x, button_y, etc.
```

#### Action Queries

```lua
-- These work for regular actions, chords, sequences, and holds
if is_pressed('action') then end   -- true for one frame when pressed
if is_down('action') then end      -- true while held
if is_released('action') then end  -- true for one frame when released
```

#### Axis Helpers

```lua
-- Get axis value from two opposing actions (-1, 0, or 1)
local move_x = input_get_axis('move_left', 'move_right')

-- Get normalized 2D vector (diagonal movement is ~0.707, not 1.414)
local vx, vy = input_get_vector('move_left', 'move_right', 'move_up', 'move_down')

-- Set gamepad deadzone (default 0.2)
input_set_deadzone(0.3)
```

#### Chords (simultaneous keys)

```lua
-- Chord triggers when ALL actions are held simultaneously
input_bind_chord('chord_name', {'action1', 'action2', ...})

-- Example: Shift+Space for super jump
input_bind_chord('super_jump', {'lshift', 'space'})

-- In update:
if is_pressed('super_jump') then
    -- Triggers once when both Shift AND Space are held
    player:super_jump()
end
```

#### Sequences (combos, double-tap)

```lua
-- Sequence triggers when actions are pressed in order within time windows
-- Format: {action1, delay1, action2, delay2, action3, ...}
-- delay = max seconds allowed before next action
input_bind_sequence('sequence_name', {action1, delay, action2, ...})

-- Example: double-tap D to dash (press D, then D again within 0.3s)
input_bind_sequence('dash', {'d', 0.3, 'd'})

-- Example: fighting game combo (down, down-right, right, punch within 0.5s each)
input_bind_sequence('hadouken', {'down', 0.5, 'downright', 0.5, 'right', 0.5, 'punch'})

-- In update:
if is_pressed('dash') then
    -- Triggers once when sequence completes
    player:dash()
end
```

#### Holds (charge attacks)

```lua
-- Hold triggers after holding source action for specified duration
input_bind_hold('hold_name', duration_seconds, 'source_action')

-- Example: hold Space for 1 second to charge
input_bind_hold('charge', 1.0, 'space')

-- Get current hold progress (for charge bar UI)
local duration = input_get_hold_duration('charge')  -- 0 to required_duration

-- In update:
local charge_time = input_get_hold_duration('charge')
if charge_time > 0 and charge_time < 1.0 then
    -- Show charge bar: charge_time / 1.0 = progress 0-1
    draw_charge_bar(charge_time / 1.0)
end

if is_pressed('charge') then
    -- Triggers once when hold duration reached
    player:release_charged_attack()
end

if is_released('charge') then
    -- Triggers if player releases before charge completes OR after
end
```

#### Mouse

```lua
local x, y = mouse_position()    -- game coordinates (scaled to game resolution)
local dx, dy = mouse_delta()     -- raw pixel movement this frame
local wx, wy = mouse_wheel()     -- scroll wheel delta this frame

-- Raw mouse button queries
if mouse_is_pressed(1) then end  -- left click just pressed
if mouse_is_down(1) then end     -- left button held
if mouse_is_released(1) then end -- left click just released
-- Button 1 = left, 2 = middle, 3 = right

mouse_set_visible(false)
mouse_set_grabbed(true)  -- confine to window
```

#### Keyboard (raw)

```lua
-- Raw key queries (bypass action system)
if key_is_pressed('space') then end   -- just pressed
if key_is_down('w') then end          -- held
if key_is_released('escape') then end -- just released

-- Key names: a-z, 0-9, space, enter, escape, tab, backspace,
-- up, down, left, right, lshift, rshift, lctrl, rctrl, lalt, ralt,
-- f1-f12, etc.
```

#### Gamepad

```lua
if gamepad_is_connected() then
    -- Raw axis values (-1.0 to 1.0, with deadzone applied)
    local lx = gamepad_get_axis('leftx')
    local ly = gamepad_get_axis('lefty')
    local rx = gamepad_get_axis('rightx')
    local ry = gamepad_get_axis('righty')
    local lt = gamepad_get_axis('triggerleft')   -- 0 to 1
    local rt = gamepad_get_axis('triggerright')  -- 0 to 1
end

-- Gamepad buttons are accessed via action system:
-- After input_bind_all(), use is_pressed('button_a'), is_pressed('button_x'), etc.
```

#### Input Type Detection

```lua
-- Returns 'keyboard', 'mouse', or 'gamepad'
-- Updates whenever user provides input
local input_type = input_get_last_type()

-- Example: switch UI prompts based on input device
if input_type == 'gamepad' then
    show_prompt("Press A to continue")
else
    show_prompt("Press Space to continue")
end
```

#### Rebinding Capture

```lua
-- For options menu: capture next input to rebind a control

-- Start capture mode (suppresses normal input, waits for any key/button/axis)
input_start_capture()

-- Check if something was captured (returns control string or nil)
local captured = input_get_captured()  -- e.g., 'key:space', 'button:a', 'axis:leftx+'

-- Stop capture mode
input_stop_capture()

-- Example: rebinding UI
local rebinding = nil  -- action being rebound, or nil

function start_rebind(action_name)
    rebinding = action_name
    input_start_capture()
    show_message("Press a key or button...")
end

function update(dt)
    if rebinding then
        local captured = input_get_captured()
        if captured then
            input_unbind_all(rebinding)      -- clear old bindings
            input_bind(rebinding, captured)  -- apply new binding
            input_stop_capture()
            rebinding = nil
            show_message("Bound!")
        end
    end
end
```

#### Utility

```lua
-- True if ANY bound action was just pressed this frame
if input_any_pressed() then
    -- "Press any key to continue"
end

-- Get the name of the action that was pressed (or nil)
local action = input_get_pressed_action()
if action then
    print("You pressed: " .. action)
end
```

---

### Verification (test each step individually)

- [x] Step 1: `key_is_pressed('space')` fires once on press, `key_is_down` while held
- [x] Step 2: `mouse_position()` returns correct game coords, `mouse_delta()` tracks movement
- [x] Step 3: `input_bind` + `is_pressed` works with multiple controls per action
- [x] Step 4: `input_unbind` removes control, `input_bind_all` creates all key actions
- [x] Step 5: `input_get_axis` returns -1/0/1, `input_get_vector` normalizes diagonal
- [x] Step 6: Gamepad detected, `gamepad_get_axis` returns analog values
- [x] Step 7: Chord fires only when all actions held simultaneously
- [x] Step 8: Sequence fires only when actions pressed in order within time windows
- [x] Step 9: Hold fires only after holding source action for specified duration
- [x] Step 10: `input_get_last_type` updates correctly when switching input devices
- [x] Step 11: Capture mode captures next input, normal input suppressed during capture
- [x] Step 12: `input_any_pressed` detects any action press
- [x] All steps verified on Windows and Web

**Deliverable:** Full input system with actions, chords, sequences, holds, and rebinding support.

---

## Phase 6: Audio

**Goal:** Sound effects with pitch control, music playback, separate volume controls for sounds and music.

### Library Decision

**miniaudio** — single-header C library. Pitch shifting via resampling (changing playback rate). Works with Emscripten. Fits the monolithic anchor.c style.

### Architecture

**Volume:** Two separate master volumes (for options menu sliders)
- Sound effects: `per_play_volume × sound_master_volume`
- Music: `music_master_volume`

**Pitch:** Per-play pitch + global master pitch
- Per-play pitch: variation for each sound (e.g., 0.95-1.05 for natural variation)
- Master pitch: slow-mo effect, multiplies with per-play pitch
- Final pitch = `per_play_pitch × master_pitch`
- Real-time adjustment — master pitch change affects currently playing sounds

---

### Implementation Steps

**Step 1: Integrate miniaudio**
- [x] Download miniaudio.h to `engine/include/`
- [x] `#define MINIAUDIO_IMPLEMENTATION` in anchor.c
- [x] Add stb_vorbis.c for OGG/Vorbis decoding (header at top, implementation at end of file to avoid macro conflicts)
- [x] Rename `Chord` → `InputChord` and `shutdown` → `engine_shutdown` to avoid Windows header conflicts
- [x] Verify it compiles on Windows

**Step 2: Audio device initialization**
- [x] Initialize `ma_engine` (high-level API, handles mixing)
- [x] Shutdown on exit
- [x] Verify no errors on startup

**Step 3: Sound loading**
- [x] `sound_load(path)` — returns Sound userdata
- [x] Load WAV and OGG (via stb_vorbis)
- [x] Sound struct stores path; audio data cached by miniaudio's resource manager
- [x] Lua binding

**Step 4: Sound playback**
- [x] `sound_play(sound)` — play at volume=1, pitch=1
- [x] `sound_play(sound, volume, pitch)` — play with per-play volume and pitch
- [x] Per-play pitch for variation (e.g., random 0.95-1.05)
- [x] Sound instance pool (64 slots) with main-thread cleanup to avoid audio thread issues
- [x] Lua binding

**Step 5: Sound master volume**
- [x] `sound_set_volume(volume)` — 0 to 1, affects all sound effects
- [x] Store as global, apply when playing sounds
- [x] Lua binding

**Step 6: Music loading**
- [x] `music_load(path)` — returns Music userdata
- [x] Streaming playback (not fully loaded into memory)
- [x] Lua binding

**Step 7: Music playback**
- [x] `music_play(music)` — play once
- [x] `music_play(music, loop)` — loop if true
- [x] `music_stop()` — stop current music
- [x] Only one music track at a time
- [x] Lua bindings

**Step 8: Music master volume**
- [x] `music_set_volume(volume)` — 0 to 1
- [x] Lua binding

**Step 9: Master pitch (slow-mo)**
- [x] `audio_set_master_pitch(pitch)` — affects all audio
- [x] Multiplies with per-play pitch: `final = per_play_pitch × master_pitch`
- [x] Must work on currently playing sounds (real-time adjustment)
- [x] Lua binding

**Step 10: Perceptual volume scaling**
- [x] `linear_to_perceptual(linear)` helper — applies power curve (linear²)
- [x] Applied to sound_play, music_play, and music_set_volume
- [x] Human hearing is logarithmic; this makes sliders feel natural

**Step 11: Bug fix — number key scancodes**
- [x] SDL scancodes for 1-9 are sequential, then 0 (keyboard layout order, not 0-9)
- [x] Fixed `key_name_to_scancode` to handle this correctly

**Step 12: Web audio context unlock**
- [x] Handle browser requirement for user interaction before audio
- [x] Unlock on first input event (key/mouse/touch)
- [x] `audio_try_unlock()` called from SDL_KEYDOWN, SDL_MOUSEBUTTONDOWN, SDL_FINGERDOWN

**Step 13: Verification**
- [x] Test on Windows: sound effects with volume/pitch variation
- [x] Test on Windows: music looping
- [x] Test on Windows: slow-mo pitch effect
- [x] Test on Windows: perceptual volume scaling (triangle wave oscillation test)
- [x] Test on Web: audio context unlocks on interaction
- [x] Test on Web: all features match Windows

---

### Lua API

```lua
-- Loading
local hit = sound_load('hit.ogg')
local bgm = music_load('bgm.ogg')

-- Sound playback (fire-and-forget)
sound_play(hit)                                -- volume=1, pitch=1
sound_play(hit, 0.5, 1.2)                      -- volume=0.5, pitch=1.2
sound_play(hit, 1.0, 0.95 + random() * 0.1)   -- random pitch 0.95-1.05

-- Music (one track at a time)
music_play(bgm)                        -- play once
music_play(bgm, true)                  -- loop
music_stop()

-- Volume controls (for options menu sliders)
sound_set_volume(0.8)                  -- all sound effects
music_set_volume(0.5)                  -- music

-- Master pitch (for slow-mo, multiplies with per-play pitch)
audio_set_master_pitch(0.5)            -- all audio plays at half speed/pitch
audio_set_master_pitch(1.0)            -- back to normal
```

---

### Verification Checklist

- [x] Step 1-2: miniaudio initializes without errors
- [x] Step 3-4: Sound loads and plays with volume/pitch
- [x] Step 5: Sound master volume affects all sound effects
- [x] Step 6-7: Music loads, plays, loops, stops
- [x] Step 8: Music master volume works
- [x] Step 9: Master pitch affects all playing audio in real-time
- [x] Step 10: Perceptual volume scaling produces natural-feeling volume changes
- [x] Step 11: Number keys 1-9 and 0 work correctly
- [x] Step 12: Web audio context unlock implemented
- [x] Step 13: Web build tested and verified
- [x] All steps verified on Windows and Web

**Deliverable:** Audio with pitch shifting, separate volume controls, and perceptual scaling. ✓ Complete

---

## Phase 7: Physics

**Goal:** Box2D 3.1 with sensor/contact events and spatial queries.

### 7.1 World Setup
- [ ] Box2D world creation
- [ ] Gravity configuration
- [ ] Fixed timestep stepping (synced with game loop)
- [ ] Configurable pixel-to-meter scale (check old Anchor values for reference)

### 7.2 Body Management
- [ ] Body creation: static, dynamic, kinematic
- [ ] Shape types: circle, rectangle, polygon
- [ ] Return body ID to Lua (Lua manages lifetime)

### 7.3 Collision Configuration
- [ ] Collision tag system (string API for readability, maps to Box2D internally)
- [ ] Enable/disable contact between tag pairs
- [ ] Enable/disable sensor between tag pairs

**Note:** Start with collision detection only. Add joints (hinges, ropes, etc.) if/when needed for specific games.

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

**Goal:** Seedable PRNG for deterministic gameplay and replay support.

### 8.1 Implementation
- [ ] PCG or xorshift PRNG (fast, good quality)
- [ ] Seed function
- [ ] Save/restore RNG state (required for replays)
- [ ] Consider separate streams for gameplay vs cosmetic RNG

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

**Goal:** Font rendering with per-character effects (handled in YueScript).

### 9.1 Font Loading (C Side)
- [ ] Support both bitmap fonts (pixel-perfect) and TTF (scalable)
- [ ] TTF: Load via stb_truetype, bake glyphs to texture atlas
- [ ] Bitmap: Load from spritesheet with metrics file
- [ ] Support multiple sizes per font (separate atlases)
- [ ] Store glyph metrics (advance, bearing, size)
- [ ] Error screen shows full stack trace (file:line for each call)

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

**Sibling update order:** Creation order (first added = first updated).

**Timer callbacks:** Receive only `self` (may explore alternatives during implementation).

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

## Phase 11: Distribution & Polish

**Goal:** Packaging, web edge cases, and final robustness.

### 11.1 Asset Packaging
- [ ] Single executable with embedded assets (zip-append or similar)
- [ ] Asset extraction for modders (optional)

### 11.2 Web Build Polish
- [ ] Audio context unlock handling
- [ ] Gamepad support via SDL2/Emscripten
- [ ] Additional keyboard key capture (F1-F12, Tab, Backspace, etc.)
- [ ] Final cross-browser testing

### 11.3 Engine State Exposure
- [ ] Expose frame counters to Lua: `an.frame`, `an.step`, `an.game_time`

### 11.4 Robustness
- [ ] Final error handling review
- [ ] Edge case testing
- [ ] Performance profiling

**Deliverable:** Production-ready engine with single-exe distribution.

---

## Phase Summary

| Phase | Focus | Key Deliverable |
|-------|-------|-----------------|
| 1 | C Skeleton | OpenGL window + Lua + error handling |
| 2 | Web Build | Emscripten/WebGL parity |
| 3 | Rendering | Shapes, sprites, layers, blend modes |
| 4 | Effects | Post-process shaders (outline, shadow) + per-object flash |
| 5 | Input | Actions, chords, sequences, holds, gamepad, rebinding |
| 6 | Audio | Sound/music with pitch shifting |
| 7 | Physics | Box2D 3.1 with events and queries |
| 8 | Random | Seedable PRNG with replay support |
| 9 | Text | Bitmap + TTF fonts, glyph rendering |
| 10 | YueScript | Object tree, timers, springs, colliders |
| 11 | Distribution | Single-exe packaging, web polish, robustness |

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

---

## Deferred Features

Not implementing now, add later if needed:

- **Steam Input** — Steam release prep (beyond basic gamepad)
- **Hot reloading** — nice for iteration but not essential
- **Debug console/inspector** — print debugging is sufficient
- **Advanced audio effects** — reverb, filters (after basic audio works)
- **Save/load system** — core gameplay first
- **Networking** — too early to consider
- **Full UI system** — existing layout system sufficient for now

---

## Technical Notes

### Decoupled Timestep (Pixel-Perfect Rendering)

The engine uses decoupled physics and rendering rates to achieve pixel-perfect visuals with responsive input:

- **Physics/input**: 120Hz (PHYSICS_RATE) — responsive feel, precise collision
- **Rendering**: 60Hz (RENDER_RATE) — exactly 2 physics updates per rendered frame

**Why 120Hz:** It divides evenly into 60Hz (2:1), 120Hz (1:1), and 240Hz (1:2). This avoids the 2-2-3 stutter pattern that 144Hz would cause on 60Hz monitors. The render rate cap ensures consistent chunky pixel-art movement regardless of monitor refresh rate.

**Shader-level snapping** (still in use):
- Sprites: texel center snapping for crisp pixels
- Circles: radius snapping in rough mode for consistent shapes

**C-level position snapping**: Not needed with decoupled rates. Was tried but caused jagged diagonal movement.

See `reference/pixel-perfect-research.md` for full investigation details.

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
