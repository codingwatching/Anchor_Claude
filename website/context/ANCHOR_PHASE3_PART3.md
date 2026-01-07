# Anchor Phase 3 Part 3 - Status Update

This document tracks progress for Phase 3 rendering implementation.

---

## Completed Steps

### Step 6: Transform Stack (push/pop) ✓

- `mat3_multiply()` — 3x3 matrix multiplication for composing transforms
- `layer_push(layer, x, y, r, sx, sy)` — build TRS matrix, multiply with current
- `layer_pop(layer)` — decrement depth with underflow warning
- UV-space SDF approach for rotation support (no extra vertex data needed)
- Lua bindings: `layer_push()`, `layer_pop()`
- Verified with comprehensive test (nested transforms, orbits, non-uniform scale, corner pivots)
- Matching LÖVE test created for visual comparison

### Step 7: Sprites (texture loading, draw_image) ✓

- Texture loading via stb_image: `texture_load(path)`
- `texture_get_width(tex)`, `texture_get_height(tex)` — query dimensions
- `layer_draw_texture(layer, tex, x, y)` — draws sprite centered at position
- SPRITE mode in shader (sample texture at texel centers)
- Texel center snapping for pixel-perfect sprite rendering
- Batch flush on texture change
- Verified with bouncing emoji + orbiting stars test

### Pixel-Perfect Rendering Solution ✓

Decoupled physics (144Hz) from rendering (60Hz) for pixel-perfect visuals with responsive input.

See `reference/pixel-perfect-research.md` for full details on what was tried and the final solution.

---

## Remaining Steps

### Step 8: Blend modes
- [ ] `layer_set_blend_mode(layer, mode)` — 'alpha' or 'additive'
- [ ] Blend mode stored per-command
- [ ] Batch flush on blend mode change
- [ ] Apply blend state before drawing batch

### Step 9: Multiple layers + composition
- [ ] Layer registry (max 16 layers)
- [ ] `layer_create(name)` creates/retrieves named layer
- [ ] Layer ordering for composition
- [ ] Composite all layers to screen at frame end

---

## Key Code Locations

- **Transform stack:** `anchor.c` lines 259-305 (mat3_multiply, layer_push, layer_pop)
- **Sprite rendering:** `anchor.c` lines 565-617 (process_sprite, texture handling)
- **Decoupled timestep:** `anchor.c` lines 42-43 (PHYSICS_RATE, RENDER_RATE)
- **Main loop:** `anchor.c` lines 982+ (physics_lag, render_lag accumulators)

---

## Files to Reference

- **Main engine code:** `engine/src/anchor.c`
- **Test Lua script:** `engine/main.lua` (combined circle + emoji test)
- **LÖVE comparison:** `engine/love-compare/main.lua`
- **Implementation plan:** `docs/ANCHOR_IMPLEMENTATION_PLAN.md`
- **Shapes system plan:** `docs/SHAPES_PLAN.md`
- **Pixel-perfect research:** `reference/pixel-perfect-research.md`
