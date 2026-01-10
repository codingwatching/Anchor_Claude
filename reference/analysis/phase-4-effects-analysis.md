# Phase 4: Effects System Analysis

A comprehensive analysis of shader/effect requirements for Anchor, based on examination of Super Emoji Box, BYTEPATH, and general 2D game effect patterns.

---

## Table of Contents

1. [Usage Analysis: Super Emoji Box](#usage-analysis-super-emoji-box)
2. [Usage Analysis: BYTEPATH](#usage-analysis-bytepath)
3. [Effect Categories](#effect-categories)
4. [Per-Layer vs Per-Object Effects](#per-layer-vs-per-object-effects)
5. [Recommended Architecture](#recommended-architecture)
6. [Ping-Pong Buffer System](#ping-pong-buffer-system)
7. [Per-Object Flash via Vertex Attributes](#per-object-flash-via-vertex-attributes)
8. [Lua API](#lua-api)
9. [C-Side Requirements](#c-side-requirements)
10. [Phase 4 Scope](#phase-4-scope)

---

## Usage Analysis: Super Emoji Box

Super Emoji Box uses a **simple, layer-based effect system** with three shaders:

### Shaders Used

| Shader | Purpose | Application |
|--------|---------|-------------|
| `outline.frag` | Black silhouette behind objects | Per-layer post-process |
| `shadow.frag` | Dark drop shadow (offset) | Per-layer post-process |
| `combine.frag` | Additive flash when hit/attacking | Per-object |

### Outline Shader
```glsl
// Samples 24 neighbors in 5x5 grid to detect alpha edges
vec4 effect(vec4 vcolor, Image texture, vec2 tc, vec2 pc) {
    vec4 t = Texel(texture, tc);
    float x = 1.0/love_ScreenSize.x;
    float y = 1.0/love_ScreenSize.y;

    float a = 0.0;
    a += Texel(texture, vec2(tc.x - 2.0*x, tc.y - 2.0*y)).a;
    // ... 23 more samples
    a = min(a, 1.0);

    return vec4(0.0, 0.0, 0.0, a);  // Black outline
}
```

### Shadow Shader
```glsl
vec4 effect(vec4 vcolor, Image texture, vec2 tc, vec2 pc) {
    return vec4(0.1, 0.1, 0.1, Texel(texture, tc).a * 0.2);
}
```

### Combine Shader (Additive Flash)
```glsl
vec4 effect(vec4 vcolor, Image texture, vec2 tc, vec2 pc) {
    vec4 t = Texel(texture, tc);
    return vec4(vcolor.rgb + t.rgb, t.a);  // Add vertex color to texture
}
```

### Rendering Pipeline

```
1. Draw objects to each layer's 'main' canvas
2. Apply outline shader: main → outline canvas
3. Apply shadow shader: game+effects → shadow layer
4. Composite in order:
   - background layers
   - outline canvases (behind)
   - shadow layer (offset by 2px)
   - main canvases (on top)
   - UI layers
5. Scale to screen
```

**Key insight:** No true effect chaining — effects are applied once per layer, then composited via draw order. The "chaining" is just layering multiple canvases.

---

## Usage Analysis: BYTEPATH

BYTEPATH uses a **complex multi-pass post-processing pipeline** with true effect chaining:

### Shaders Used

| Shader | Purpose | Parameters |
|--------|---------|------------|
| `displacement.frag` | Warp image via displacement map | `displacement_map` texture |
| `distort.frag` | CRT monitor effect | `scanlines`, `rgb_offset`, `horizontal_fuzz`, `time` |
| `glitch.frag` | Horizontal glitch displacement | `glitch_map` texture |
| `rgb_shift.frag` | Chromatic aberration | `amount` (vec2) |
| `rgb.frag` | RGB color mapping | — |
| `grayscale.frag` | Desaturation | — |
| `combine.frag` | Additive blending | — |
| `flat_color.frag` | Solid color fill | — |

### Rendering Pipeline (True Effect Chain)

```
Canvas Architecture:
├── main_canvas          (gameplay rendering)
├── shockwave_canvas     (displacement source)
├── glitch_canvas        (glitch map source)
├── rgb_shift_canvas     (RGB shift source)
├── rgb_canvas           (RGB effect source)
├── temp_canvas          (processing step 1)
├── temp_canvas_2        (processing step 2)
├── temp_canvas_3        (processing step 3)
└── final_canvas         (composite output)

Effect Chain:
1. Render gameplay → main_canvas
2. Render effect sources → shockwave/glitch/rgb_shift/rgb canvases
3. main_canvas + displacement shader (using shockwave_canvas) → temp_canvas
4. temp_canvas + glitch shader (using glitch_canvas) → temp_canvas_2
5. temp_canvas_2 + rgb_shift shader → temp_canvas_3
6. temp_canvas_3 + rgb shader (conditional) → final_canvas
7. final_canvas + distort shader (conditional) → screen
```

**Key insight:** This is true ping-pong effect chaining with multiple passes and shader parameters fed from other canvases.

---

## Effect Categories

Based on analysis, 2D game effects fall into these categories:

### 1. Simple Color Manipulation
- **Tint:** Multiply or blend with solid color
- **Brightness/Contrast:** Scale RGB values
- **Grayscale/Desaturation:** Convert to luminance

*Implementation: Single-pass, no extra textures needed*

### 2. Outline/Edge Effects
- **Outline:** Sample neighbors, detect alpha edges
- **Glow:** Blur + additive blend
- **Drop Shadow:** Offset + darken

*Implementation: Single-pass post-process, samples neighbors, needs pixel size uniform*

### 3. Distortion Effects
- **Displacement Map:** Warp based on texture R/G channels
- **Shockwave:** Radial displacement from point
- **Ripple/Wave:** Sinusoidal UV distortion
- **Glitch:** Horizontal displacement bands

*Implementation: May need additional texture input (displacement map)*

### 4. Screen-Space Effects
- **Chromatic Aberration (RGB Shift):** Sample R/G/B at offset positions
- **Scanlines:** Darken alternating rows
- **CRT/VHS:** Combine scanlines + noise + aberration
- **Vignette:** Darken edges

*Implementation: Single-pass, may need time uniform for animation*

### 5. Composite/Blend Effects
- **Additive Flash:** Add color to existing
- **Multiply:** Darken via multiplication
- **Screen:** Lighten via inverse multiply

*Implementation: Single-pass, uses vertex color or uniform*

---

## Per-Layer vs Per-Object Effects

### Per-Layer Effects (Post-Processing)

Applied after all objects are drawn to the layer:

```
Objects → Layer FBO → Effect Shader → Output
```

**Use cases:**
- Outline around all game objects
- Screen-wide color grading
- CRT/retro effects
- Bloom/glow for entire layer

**Key property:** Can sample neighboring pixels (required for outline, blur).

### Per-Object Effects

Applied when drawing individual objects:

```
Object → Shader → Layer FBO
```

**Use cases:**
- Flash white when hit (specific object)
- Color shift for specific enemy type

**Key property:** Cannot sample neighbors (only has access to sprite's own texture).

### Layer-Level Outline vs Per-Object Outline

These are different effects:

- **Layer outline:** Outlines the *combined silhouette* of all objects on the layer. Overlapping objects share one outline. Requires post-processing pass.

- **Per-object outline:** Each sprite has its own outline. Could be done in uber-shader if sprites have transparent padding, but overlapping objects get separate outlines.

For emoji-style games, **layer-level outline** is what's used (Super Emoji Box style).

---

## Recommended Architecture

**Lua-controlled pipeline** — C provides primitives, Lua orchestrates.

### What C Provides

```c
// Shader loading (fragment only, uses standard screen vertex shader)
GLuint shader_load_file(const char* path);
GLuint shader_load_string(const char* frag_source);
void shader_destroy(GLuint shader);

// Uniform setting (call before layer_apply_shader)
void shader_set_float(GLuint shader, const char* name, float v);
void shader_set_vec2(GLuint shader, const char* name, float x, float y);
void shader_set_vec4(GLuint shader, const char* name, float x, float y, float z, float w);
void shader_set_texture(GLuint shader, const char* name, GLuint texture, int unit);

// Layer post-processing
void layer_apply_shader(Layer* layer, GLuint shader);
GLuint layer_get_texture(Layer* layer);
```

### What Lua Provides

- Which shaders exist (loaded from files)
- How shaders are parameterized
- The rendering pipeline order
- Effect chaining logic

---

## Ping-Pong Buffer System

Each layer has two buffers for post-processing:

```c
typedef struct {
    GLuint fbo;              // Main framebuffer
    GLuint color_texture;    // Main render target

    GLuint effect_fbo;       // Effect framebuffer (created on first use)
    GLuint effect_texture;   // Effect render target

    bool textures_swapped;   // Tracks which buffer is "current"
} Layer;
```

### How It Works

**Initial state:**
- Draw commands render to `color_texture`
- `textures_swapped = false`

**After `layer_apply_shader(layer, shader1)`:**
- Renders `color_texture` → `effect_texture` using shader1
- Sets `textures_swapped = true`

**After `layer_apply_shader(layer, shader2)`:**
- Renders `effect_texture` → `color_texture` using shader2
- Sets `textures_swapped = false`

**Pattern continues:** Each apply swaps source and destination.

**At draw-to-screen time:**
- `layer_get_texture(layer)` returns whichever buffer is current
- If `textures_swapped = false`: returns `color_texture`
- If `textures_swapped = true`: returns `effect_texture`

### Example Trace

```
1. layer_draw_commands(game)        → game.color_texture has content
                                      game.textures_swapped = false

2. layer_apply_shader(game, outline_shader)
   → game.color_texture → game.effect_texture (with outline)
   → game.textures_swapped = true

3. layer_apply_shader(game, rgb_shift_shader)
   → game.effect_texture → game.color_texture (with rgb shift)
   → game.textures_swapped = false

4. layer_draw_to_screen(game)
   → checks game.textures_swapped (false)
   → draws game.color_texture
```

---

## Per-Object Flash via Vertex Attributes

**Problem:** Shader swapping for per-object effects breaks batching. If 50 enemies flash, that's 50+ draw calls instead of 1.

**Solution:** Bake flash into the uber-shader via per-vertex attributes.

### Current Vertex Format

```
x, y, u, v, r, g, b, a, type, shape[4]  (13 floats)
```

### Extended Vertex Format

```
x, y, u, v, r, g, b, a, type, shape[4], addR, addG, addB  (16 floats)
```

Add 3 floats for additive color (flash). The `vColor` RGBA is multiply (tint), the new `vAddColor` RGB is additive (flash).

### Uber-Shader Change

```glsl
in vec4 vColor;      // Multiply color (tint)
in vec3 vAddColor;   // Additive color (flash)

// For sprites:
vec4 texColor = texture(u_texture, snappedUV);
FragColor = vec4(texColor.rgb * vColor.rgb + vAddColor, texColor.a * vColor.a);
```

### API

```lua
-- No shader swap needed, stays in same batch
layer_draw_image(game, sprite, x, y, color, flash_color)

-- Example
local white = 0xFFFFFFFF
local flash = enemy.flashing and 0xFFFFFF or 0x000000
layer_draw_image(game, enemy.sprite, enemy.x, enemy.y, white, flash)
```

50 flashing enemies = still 1 batched draw call.

---

## Lua API

### Shader Loading

```lua
local outline_shader = shader_load_file('shaders/outline.frag')
local shadow_shader = shader_load_file('shaders/shadow.frag')
```

### Uniform Setting

```lua
shader_set_vec2(outline_shader, 'pixel_size', 1/480, 1/270)
shader_set_float(crt_shader, 'time', game_time)
shader_set_texture(displacement_shader, 'displacement_map', layer_get_texture(shockwave))
```

### Layer Post-Processing

```lua
-- Apply shader to layer (ping-pong)
layer_apply_shader(game, outline_shader)

-- Chain multiple effects
layer_apply_shader(game, effect1)
layer_apply_shader(game, effect2)
layer_apply_shader(game, effect3)

-- Get layer texture (for use as input to another shader)
local tex = layer_get_texture(shockwave)
```

### Per-Object Flash

```lua
-- Flash color as 6th parameter (0 = no flash)
layer_draw_image(game, sprite, x, y, 0xFFFFFFFF, enemy.flashing and 0xFFFFFF or 0)
```

---

## C-Side Requirements

### Layer Struct Additions

```c
typedef struct {
    // Existing...
    GLuint fbo;
    GLuint color_texture;

    // New: for ping-pong effect application
    GLuint effect_fbo;
    GLuint effect_texture;
    bool textures_swapped;
} Layer;
```

### New Functions

```c
// Shader loading
GLuint shader_load_file(const char* path);
GLuint shader_load_string(const char* frag_source);
void shader_destroy(GLuint shader);

// Uniforms
void shader_set_float(GLuint shader, const char* name, float v);
void shader_set_vec2(GLuint shader, const char* name, float x, float y);
void shader_set_vec4(GLuint shader, const char* name, float x, float y, float z, float w);
void shader_set_texture(GLuint shader, const char* name, GLuint texture, int unit);

// Layer effects
void layer_apply_shader(Layer* layer, GLuint shader);
GLuint layer_get_texture(Layer* layer);
```

### Vertex Format Extension

Add 3 floats for additive color:

```c
#define VERTEX_FLOATS 16  // was 13

// In batch_add_vertex, add: addR, addG, addB
// In VAO setup, add attribute for vAddColor (location 5)
```

### Shader Compilation

- Fragment source only (pair with standard screen vertex shader)
- Auto-prepend platform header (`#version 330 core` / `#version 300 es`)
- Standard uniform `u_texture` bound to texture unit 0

---

## Phase 4 Scope

For emoji-style games, we need:

### Must Have
1. **Outline shader** — layer-level, samples neighbors for edge detection
2. **Shadow shader** — layer-level, darkens and reduces alpha
3. **Flash** — per-object via vertex attribute (additive color)

### Infrastructure Required
- Shader loading from file
- Uniform setting (float, vec2, vec4, texture)
- `layer_apply_shader()` with ping-pong buffers
- `layer_get_texture()` for accessing layer as texture input
- Extended vertex format with additive color

### Deferred
- Per-object outline (different from layer outline)
- Grayscale/silhouette effects
- Complex effect chaining (BYTEPATH-style)
- CRT/distortion effects

These can be added later using the same infrastructure.
