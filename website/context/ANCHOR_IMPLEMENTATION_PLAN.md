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

**Goal:** Action-based input with keyboard and mouse.

### 5.1 Input State Tracking
- [ ] Keyboard state (down, pressed this frame, released this frame)
- [ ] Mouse state (position, buttons, wheel)
- [ ] Per-frame state transitions

### 5.2 Binding System
- [ ] Action → input mapping
- [ ] Multiple inputs per action
- [ ] Input string parsing: `key:a`, `key:space`, `mouse:1`, `mouse:wheel_up`
- [ ] Runtime rebindable (for options menus)
- [ ] Key capture support for rebinding UI

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
| 5 | Input | Keyboard/mouse with action bindings (runtime rebindable) |
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
