# Anchor Phase 3 Part 3 - Continuation Guide

This document is for the next Claude instance continuing Anchor engine development. It provides context on what was accomplished, inconsistencies found, potential issues, and detailed next steps.

---

## Session Summary (Phase 3 Parts 1-2)

### What Was Accomplished

**Phase 3 Part 1:**
- Layer struct with FBO, color texture, transform stack (32 deep)
- DrawCommand struct with type, blend_mode, color, transform, params
- Command queue with dynamic growth
- Rectangle rendering with batch system
- Lua bindings: `layer_create()`, `layer_rectangle()`, `rgba()`

**Phase 3 Part 2 (This Session):**
- SDF uber-shader with type branching (RECT=0, CIRCLE=1, SPRITE=2)
- Vertex format expanded to 13 floats: x, y, u, v, r, g, b, a, type, shape[4]
- Circle SDF with pixel-art style superellipse (n=1.95) for "rough" mode
- Shape filter modes: "smooth" (anti-aliased) vs "rough" (pixel-perfect)
- Pixel snapping in rough mode (position, center, radius all snapped to grid)
- Integer-only screen scaling to prevent interpolation artifacts
- Main loop fix: events outside fixed loop, commands cleared at START of update
- Lua binding: `layer_circle()`, `set_shape_filter()`
- Verified on Windows and Web (Emscripten)

---

## Documentation Inconsistencies (Minor)

### ANCHOR.md: Future API Examples

**Location:** `docs/ANCHOR.md` lines 50-58

The doc shows examples like `game\circle @x, @y, 10, colors.white` which is the future YueScript API. The current C bindings use `layer_circle(game, x, y, r, color)`. This is expected (C first, YueScript later), but a future Claude might be confused. Consider adding a note that the examples show the final YueScript API, not the current C bindings.

---

## Potential Bugs and Issues

### 1. Rectangle SDF Assumes No Rotation (CRITICAL for Step 6)

**Location:** `engine/src/anchor.c` lines 356-361

```c
// Compute center and half-size in world space (for SDF)
// Note: This assumes no rotation in transform for now
float cx, cy;
transform_point(cmd->transform, x + w * 0.5f, y + h * 0.5f, &cx, &cy);
float half_w = w * 0.5f;
float half_h = h * 0.5f;
```

When implementing transform stack (Step 6), this will break. The rectangle SDF is computed in world space with untransformed half_w/half_h. With rotation, the SDF parameters need to be in local space and the shader needs the full transform to compute distances correctly.

**Solution options:**
1. Pass local-space params + transform matrix to shader (like Cute Framework)
2. Decompose transform to extract rotation and adjust SDF
3. Only support uniform scale + translation for rectangles (limiting)

### 2. No Batch Flushing on State Change

**Location:** `engine/src/anchor.c` lines 408-435

The `layer_render()` function iterates commands but doesn't flush the batch when blend mode changes. Currently all shapes use the same blend mode (BLEND_ALPHA), but when Step 8 implements blend mode switching, this will cause visual artifacts.

```c
// MISSING: Check for blend mode change and flush
switch (cmd->type) {
    case SHAPE_RECTANGLE:
        process_rectangle(cmd);
        break;
    // ...
}
```

### 3. Rough Mode: Rectangles vs Circles Mismatch

**Location:** `engine/src/anchor.c` fragment shader lines 576-592

In rough mode, circles use superellipse (n=1.95) for pixel-art cardinal bumps, but rectangles use standard SDF. This means circles look "pixely" while rectangles have perfect edges. This might be intentional but could look inconsistent.

### 4. Shape Params Limited to 4 Floats

**Location:** `engine/src/anchor.c` line 521

```c
"layout (location = 4) in vec4 aShape;\n"
```

The shape[] attribute is vec4 (4 floats). SHAPES_PLAN references Randy's approach with shape[8] (16 floats) needed for polygons with up to 8 vertices. When implementing polygons, the vertex format will need expansion.

### 5. AA Padding in Rough Mode

**Location:** `engine/src/anchor.c` lines 337, 379

```c
float pad = 2.0f;  // Added even in rough mode
```

The padding is always added for anti-aliasing, but in rough mode (step edges), this padding is unnecessary. Minor performance impact (slightly larger quads than needed).

### 6. Integer Scaling Edge Case

**Location:** `engine/src/anchor.c` lines 827-828

```c
int int_scale = (int)scale;
if (int_scale < 1) int_scale = 1;
```

When the window is smaller than game resolution (e.g., 400x200 for 480x270 game), scale < 1 clips to 1. This shows the game at native size with clipping. Might want to allow fractional scaling below 1x, or handle this case differently.

---

## Next Steps (Detailed)

### Step 6: Transform Stack (push/pop)

**Goal:** Allow hierarchical transforms for rotation and scaling.

**Implementation:**

1. **Add `layer_push()` function:**
```c
static void layer_push(Layer* layer, float x, float y, float r, float sx, float sy) {
    if (layer->transform_depth >= MAX_TRANSFORM_DEPTH - 1) return;

    // Build TRS matrix: translate -> rotate -> scale
    float c = cosf(r), s = sinf(r);
    float m[9] = {
        sx * c, -sy * s, x,
        sx * s,  sy * c, y,
        0,       0,      1
    };

    // Multiply with current transform
    layer->transform_depth++;
    float* current = layer_get_transform(layer);
    float* parent = current - 9;
    // Matrix multiply: current = parent * m
    mat3_multiply(parent, m, current);
}

static void layer_pop(Layer* layer) {
    if (layer->transform_depth > 0) {
        layer->transform_depth--;
    }
}
```

2. **Add Lua bindings:**
```c
static int l_layer_push(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0.0);
    float y = (float)luaL_optnumber(L, 3, 0.0);
    float r = (float)luaL_optnumber(L, 4, 0.0);
    float sx = (float)luaL_optnumber(L, 5, 1.0);
    float sy = (float)luaL_optnumber(L, 6, 1.0);
    layer_push(layer, x, y, r, sx, sy);
    return 0;
}

static int l_layer_pop(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    layer_pop(layer);
    return 0;
}
```

3. **Fix rectangle rotation issue** (see Bug #1 above):
   - Pass full transform to shader as uniform or per-vertex data
   - Or compute SDF in local space with UV remapping

**Verification test:**
```lua
function update(dt)
    layer_push(game, 240, 135, game_time, 2, 2)  -- center, rotate, scale 2x
        layer_rectangle(game, -25, -25, 50, 50, 0xFF0000FF)  -- red rect at origin

        layer_push(game, 50, 0, 0, 0.5, 0.5)  -- offset, half scale
            layer_circle(game, 0, 0, 20, 0x00FF00FF)  -- green circle
        layer_pop(game)
    layer_pop(game)
end
```

### Step 7: Sprites (Texture Loading)

**Goal:** Load images and draw textured quads.

**Implementation:**

1. **Texture loading via stb_image:**
```c
typedef struct {
    GLuint id;
    int width;
    int height;
} Texture;

static Texture* texture_load(const char* path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) return NULL;

    Texture* tex = (Texture*)malloc(sizeof(Texture));
    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    stbi_image_free(data);

    tex->width = w;
    tex->height = h;
    return tex;
}
```

2. **Add `layer_draw_image()` function:**
```c
static void layer_add_sprite(Layer* layer, Texture* tex,
                             float x, float y, float r,
                             float sx, float sy, float ox, float oy,
                             uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = SHAPE_SPRITE;
    cmd->color = color;
    cmd->texture_id = tex->id;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = tex->width * fabsf(sx);
    cmd->params[3] = tex->height * fabsf(sy);
    cmd->params[4] = ox;
    cmd->params[5] = oy;
    // Store scale signs for flip
    cmd->params[6] = sx < 0 ? -1.0f : 1.0f;
    cmd->params[7] = sy < 0 ? -1.0f : 1.0f;
}
```

3. **Update shader for SPRITE mode:**
```glsl
} else {
    // Sprite - sample texture
    FragColor = texture(u_texture, vUV) * vColor;
    return;
}
```

4. **Batch flush on texture change:**
```c
if (cmd->type == SHAPE_SPRITE && cmd->texture_id != current_texture) {
    batch_flush();
    glBindTexture(GL_TEXTURE_2D, cmd->texture_id);
    current_texture = cmd->texture_id;
}
```

### Step 8: Blend Modes

**Goal:** Support alpha blending and additive blending.

**Implementation:**

1. **Add `layer_set_blend_mode()` Lua binding:**
```c
static int l_layer_set_blend_mode(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    const char* mode = luaL_checkstring(L, 2);
    if (strcmp(mode, "alpha") == 0) {
        layer->current_blend = BLEND_ALPHA;
    } else if (strcmp(mode, "additive") == 0) {
        layer->current_blend = BLEND_ADDITIVE;
    }
    return 0;
}
```

2. **Track blend state in layer_render and flush on change:**
```c
static void layer_render(Layer* layer) {
    batch_vertex_count = 0;
    uint8_t current_blend = BLEND_ALPHA;
    apply_blend_mode(current_blend);

    for (int i = 0; i < layer->command_count; i++) {
        const DrawCommand* cmd = &layer->commands[i];

        // Flush on blend mode change
        if (cmd->blend_mode != current_blend) {
            batch_flush();
            current_blend = cmd->blend_mode;
            apply_blend_mode(current_blend);
        }

        // ... process commands
    }
}

static void apply_blend_mode(uint8_t mode) {
    switch (mode) {
        case BLEND_ALPHA:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BLEND_ADDITIVE:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
    }
}
```

### Step 9: Multiple Layers + Composition

**Goal:** Support multiple layers that composite in order.

**Implementation:**

1. **Layer registry:**
```c
#define MAX_LAYERS 16
static Layer* layers[MAX_LAYERS];
static int layer_count = 0;

static Layer* layer_get_or_create(const char* name) {
    // Find existing
    for (int i = 0; i < layer_count; i++) {
        if (strcmp(layers[i]->name, name) == 0) {
            return layers[i];
        }
    }
    // Create new
    if (layer_count >= MAX_LAYERS) return NULL;
    Layer* layer = layer_create(GAME_WIDTH, GAME_HEIGHT);
    strncpy(layer->name, name, sizeof(layer->name));
    layers[layer_count++] = layer;
    return layer;
}
```

2. **Render all layers at frame end:**
```c
// In main_loop_iteration:
for (int i = 0; i < layer_count; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, layers[i]->fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    layer_render(layers[i]);
}

// Composite to screen
glBindFramebuffer(GL_FRAMEBUFFER, 0);
for (int i = 0; i < layer_count; i++) {
    draw_layer_quad(layers[i]);
}
```

---

## Files to Reference

- **Main engine code:** `engine/src/anchor.c` (~1066 lines)
- **Test Lua script:** `engine/main.lua` (bouncing DVD circle test)
- **LÖVE comparison:** `engine/love-compare/main.lua` (same test for comparison)
- **Implementation plan:** `docs/ANCHOR_IMPLEMENTATION_PLAN.md`
- **Shapes system plan:** `docs/SHAPES_PLAN.md` (detailed SDF reference)
- **Engine spec:** `docs/ANCHOR.md`
- **Instructions:** `.claude/CLAUDE.md`

---

## Key Technical Decisions Made

1. **Superellipse (n=1.95) for pixel circles** - Provides characteristic "cardinal bumps" that look like traditional pixel-art circles

2. **Integer-only screen scaling** - Prevents interpolation artifacts, ensures each game pixel maps to NxN screen pixels

3. **Commands cleared at START of update** - Fixes flickering when no fixed update runs (commands persist to next render)

4. **Events outside fixed loop** - Input remains responsive regardless of update rate

5. **Shape filter uniform (u_aa_width)** - Controls smooth (1.0) vs rough (0.0) mode in shader

---

## Questions for User

Before implementing Step 6 (transform stack), clarify:

1. **Rectangle rotation approach:** Should rectangles support full rotation via transforms, or is translate+scale sufficient for now? Full rotation requires shader changes.

2. **Rough mode for rectangles:** Should rectangles also have a pixel-style mode like circles, or is perfect edge fine?

3. **Default blend mode:** Should each layer start with alpha blending, or should there be a global default?
