# Anchor Project Review

## What Has Been Implemented

**Phase 1-2: Foundation (Complete)**
- Window/OpenGL initialization with SDL2
- Lua integration with protected calls and stack traces
- Error state display (red screen, stores error for later text rendering)
- Fullscreen toggle (F11, Alt+Enter)
- Aspect-ratio scaling with letterboxing
- Decoupled timestep: 120Hz physics, 60Hz render
- Full web build parity via Emscripten/WebGL 2.0

**Phase 3: Rendering (Complete)**
- Deferred command queue architecture
- SDF-based uber-shader (RECT, CIRCLE, SPRITE types)
- Layer system with FBOs (registry of up to 16 named layers)
- Transform stack (push/pop with TRS matrix composition)
- Sprite rendering with texel-center snapping
- Blend modes (alpha, additive)
- Multi-layer composition
- Shape filter modes (smooth/rough for anti-aliased vs pixel-perfect)
- VSync snapping, delta clamping, render_lag cap

---

## Opinions on Implementation Quality

### Strengths

1. **The UV-space SDF approach is elegant.** Rather than passing world-space centers and dealing with rotation complexity, the code passes quad dimensions and lets UV interpolation handle rotation implicitly. This is genuinely clever and keeps vertex data simpler (`anchor.c:477-522`).

2. **The frame timing work is thorough.** The analysis in `frame-timing-analysis.md` is comprehensive, and the final implementation addresses the key issues: VSync snapping, delta clamping, render_lag cap, 120Hz physics (even multiple of 60). The monitor simulator tool demonstrates serious engineering discipline.

3. **The pixel-perfect research shows good methodology.** `pixel-perfect-research.md` documents what was tried, what failed (C-level position snapping), and why the decoupled rates solution works. This kind of documented experimentation is valuable.

4. **The documentation suite is excellent.** `ANCHOR.md` is comprehensive without being bloated. `ANCHOR_IMPLEMENTATION_PLAN.md` has clear checkboxes. `SHAPES_PLAN.md` is forward-looking with good references to Cute Framework.

5. **The C code is clean.** Single monolithic file is appropriate at this scale. Functions are well-separated, comments are minimal but present where needed. The batch rendering system is straightforward.

### Areas of Concern

1. **Layer system is registry-based, not tree-based.** The docs say "Layers are engine infrastructure, not gameplay objects" — but the current `layer_registry` approach is a flat array with string lookups. This works, but when Phase 10 introduces the object tree, there may be awkwardness. For example, `layer_get_or_create()` does string comparison in a loop. With 16 max layers this is fine, but it's worth noting.

2. **No texture destruction API.** `texture_load()` allocates and returns, but `texture_destroy()` exists but isn't exposed to Lua. The docs say "resources live forever" which is intentional, but if a game loads hundreds of textures dynamically, this becomes a problem. May need `an:texture_unload()` eventually.

3. **Command queue growth is unbounded.** `layer_add_command()` doubles capacity with `realloc()`. For a runaway loop that adds millions of commands, this could exhaust memory before the frame ends. A cap (like `MAX_UPDATES` for physics) might be prudent.

4. **The test file is commented-out blocks.** `test/main.lua` has multiple test cases in `--{{{ }}}` folds, with only one active at a time. This works for manual testing but means there's no automated verification that old functionality still works.

---

## Forward-Looking Analysis: Phases 4-10

### Phase 4 (Effects/Shaders)

The layer FBO infrastructure is in place. Adding post-process passes should be straightforward — render layer to FBO, apply effect shader, output. The concern is shader hot-reload: the current `compile_shader()` is called at init, not exposed to Lua. For development iteration, you'd want `an:shader_load(path)` that can be called anytime.

### Phase 5 (Input)

SDL2's event polling is already in place. The main work is:
- Key state tracking (pressed/down/released distinction)
- Mouse position in game coordinates (needs inverse of aspect-ratio scaling)
- Action binding table

This should be straightforward. One question: will web input behave identically? Keyboard capture is partially handled in `shell.html` but gamepad isn't mentioned.

### Phase 6 (Audio)

The plan notes "TBD (miniaudio or SoLoud)." This is a significant decision. miniaudio is header-only and simpler; SoLoud has more features. Both support Emscripten. **Key question:** Does pitch shifting need to be sample-accurate? If so, SoLoud's filtering might be better. If approximate is fine, miniaudio's resampling works.

### Phase 7 (Physics)

Box2D 3.1 integration. The plan mentions sensor vs contact events, which is Box2D 3.x's approach. The collision tag system will need C-side storage — a mapping from tag strings to collision masks. The `collider` engine object pattern (add as child, auto-cleanup) is well-designed for this.

### Phase 8 (Random)

Trivial. PCG or xorshift can be implemented in <50 lines. The only question is state management for replays — will the RNG state be exposed to Lua for save/restore?

### Phase 9 (Text)

stb_truetype is already vendored. The plan says glyph metrics returned to Lua for layout, which is the right approach (text effects in YueScript). **Potential issue:** stb_truetype generates grayscale bitmaps. For the SDF uber-shader, you'd either need a separate text shader or convert to SDF at bake time.

### Phase 10 (YueScript Object System)

This is the most complex phase. The reference examples in `reference/examples.yue` show the target API. Key implementation questions:

1. **Operator overloading:** YueScript inherits Lua's metamethods. The `__inherited` hook pattern from `ANCHOR.md` should work, but it needs actual testing.

2. **Update loop integration:** The docs say "C calls into Lua each frame." Currently the C code calls `update(dt)`. The object system needs to intercept this and run the tree traversal. Options:
   - Lua-side main loop entirely
   - C calls `an:update(dt)` which is YueScript

3. **Timer/Spring implementation:** Are these pure Lua objects, or do they wrap C functions? The plan says "engine objects" with `destroy()` hooks, suggesting C-side resources. But basic timers don't need C — only if you want sub-millisecond precision.

---

## Issues and Questions

### Technical Issues

1. **The error display just stores the message** (`error_message[4096]`). Text rendering isn't implemented yet (Phase 9), so the red screen is blank. This is noted in the plan but worth tracking.

2. **Web audio context unlock:** The plan mentions "handle audio context unlock (requires user interaction)" but there's no implementation yet. This is a Chrome/Safari requirement — audio won't play until the user clicks. This needs explicit handling in `shell.html`.

3. **Gamepad on web:** SDL2's gamepad support works differently on Emscripten. The standard gamepad API needs specific handling. This is deferred (Phase 5+) but worth noting.

4. **The shader has a magic number for rough-mode circles:** `n = 1.95` for superellipse. This was presumably tuned by eye. Is this consistent across resolutions? At 640x360 vs 480x270, pixel-perfect circles might look different.

5. **`layer_clear_commands()` is called at start of update, not end of render.** This is documented as a fix for flickering, but it means commands persist if no update runs in a frame (which can happen if physics_lag is below threshold). Is this actually correct? If the game is paused (physics updates stop), would stale commands keep rendering?
   - Looking at `anchor.c:1138-1141`: commands are cleared inside the `while (physics_lag >= PHYSICS_RATE)` loop. So if no physics update runs, commands aren't cleared, and the previous frame's commands persist. This is intentional (previous commands should keep rendering) but feels fragile.

### Design Questions

1. **How will camera/viewport work?** The current system has no concept of camera offset. `layer_push()` applies transforms but there's no "camera transform applied to all draws." This becomes important for scrolling games. The YueScript wrapper layer could handle this, or it could be C-side.

2. **Will there be a layer:clear() function?** Currently layers composite with transparency. What if a layer needs to be fully cleared between frames with a specific color? `glClear()` uses transparent black.

3. **How will the `an` table be structured?** The docs reference `an:layer()`, `an:physics_set_gravity()`, etc. This is the engine singleton. Is it a Lua table with functions, or a C userdata with methods? The current bindings use plain functions (`layer_create()`, `rgba()`), which would need wrapping.

4. **Will YueScript compilation be in the build script or at runtime?** The plan says "Build-time compilation" with `yue -r`, but there's no YueScript step in `build.bat` yet. The `-r` flag for line number preservation is critical for debugging.

---

## Things Previous Instances May Have Missed

1. **The `MAX_LAYERS = 16` limit is hardcoded with no warning.** If you try to create a 17th layer, `layer_get_or_create()` prints an error and returns NULL. The Lua binding then calls `luaL_error()`. This is fine, but the limit isn't documented anywhere visible to users.

2. **Transform stack overflow is silent-ish.** `layer_push()` prints a warning but doesn't error. A push beyond 32 depth silently does nothing. This could cause confusing bugs.

3. **The shapes plan mentions "segment" and "polyline" shapes** that aren't in the Phase 3 implementation. `SHAPES_PLAN.md` has detailed specs for lines, polylines, and polygons that will need implementing. The current circle/rectangle/sprite set is minimal.

4. **The coordinate system for rectangles isn't fully decided.** `SHAPES_PLAN.md`'s "Open Questions" section asks: center or top-left? The current `layer_rectangle()` uses top-left (`x, y` is corner, `w, h` extends right/down). Circles use center (`x, y` is center, `radius` extends outward). This inconsistency might cause confusion.

5. **The web shell's keyboard handling might miss some keys.** `shell.html` prevents default for arrow keys and space, but what about other keys that have browser meaning (F1-F12, Tab, Backspace)? These would need testing.

6. **Memory alignment for command queue.** `DrawCommand` is 96+ bytes with potential padding. On different platforms/compilers, the struct layout might differ. This probably doesn't matter (no serialization), but worth noting if commands ever get saved/replayed.

7. **The "resync mechanism" from frame timing analysis was never implemented.** The analysis recommends exposing `resync()` to Lua for scene transitions. This isn't in the current code or the Phase 5 plan.

8. **Distribution packaging isn't implemented.** The plan mentions "zip-append" for embedding assets in the executable, but there's no code for this yet. Currently, the engine loads `main.lua` from the filesystem.

9. **The render_lag accumulator uses `if` not `while`.** Looking at `anchor.c:1170`:
   ```c
   if (render_lag >= RENDER_RATE) {
       render_lag -= RENDER_RATE;
       // render...
   }
   ```
   The frame timing analysis mentioned this could grow unbounded, but it was supposedly fixed. Looking at `anchor.c:1111-1114`, there's a cap (`render_lag = RENDER_RATE * 2`), so it's handled. Good.

10. **No line shape in Phase 3.** The test code only uses circles, rectangles, and sprites. Lines are in `SHAPES_PLAN.md` but not implemented. This might be a deliberate deferral (lines need thickness, caps), but it means no line drawing until Phase 3B-D is done.

---

## Summary Assessment

The project is in good shape. Phases 1-3 are solidly complete with thoughtful technical decisions. The documentation is unusually thorough. The main risk areas are:

1. **Phase 10 complexity:** The YueScript object system with operators is the biggest unknown. The design is clear but untested in actual code.

2. **Audio decision:** miniaudio vs SoLoud should be decided soon as it affects Phase 6 scope.

3. **Missing primitives:** Lines, polylines, and polygons aren't implemented yet but are specified. This is explicitly deferred to "3B-D" in the shapes plan.

4. **Web edge cases:** Audio unlock, gamepad, and keyboard edge cases will need attention before web parity is truly complete.

The incremental approach is working well. Each phase builds cleanly on the previous, and the verification steps (test on Windows and Web before proceeding) ensure parity is maintained.
