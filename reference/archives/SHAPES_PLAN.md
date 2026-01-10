# Shapes Rendering System — Implementation Plan

A comprehensive 2D vector graphics system for Anchor, inspired by Freya Holmér's Shapes plugin for Unity. SDF-based rendering for clean edges at any scale, with full support for gradients, dashes, outlines, and texture fills.

---

## Goals

- **High-quality rendering**: Local anti-aliasing built into all shapes, smooth edges without relying on MSAA
- **Arbitrary thickness**: Lines and outlines at any pixel width, with proper thinness fading for sub-pixel sizes
- **Rich styling**: Gradients (linear, radial, angular, bilinear), dashes, outlines, texture fills
- **Efficient batching**: Minimize draw calls via uber-shader approach
- **Simple API**: Transforms via push/pop stack, shapes are just position + size + style

---

## Reference Implementation: Cute Framework

Randy Gaul's [Cute Framework](https://github.com/RandyGaul/cute_framework) uses a similar SDF approach and serves as a practical reference for our implementation. Key insights:

### Vertex Format

Cute Framework's `CF_Vertex` stores shape data directly in vertices:

```c
struct CF_Vertex {
    CF_V2 p;           // World space position
    CF_V2 posH;        // Camera-transformed position
    int n;             // Vertex count (for polygons)
    CF_V2 shape[8];    // 8 control points (16 floats) for SDF shapes
    CF_V2 uv;          // Texture coordinates
    CF_Pixel color;    // Color
    float radius;      // Circle radius, corner rounding
    float stroke;      // Stroke/outline width
    float aa;          // Anti-aliasing factor
    uint8_t type;      // Shape type
    uint8_t alpha;     // Alpha flags
    uint8_t fill;      // Fill vs wireframe
    CF_Color attributes; // User shader params (4 floats)
};
```

**Key insight:** Rather than encoding shape params differently per type, Randy uses a large `shape[8]` array (8 vec2s = 16 floats) that can hold any shape's control points. The shader interprets these based on `type`.

### Uber Shader Approach

The fragment shader branches on `type` (encoded as normalized byte 0-255):

```glsl
bool is_sprite = v_type < (0.5/255.0);
bool is_text   = v_type > (0.5/255.0) && v_type < (1.5/255.0);
bool is_box    = v_type > (1.5/255.0) && v_type < (2.5/255.0);
bool is_seg    = v_type > (2.5/255.0) && v_type < (3.5/255.0);
bool is_tri    = v_type > (3.5/255.0) && v_type < (4.5/255.0);
// else: polygon

if (is_sprite || is_text) {
    // Texture sampling
} else if (is_box) {
    d = distance_box(v_pos, v_ab, v_cd);
} else if (is_seg) {
    // Polyline corner: union of two segments
    d = min(distance_segment(v_pos, v_ab.xy, v_ab.zw),
            distance_segment(v_pos, v_ab.zw, v_cd.xy));
} else if (is_tri) {
    d = distance_triangle(v_pos, v_ab.xy, v_ab.zw, v_cd.xy);
} else {
    // Polygon with up to 8 vertices
    d = distance_polygon(v_pos, shape_points, n);
}
```

### SDF Application

Final color computed via smoothstep AA:

```glsl
vec4 sdf(vec4 fill_color, vec4 stroke_color, float d) {
    float wire_d = abs(d) - v_stroke;  // Stroke distance
    vec4 stroke_aa = mix(stroke_color, vec4(0), smoothstep(0.0, v_aa, wire_d));
    vec4 fill_aa = mix(fill_color, vec4(0), smoothstep(0.0, v_aa, d));
    return mix(stroke_aa, fill_aa, v_fill);
}
```

### Polyline Corners

Each polyline corner is rendered as **3 control points** forming the union of two capsule/segment SDFs. The geometry is built on CPU with careful shared-edge computation to avoid pixel flickering at triangle borders.

```
Point A -----> Point B -----> Point C
         seg1          seg2

Corner at B = min(distance_segment(p, A, B), distance_segment(p, B, C))
```

### Polygon Fallback

For polygons with **more than 8 vertices**, Cute Framework falls back to CPU ear-clipping triangulation without SDF. This means no per-pixel AA or corner rounding, but works for any vertex count.

### Implications for Anchor

1. **Simplify vertex format:** Use a large params array rather than complex per-type encoding
2. **Single uber-shader:** All shapes in one shader with type branching
3. **Polyline via segment unions:** Not geometry-based joins, but SDF corner unions
4. **Practical polygon limit:** SDF polygon up to 8 vertices, ear-clipping beyond that

---

## Architecture Overview

### Unified Uber-Shader System

Following Cute Framework's approach, Anchor uses a **single uber-shader** for all shape types. The fragment shader branches on a `type` attribute to compute the appropriate SDF or sample textures.

**SDF Shapes (rendered as quads):**
- Circle (filled disc)
- Box (axis-aligned or rotated rectangle)
- Rounded Rectangle (box with corner radius)
- Capsule (line with round caps)
- Triangle
- Segment (polyline corner — union of two line SDFs)
- Polygon (up to 8 vertices)

**Texture Shapes (rendered as quads):**
- Sprite
- Text (future)

**Mesh Fallback (no SDF, CPU triangulation):**
- Polygon with >8 vertices
- Complex polylines with many segments

The uber-shader approach maximizes batching — all shapes can be submitted in one draw call as long as texture and blend mode don't change.

### Rendering Pipeline (Deferred)

Anchor uses **deferred rendering** — draw calls during update store commands, actual GPU work happens at frame end.

**During update (Lua draw calls):**
```
layer_circle(game, 100, 100, 10, red)   → stores command in game.commands[]
layer_polygon(game, points, blue)       → stores command in game.commands[]
layer_circle(game, 200, 100, 10, green) → stores command in game.commands[]
```

Each layer maintains a command queue. Commands are small structs (~64-128 bytes) containing shape type and parameters. No vertex building, no GPU work during update.

**At frame end:**
```
For each layer:
    1. Process commands in order:
       - For each command:
         - If state change needed (shader/texture/blend): flush current batch
         - Build vertices for this command
         - Add vertices to current batch
       - Flush final batch to layer's framebuffer
    2. Apply post-process effects (if any)

Composite all layers to screen in order
Present (swap buffers)
```

**Why deferred:**
- All GPU work at one predictable time (frame end)
- Commands are tiny, vertices built once at the end
- Clean separation: update = game logic + record draws, frame end = render
- No mid-frame flushes visible to Lua code

### Vertex Format

Following Cute Framework's approach, we use a unified vertex format with shape data passed per-vertex. The shader interprets the data based on `type`.

**Current Implementation (Phase 3) — 52 bytes per vertex (13 floats):**

```c
// Vertex layout: x, y, u, v, r, g, b, a, type, shape[4]
#define VERTEX_FLOATS 13

// VAO attributes:
// location 0: vec2 aPos      (x, y)
// location 1: vec2 aUV       (u, v)
// location 2: vec4 aColor    (r, g, b, a) - unpacked floats 0-1
// location 3: float aType    (0=RECT, 1=CIRCLE, 2=SPRITE)
// location 4: vec4 aShape    (shape params, meaning depends on type)
```

**Shape data encoding (UV-space approach):**

| Type | shape.x | shape.y | shape.z | shape.w |
|------|---------|---------|---------|---------|
| RECT (0) | quad_width | quad_height | half_width | half_height |
| CIRCLE (1) | quad_size | quad_size | radius | (unused) |
| SPRITE (2) | (unused) | (unused) | (unused) | (unused) |

**Note:** We use a UV-space SDF approach for rotation support. Instead of passing world-space center, we pass the quad size to the shader. The shader computes local position from UV: `local_p = vUV * quad_size`, and the center is always at `quad_size * 0.5`. This handles rotation implicitly through UV interpolation without needing extra vertex data.

**Note:** Current shape[4] (vec4) limits us to simple shapes. For polygons (8 vertices = 16 floats), the vertex format will need expansion to shape[8] like Cute Framework.

**Future Phases (Full Features) — expanded format:**

When implementing advanced shapes (polygons, gradients, strokes), expand to:

```c
// Expanded vertex layout for full shapes system
// location 4: vec4 aShape0   (shape params 0-3)
// location 5: vec4 aShape1   (shape params 4-7)
// + additional attributes for: color2, stroke, gradient params, dash params
```

The base format supports Phase 3 shapes (circle, rectangle, sprite). Advanced features (gradients, dashes, polygons) are added incrementally.

---

## Shape Primitives

### Disc (Filled Circle)

**SDF:**
```glsl
float sdf_disc(vec2 uv, float radius) {
    return length(uv - 0.5) - radius;
}
```

**Parameters:**
- `x, y` — Center position
- `radius` — Circle radius in pixels
- `color` — Fill color
- `outline_color` — Optional outline color
- `outline_thickness` — Outline width in pixels (0 = no outline)

**Quad sizing:** Bounding box is `(x - radius - outline, y - radius - outline)` to `(x + radius + outline, y + radius + outline)`, plus padding for anti-aliasing.

---

### Ring (Circle Outline / Donut)

**SDF:**
```glsl
float sdf_ring(vec2 uv, float inner, float outer) {
    float d = length(uv - 0.5);
    return max(inner - d, d - outer);
}
```

**Parameters:**
- `x, y` — Center position
- `inner_radius` — Inner edge radius
- `outer_radius` — Outer edge radius
- `color` — Fill color
- `outline_color`, `outline_thickness` — Optional outline on both edges

**Notes:** A ring with `inner_radius = 0` is equivalent to a disc. A ring where `outer_radius - inner_radius` is small creates a thin circle outline.

---

### Arc (Ring Segment)

**SDF:**
```glsl
float sdf_arc(vec2 uv, float inner, float outer, float start, float end) {
    vec2 p = uv - 0.5;
    float d = length(p);
    float angle = atan(p.y, p.x);

    // Radial bounds
    float radial = max(inner - d, d - outer);

    // Angular bounds (handle wrapping)
    float half_span = (end - start) * 0.5;
    float mid = start + half_span;
    vec2 mid_dir = vec2(cos(mid), sin(mid));

    // Rotate point to align with arc center
    float local_angle = atan(
        p.x * mid_dir.y - p.y * mid_dir.x,
        p.x * mid_dir.x + p.y * mid_dir.y
    );

    // Angular distance from arc
    float angular = abs(local_angle) - half_span;
    if (angular > 0.0) {
        // Outside angular range - distance to nearest endpoint
        float end_angle = local_angle > 0.0 ? end : start;
        vec2 end_pos = vec2(cos(end_angle), sin(end_angle));
        // Compute distance to arc endpoint...
    }

    return max(radial, angular_distance);
}
```

**Parameters:**
- `x, y` — Center position
- `inner_radius`, `outer_radius` — Radial bounds
- `start_angle`, `end_angle` — Angular bounds in radians
- `cap_type` — End cap style: `NONE`, `SQUARE`, `ROUND`
- `color`, `outline_color`, `outline_thickness`

**Notes:** Arc SDF is the most complex due to angular bounds and end caps. The end caps require special handling — round caps add semicircles at endpoints, square caps extend the arc.

---

### Pie (Filled Wedge)

**SDF:**
```glsl
float sdf_pie(vec2 uv, float radius, float start, float end) {
    vec2 p = uv - 0.5;
    float d = length(p);
    float angle = atan(p.y, p.x);

    // Radial bound
    float radial = d - radius;

    // Angular bounds (similar to arc)
    // ... angle handling ...

    return max(radial, angular_distance);
}
```

**Parameters:**
- `x, y` — Center position
- `radius` — Outer radius (inner is always 0 — it's filled to center)
- `start_angle`, `end_angle` — Wedge bounds in radians
- `color`, `outline_color`, `outline_thickness`

**Notes:** A pie is essentially an arc with `inner_radius = 0` but the SDF is simpler since we don't need the inner radial bound. The edges from center to arc need outline handling.

---

### Rectangle

**SDF:**
```glsl
float sdf_rect(vec2 uv, vec2 half_size) {
    vec2 d = abs(uv - 0.5) - half_size;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
```

**Parameters:**
- `x, y` — Center position (or top-left, depending on API convention)
- `width`, `height` — Rectangle dimensions
- `color`, `outline_color`, `outline_thickness`

**Notes:** The SDF gives exact distance to rectangle boundary, including correct behavior at corners.

---

### Rounded Rectangle

**SDF:**
```glsl
float sdf_rounded_rect(vec2 uv, vec2 half_size, float radius) {
    vec2 d = abs(uv - 0.5) - half_size + radius;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;
}
```

**Parameters:**
- `x, y` — Center position
- `width`, `height` — Rectangle dimensions
- `corner_radius` — Radius of rounded corners (can be single value or 4 values for different corners)
- `color`, `outline_color`, `outline_thickness`

**Notes:** When `corner_radius >= min(width, height) / 2`, the shape becomes a stadium (pill shape). Four different corner radii requires a more complex SDF.

**Four-corner variant:**
```glsl
float sdf_rounded_rect_4(vec2 uv, vec2 half_size, vec4 radii) {
    // Select radius based on quadrant
    vec2 q = step(vec2(0.5), uv);
    float r = mix(
        mix(radii.x, radii.y, q.x),  // bottom-left, bottom-right
        mix(radii.w, radii.z, q.x),  // top-left, top-right
        q.y
    );
    vec2 d = abs(uv - 0.5) - half_size + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}
```

---

### Line

**SDF:**
```glsl
float sdf_line(vec2 p, vec2 a, vec2 b, float thickness) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - thickness * 0.5;
}
```

**Parameters:**
- `x1, y1` — Start point
- `x2, y2` — End point
- `thickness` — Line width in pixels
- `cap_type` — End cap style: `NONE`, `SQUARE`, `ROUND`
- `color`, `outline_color`, `outline_thickness`

**End caps:**
- `NONE`: Line ends exactly at endpoints (flat cut)
- `SQUARE`: Line extends by `thickness/2` past endpoints
- `ROUND`: Semicircular caps at endpoints

**SDF with caps:**
```glsl
float sdf_line_round_caps(vec2 p, vec2 a, vec2 b, float thickness) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - thickness * 0.5;
    // Round caps are implicit — the SDF naturally creates them
}

float sdf_line_square_caps(vec2 p, vec2 a, vec2 b, float thickness) {
    // Extend a and b by thickness/2 in the line direction
    vec2 dir = normalize(b - a);
    a -= dir * thickness * 0.5;
    b += dir * thickness * 0.5;
    // Then compute rectangle SDF oriented along line...
}

float sdf_line_no_caps(vec2 p, vec2 a, vec2 b, float thickness) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = dot(pa, ba) / dot(ba, ba);
    if (h < 0.0 || h > 1.0) return 1e10; // Outside line segment
    return length(pa - ba * h) - thickness * 0.5;
}
```

---

### Arbitrary Polygon (Mesh-Based)

**Not SDF** — Generated via triangulation.

**Algorithm:** Ear clipping triangulation
1. Input: List of points defining outer boundary, optional lists for holes
2. For each hole, find optimal bridge point to merge with outer boundary
3. Apply ear clipping to the merged polygon
4. Output: Triangle list

**Parameters:**
- `points` — Array of `{x, y}` vertices defining outer boundary (clockwise or counter-clockwise)
- `holes` — Optional array of point arrays defining holes
- `color` — Fill color (or gradient)
- `outline_color`, `outline_thickness` — If outline desired, generate outline geometry separately

**Implementation notes:**
- Ear clipping is O(n²) but simple and handles all cases
- For performance-critical cases with many polygons, could use libtess2 or similar
- Holes require merging into outer boundary via bridge edges before triangulation

---

### Polyline (Mesh-Based)

**Not SDF** — Generated via geometry.

**Algorithm:**
1. For each line segment, generate a quad (4 vertices, 2 triangles)
2. At each join point, generate join geometry based on type
3. At endpoints, generate cap geometry based on type

**Join types:**

**Simple:** No extra geometry. Works for shallow angles, creates gaps at sharp angles.

**Miter:** Extend the edges of adjacent segments until they meet. Can create very long spikes at sharp angles — typically clamped with a miter limit.
```
    \      /
     \    /
      \  /
       \/  ← miter point
```

**Bevel:** Cut off the miter point with a flat edge. Add one triangle to fill the gap.
```
    \    /
     \  /
      \/
      /\   ← bevel edge
```

**Round:** Fill the gap with a circular arc. Add multiple triangles to approximate the curve.
```
    \    /
     \  /
      ()   ← circular arc
```

**Parameters:**
- `points` — Array of `{x, y}` vertices defining the path
- `thickness` — Line width in pixels
- `join_type` — `SIMPLE`, `MITER`, `BEVEL`, `ROUND`
- `cap_type` — `NONE`, `SQUARE`, `ROUND` (for start and end)
- `miter_limit` — Maximum miter extension before falling back to bevel (default: 4.0)
- `color` — Line color (or gradient along path)
- `closed` — If true, connect last point to first

---

## Features

### Local Anti-Aliasing

All SDF shapes have built-in anti-aliasing via `smoothstep`:

```glsl
float aa_width = fwidth(sdf) * 1.5; // or fixed pixel width
float alpha = smoothstep(aa_width, -aa_width, sdf);
```

`fwidth()` computes the rate of change of the SDF across the pixel, giving us the right smoothing width regardless of shape size or screen position.

For mesh shapes (polygon, polyline), we can:
1. Rely on MSAA if available
2. Generate slightly expanded geometry with alpha fade at edges
3. Accept aliased edges (often acceptable for filled polygons)

---

### Thinness Fading

When shapes are thinner than ~1 pixel, naive rendering either:
- Snaps to 1 pixel (inconsistent sizing)
- Disappears (popping artifacts)

Thinness fading renders at minimum 1 pixel width but fades opacity proportionally:

```glsl
float actual_thickness = /* user-specified thickness */;
float render_thickness = max(actual_thickness, 1.0);
float alpha_multiplier = actual_thickness / render_thickness;

// Apply to final alpha
color.a *= alpha_multiplier;
```

A 0.5px line renders as a 1px line at 50% opacity. Smooth fadeout instead of popping.

---

### Gradients

**Linear Gradient:**
```glsl
// Gradient from point A to point B
vec2 grad_dir = normalize(grad_end - grad_start);
float t = dot(uv - grad_start, grad_dir) / length(grad_end - grad_start);
t = clamp(t, 0.0, 1.0);
vec4 color = mix(color1, color2, t);
```

**Radial Gradient:**
```glsl
float t = length(uv - grad_center) / grad_radius;
t = clamp(t, 0.0, 1.0);
vec4 color = mix(color1, color2, t);
```

**Angular (Sweep) Gradient:**
```glsl
vec2 d = uv - grad_center;
float angle = atan(d.y, d.x);
float t = (angle + PI) / (2.0 * PI); // 0 to 1 around the circle
// Optional: offset and scale for partial sweeps
vec4 color = mix(color1, color2, t);
```

**Bilinear (4-Corner) Gradient:**
```glsl
vec4 color = mix(
    mix(color_bl, color_br, uv.x),
    mix(color_tl, color_tr, uv.x),
    uv.y
);
```

For mesh shapes, gradients are applied via vertex colors (bilinear) or UV-based sampling (linear/radial/angular).

---

### Dashes

Dashes are computed along the shape's path using a pattern function:

```glsl
// t = position along path, 0 to total_length
float pattern_pos = mod(t + dash_offset, dash_size + dash_spacing);
float in_dash = step(pattern_pos, dash_size);

// For soft edges on dash ends:
float dash_edge = smoothstep(0.0, aa_width, pattern_pos) *
                  smoothstep(0.0, aa_width, dash_size - pattern_pos);
```

**Dash parameters:**
- `dash_size` — Length of each dash in pixels
- `dash_spacing` — Gap between dashes in pixels
- `dash_offset` — Offset into pattern (for animation)
- `dash_type` — End style: `BASIC` (square ends), `ROUNDED` (semicircular ends), `ANGLED` (45° cut)

**Dash snapping:**
To avoid partial dashes at shape ends:
- `SNAP_NONE` — No adjustment, may have partial dashes
- `SNAP_TILING` — Adjust dash_size to fit whole number of pattern repeats
- `SNAP_ENDTOEND` — Ensure dashes at both endpoints, adjust spacing

**For each shape type:**
- **Line:** `t` is distance from start point
- **Arc/Ring:** `t` is arc length (angle × radius)
- **Rectangle:** `t` is perimeter distance from corner
- **Polyline:** `t` is accumulated length along path

---

### Outlines

Per-shape outline support renders both fill and stroke in one pass:

```glsl
float sdf = sdf_shape(...);
float outline_inner = sdf + outline_thickness;
float outline_outer = sdf;

// Fill alpha (inside the shape)
float fill_alpha = smoothstep(aa_width, -aa_width, sdf);

// Outline alpha (between sdf and sdf + thickness)
float outline_alpha = smoothstep(aa_width, -aa_width, outline_outer) -
                      smoothstep(aa_width, -aa_width, outline_inner);

// Composite: outline on top of fill
vec4 final_color = fill_color * fill_alpha;
final_color = mix(final_color, outline_color, outline_alpha);
```

For shapes where outline should be **outside** the fill (most common):
- Fill uses original SDF
- Outline is the region where `sdf > 0 && sdf < outline_thickness`

For shapes where outline should be **centered** on the edge:
- Expand the fill SDF by `outline_thickness / 2`
- Outline is centered on the original boundary

---

### Texture Fill

Shapes can be filled with a texture instead of solid color:

```glsl
vec4 tex_color = texture(u_texture, tex_uv);
float shape_mask = smoothstep(aa_width, -aa_width, sdf);
vec4 final_color = vec4(tex_color.rgb, tex_color.a * shape_mask);
```

**UV mapping modes:**
- `STRETCH` — Texture UVs match shape UVs (stretches to fit)
- `TILE` — Texture repeats at original aspect ratio
- `FIT` — Texture scaled to fit, maintaining aspect ratio

**Texture + tint:**
```glsl
vec4 final_color = tex_color * tint_color; // multiply tint
// or
vec4 final_color = mix(tex_color, tint_color, tint_amount); // blend tint
```

---

### End Caps

For lines and arcs, end caps define how the shape terminates:

**None:** Shape ends exactly at the boundary. For lines, this creates flat cuts perpendicular to the line direction.

**Square:** Shape extends past the endpoint by `thickness / 2`. The end is still flat.

**Round:** Semicircular cap centered on the endpoint with radius `thickness / 2`.

Implementation in SDF (for lines):
```glsl
// The basic line SDF with clamp(h, 0, 1) naturally creates round caps
// For square caps, extend endpoints before SDF calculation
// For no caps, reject pixels where h < 0 or h > 1
```

---

## Layer System

Each layer is a framebuffer plus a command queue:

### Command Structure

```c
typedef struct {
    uint8_t type;           // DISC, RING, ARC, PIE, RECT, RRECT, LINE, POLYGON, POLYLINE
    uint8_t blend_mode;     // ALPHA, ADDITIVE, MULTIPLY
    uint8_t gradient_type;  // NONE, LINEAR, RADIAL, ANGULAR, BILINEAR
    uint8_t cap_type;       // NONE, SQUARE, ROUND
    uint8_t dash_type;      // NONE, BASIC, ROUNDED, ANGLED
    uint8_t flags;          // HAS_OUTLINE, HAS_TEXTURE, etc.
    uint16_t reserved;

    // Transform at time of draw call (captured from stack)
    float transform[6];     // 2D affine matrix (2x3)

    // Colors
    uint32_t color;         // Primary fill color
    uint32_t color2;        // Gradient end / outline color
    uint32_t color3;        // Bilinear corner 3
    uint32_t color4;        // Bilinear corner 4

    // Shape parameters (meaning depends on type)
    float params[12];       // Position, size, radii, angles, thickness, etc.

    // Optional pointers for complex shapes
    void* points;           // For POLYGON/POLYLINE: point array
    void* holes;            // For POLYGON: hole arrays
    int point_count;
    int hole_count;

    // Texture (if HAS_TEXTURE)
    GLuint texture;
    uint8_t texture_mode;   // STRETCH, TILE, FIT
} DrawCommand;
```

Total: ~96 bytes per command (excluding point data for polygons/polylines).

### Layer Structure

```c
typedef struct {
    // Framebuffer
    GLuint fbo;
    GLuint color_texture;
    int width, height;

    // Command queue (deferred rendering)
    DrawCommand* commands;
    int command_count;
    int command_capacity;

    // Current state (for Lua draw calls)
    BlendMode current_blend;
    mat3 transform_stack[32];
    int transform_depth;

    // Composition settings
    BlendMode composite_blend;  // How layer composites to screen
    float opacity;
    char name[64];
} Layer;

#define MAX_LAYERS 16
Layer layers[MAX_LAYERS];
int layer_count = 0;
int layer_order[MAX_LAYERS];
```

### Layer API

**Raw C bindings (Phase 3):**
```lua
game = layer_create('game')
effects = layer_create('effects')
ui = layer_create('ui')

-- Drawing to layers (stores commands, no GPU work)
layer_circle(game, x, y, r, color)     -- Adds command to game.commands[]
layer_circle(effects, x, y, r, color)  -- Adds command to effects.commands[]
```

**YueScript wrappers (final API):** See Lua API section below.

### Frame-End Rendering

At frame end, the engine processes all command queues and renders:

```c
void render_frame(void) {
    // 1. Render each layer
    for (int i = 0; i < layer_count; i++) {
        render_layer(&layers[i]);
    }

    // 2. Composite layers to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // Screen
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < layer_count; i++) {
        int idx = layer_order[i];
        Layer* layer = &layers[idx];

        set_blend_mode(layer->composite_blend);
        bind_texture(layer->color_texture);
        draw_fullscreen_quad(layer->opacity);
    }

    // 3. Present
    SDL_GL_SwapWindow(window);
}

void render_layer(Layer* layer) {
    glBindFramebuffer(GL_FRAMEBUFFER, layer->fbo);
    glClear(GL_COLOR_BUFFER_BIT);

    // Batch state
    Batch batch = {0};
    ShaderType current_shader = SHADER_NONE;
    BlendMode current_blend = BLEND_ALPHA;
    GLuint current_texture = 0;

    // Process commands in order
    for (int i = 0; i < layer->command_count; i++) {
        DrawCommand* cmd = &layer->commands[i];

        // Determine required state
        ShaderType needed_shader = is_sdf_shape(cmd->type) ? SHADER_SDF : SHADER_MESH;
        GLuint needed_texture = (cmd->flags & HAS_TEXTURE) ? cmd->texture : white_texture;

        // Flush batch if state change required
        bool need_flush = (needed_shader != current_shader) ||
                          (needed_texture != current_texture) ||
                          (cmd->blend_mode != current_blend) ||
                          (batch.vertex_count >= MAX_BATCH_VERTICES);

        if (need_flush && batch.vertex_count > 0) {
            flush_batch(&batch, current_shader, current_texture, current_blend);
        }

        // Update state
        current_shader = needed_shader;
        current_texture = needed_texture;
        current_blend = cmd->blend_mode;

        // Build vertices for this command and add to batch
        build_command_vertices(cmd, &batch);
    }

    // Flush final batch
    if (batch.vertex_count > 0) {
        flush_batch(&batch, current_shader, current_texture, current_blend);
    }

    // Reset command queue for next frame
    layer->command_count = 0;

    // Apply post-process effects (Phase 4)
    // apply_layer_effects(layer);
}
```

### Batch Flushing

```c
void flush_batch(Batch* batch, ShaderType shader, GLuint texture, BlendMode blend) {
    // Set GL state
    glUseProgram(shader == SHADER_SDF ? sdf_program : mesh_program);
    glBindTexture(GL_TEXTURE_2D, texture);
    set_blend_mode(blend);

    // Upload vertices
    glBindBuffer(GL_ARRAY_BUFFER, batch_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    batch->vertex_count * sizeof(SdfVertex),
                    batch->vertices);

    // Draw
    glDrawElements(GL_TRIANGLES, batch->index_count, GL_UNSIGNED_SHORT, 0);

    // Reset batch
    batch->vertex_count = 0;
    batch->index_count = 0;
}
```

**Layer-specific effects:**
Each layer can have post-process effects applied before composition. This is Phase 4 (Effects) territory but the layer system should support it:
```lua
game:set_effect('outline', {color = 0x000000FF, thickness = 1})
```

---

## Transform Stack

Each layer maintains its own transform stack. Transforms are captured when commands are recorded (during Lua draw calls), then applied when building vertices at frame end.

### Stack Management

```c
// Each layer has a transform stack (see Layer struct)
// mat3 transform_stack[32];
// int transform_depth;

void layer_push(Layer* layer, float x, float y, float r, float sx, float sy) {
    // Build transform matrix
    mat3 m = mat3_identity();
    m = mat3_translate(m, x, y);
    m = mat3_rotate(m, r);
    m = mat3_scale(m, sx, sy);

    // Push onto stack
    layer->transform_depth++;
    layer->transform_stack[layer->transform_depth] =
        mat3_multiply(layer->transform_stack[layer->transform_depth - 1], m);
}

void layer_pop(Layer* layer) {
    layer->transform_depth--;
}

mat3 layer_get_transform(Layer* layer) {
    return layer->transform_stack[layer->transform_depth];
}
```

### Command Recording

When a draw call is made, the current transform is captured into the command:

```c
void layer_add_circle(Layer* layer, float x, float y, float r, uint32_t color) {
    DrawCommand cmd = {0};
    cmd.type = SHAPE_DISC;
    cmd.color = color;
    cmd.params[0] = x;
    cmd.params[1] = y;
    cmd.params[2] = r;

    // Capture current transform
    mat3 t = layer_get_transform(layer);
    cmd.transform[0] = t.m[0]; cmd.transform[1] = t.m[1];  // row 0
    cmd.transform[2] = t.m[3]; cmd.transform[3] = t.m[4];  // row 1
    cmd.transform[4] = t.m[6]; cmd.transform[5] = t.m[7];  // translation

    layer_add_command(layer, &cmd);
}
```

### Vertex Building (Frame End)

At frame end, transforms are applied when building vertices:

```c
void build_command_vertices(DrawCommand* cmd, Batch* batch) {
    // Build quad corners in local space
    vec2 corners[4] = { /* based on shape bounds */ };

    // Apply captured transform
    mat3 t = mat3_from_array(cmd->transform);
    for (int i = 0; i < 4; i++) {
        corners[i] = mat3_transform_point(t, corners[i]);
    }

    // Add transformed vertices to batch
    // ...
}
```

**Usage in Lua:**
```lua
game:push(100, 100, math.pi/4, 2, 2)  -- translate, rotate 45°, scale 2x
    game:rectangle(0, 0, 50, 50, color)  -- command captures this transform
game:pop()
-- At frame end: vertices built with the captured transform
```

### SDF and Transforms

For SDF shapes, the SDF is computed in UV space (0-1 across the quad). Rotation works fine — the quad rotates and the SDF is computed relative to the rotated quad.

Non-uniform scale can distort the SDF (circles become ellipses). Solutions:
1. Store scale in command, pass to shader for SDF correction
2. Only support uniform scale for SDF shapes
3. Compute SDF in world space (more complex)

Recommended: Store scale factors in command, correct AA width in shader:
```glsl
// Per-vertex or per-command scale factors
float corrected_aa = aa_width / min(scale.x, scale.y);
```

---

## Blend Modes

Three blend modes, set per-layer or per-draw-call:

**Alpha (default):**
```c
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
// result = src * src.a + dst * (1 - src.a)
```
Standard transparency. New pixels blend over existing based on alpha.

**Additive:**
```c
glBlendFunc(GL_SRC_ALPHA, GL_ONE);
// result = src * src.a + dst
```
New pixels add to existing. Good for glows, particles, light effects.

**Multiply:**
```c
glBlendFunc(GL_DST_COLOR, GL_ZERO);
// result = src * dst
```
Darkens based on source color. Good for shadows, darkening effects.

**Per-draw-call blend mode:**
```lua
game:set_blend_mode('additive')
game:circle(x, y, r, color)  -- command stores blend_mode = ADDITIVE
game:set_blend_mode('alpha')  -- subsequent commands use ALPHA
```

Blend mode is stored per-command. At frame end, changing blend mode between commands triggers a batch flush.

---

## Lua API

Anchor exposes **two API levels**:

1. **Raw C bindings** — Plain functions called from Lua, used during Phase 3 implementation
2. **YueScript wrappers** — OOP-style methods built on top of C bindings, the final user-facing API

### Raw C Bindings (Phase 3)

```lua
-- Layer management
local game = layer_create('game')

-- Shapes (all take layer as first argument)
layer_circle(game, x, y, radius, color)
layer_rectangle(game, x, y, w, h, color)
layer_line(game, x1, y1, x2, y2, thickness, color)

-- Transforms
layer_push(game, x, y, r, sx, sy)
layer_pop(game)

-- State
layer_set_blend_mode(game, 'additive')

-- Helpers
local red = rgba(255, 0, 0, 255)
```

### YueScript API (Final)

The following shows the user-facing API after YueScript wrappers are built:

### Layer Management

```lua
-- Create layers (typically at startup)
local game = an:layer('game')
local effects = an:layer('effects')
local ui = an:layer('ui')

-- Layer settings
game:set_blend_mode('alpha')  -- 'alpha' or 'additive'
game:set_opacity(1.0)         -- 0.0 to 1.0
```

### Transform Stack

```lua
game:push(x, y, r, sx, sy)  -- Push transform (all params optional, default to identity)
game:pop()                   -- Pop transform

-- Typical usage
game:push(player.x, player.y, player.angle, player.scale, player.scale)
    game:circle(0, 0, 10, colors.white)  -- Drawn at player position/rotation/scale
game:pop()
```

### Basic Shapes

```lua
-- Disc (filled circle)
game:disc(x, y, radius, color)
game:disc(x, y, radius, {
    color = 0xFFFFFFFF,
    outline_color = 0x000000FF,
    outline_thickness = 2,
    gradient = 'radial',
    gradient_color = 0x0000FFFF,
})

-- Ring (circle outline / donut)
game:ring(x, y, inner_radius, outer_radius, color)
game:ring(x, y, inner, outer, { ... })  -- same options as disc

-- Arc (ring segment)
game:arc(x, y, inner_radius, outer_radius, start_angle, end_angle, color)
game:arc(x, y, inner, outer, start, end_angle, {
    color = 0xFFFFFFFF,
    cap = 'round',  -- 'none', 'square', 'round'
    outline_color = 0x000000FF,
    outline_thickness = 1,
})

-- Pie (filled wedge)
game:pie(x, y, radius, start_angle, end_angle, color)
game:pie(x, y, radius, start, end_angle, { ... })

-- Rectangle
game:rectangle(x, y, width, height, color)
game:rectangle(x, y, w, h, {
    color = 0xFFFFFFFF,
    corner_radius = 5,           -- single value or {tl, tr, br, bl}
    outline_color = 0x000000FF,
    outline_thickness = 2,
})

-- Line
game:line(x1, y1, x2, y2, color, thickness)
game:line(x1, y1, x2, y2, {
    color = 0xFFFFFFFF,
    thickness = 2,
    cap = 'round',  -- 'none', 'square', 'round'
})
```

### Complex Shapes

```lua
-- Polygon (arbitrary shape)
game:polygon(points, color)
game:polygon(points, {
    color = 0xFFFFFFFF,
    holes = { hole_points_1, hole_points_2 },
    outline_color = 0x000000FF,
    outline_thickness = 2,
})
-- points = {{x1, y1}, {x2, y2}, ...} or {x1, y1, x2, y2, ...}

-- Polyline
game:polyline(points, color, thickness)
game:polyline(points, {
    color = 0xFFFFFFFF,
    thickness = 2,
    join = 'round',  -- 'simple', 'miter', 'bevel', 'round'
    cap = 'round',   -- 'none', 'square', 'round'
    closed = false,  -- if true, connects last point to first
    miter_limit = 4, -- for miter joins
})
```

### Gradients

```lua
-- Linear gradient
game:disc(x, y, r, {
    gradient = 'linear',
    color = 0xFF0000FF,        -- start color
    gradient_color = 0x0000FFFF, -- end color
    gradient_angle = 0,         -- direction in radians (0 = left to right)
})

-- Radial gradient
game:disc(x, y, r, {
    gradient = 'radial',
    color = 0xFFFFFFFF,         -- center color
    gradient_color = 0x000000FF, -- edge color
})

-- Angular gradient
game:disc(x, y, r, {
    gradient = 'angular',
    color = 0xFF0000FF,
    gradient_color = 0x0000FFFF,
    gradient_angle = 0,  -- start angle
})

-- Bilinear (4-corner) gradient
game:rectangle(x, y, w, h, {
    gradient = 'bilinear',
    color_tl = 0xFF0000FF,  -- top-left
    color_tr = 0x00FF00FF,  -- top-right
    color_br = 0x0000FFFF,  -- bottom-right
    color_bl = 0xFFFF00FF,  -- bottom-left
})
```

### Dashes

```lua
game:line(x1, y1, x2, y2, {
    color = 0xFFFFFFFF,
    thickness = 2,
    dash_size = 10,
    dash_spacing = 5,
    dash_offset = 0,        -- animate this for marching ants
    dash_type = 'round',    -- 'basic', 'round', 'angled'
    dash_snap = 'none',     -- 'none', 'tiling', 'endtoend'
})

-- Dashes work on arcs, rings, rectangles too
game:arc(x, y, inner, outer, start, end_angle, {
    dash_size = 10,
    dash_spacing = 5,
})
```

### Texture Fill

```lua
local tex = an:texture_load('pattern.png')

game:disc(x, y, r, {
    texture = tex,
    texture_mode = 'tile',  -- 'stretch', 'tile', 'fit'
    color = 0xFFFFFFFF,     -- tint color (multiplied)
})

game:rectangle(x, y, w, h, {
    texture = tex,
    texture_mode = 'stretch',
})
```

### Blend Modes

```lua
game:set_blend_mode('additive')
game:disc(x, y, r, glow_color)  -- additive glow
game:set_blend_mode('alpha')    -- back to normal
```

---

## Implementation Phases

**Note:** Following Cute Framework's approach, we use a single uber-shader for all shapes (SDF and sprites). The mesh fallback is only needed for polygons >8 vertices.

### Phase 3A: Core Infrastructure (Done in Phase 3 Part 1)

1. **Framebuffer setup** ✓
   - Layer struct with FBO at game resolution
   - Nearest-neighbor filtering
   - Blit to screen with aspect-ratio scaling

2. **Command queue system** ✓
   - DrawCommand struct definition
   - Per-layer command arrays with dynamic growth
   - Command recording from Lua draw calls

3. **Vertex buffer management** ✓
   - Dynamic vertex buffer for batching
   - Currently using GL_TRIANGLES (6 verts/quad)
   - Can optimize to indexed quads later

4. **Basic shader pipeline** ✓
   - SDF uber-shader with type branching (RECT, CIRCLE, SPRITE)
   - Shape filter modes: smooth (anti-aliased) vs rough (pixel-perfect)
   - Uniform setup (projection matrix, aa_width) ✓

5. **Frame-end renderer**
   - Process command queues in order
   - Build vertices from commands
   - Batch and flush with state change detection
   - Track current shader/texture/blend mode

### Phase 3B: SDF Shapes (Circle/Box/Sprite Done)

Following Cute Framework, implement these SDFs in the uber-shader:

1. **Circle (Disc)** ✓
   ```glsl
   float sdf_circle(vec2 p, vec2 center, float radius) {
       return length(p - center) - radius;
   }
   // Also: sdf_circle_pixel() using superellipse (n=1.95) for pixel-art style
   // Radius snapping in rough mode for consistent pixel shapes
   ```

2. **Box (Rectangle)** ✓
   ```glsl
   float sdf_rect(vec2 p, vec2 center, vec2 half_size) {
       vec2 d = abs(p - center) - half_size;
       return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
   }
   ```

3. **Sprite** ✓
   - Texture sampling with texel center snapping for pixel-perfect rendering
   - Batch flush on texture change
   - Works with transform stack (rotation, scale)

4. **Segment (Line/Capsule)**
   ```glsl
   float sdf_segment(vec2 p, vec2 a, vec2 b) {
       vec2 pa = p - a, ba = b - a;
       float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
       return length(pa - ba * h);
   }
   ```

4. **Triangle**
   - Three-point SDF using edge distances

5. **Polyline Corners** (Cute Framework approach)
   - Each corner = union of two segments: `min(sdf_segment(p, A, B), sdf_segment(p, B, C))`
   - Build corner geometry on CPU with careful shared-edge computation
   - Avoids complex join geometry

6. **Polygon (up to 8 vertices)**
   ```glsl
   // Referenced from: https://www.shadertoy.com/view/wdBXRW
   float sdf_polygon(vec2 p, vec2[8] v, int N) { ... }
   ```
   - For >8 vertices: fall back to ear-clipping triangulation (no SDF)

**Deferred to later phases:**
- Arc and Pie (complex angular math)
- Ring (can be approximated with stroke on circle)
- Four-corner radius variants

### Phase 3C: Styling Features

1. **Outlines**
   - Implement fill + outline in single pass
   - Test outline on all shape types

2. **Gradients**
   - Linear gradient
   - Radial gradient
   - Angular gradient
   - Bilinear gradient
   - Test on various shapes

3. **Dashes**
   - Basic dash pattern
   - Rounded/angled dash ends
   - Snap modes
   - Test on lines, arcs, rectangles

4. **Texture fill**
   - Texture binding in batch
   - UV mapping modes
   - Test with various textures and shapes

### Phase 3D: Mesh Shapes

1. **Polygon triangulation**
   - Implement ear clipping algorithm
   - Hole merging via bridge edges
   - Test with complex shapes

2. **Polyline generation**
   - Basic segment quads
   - Join types: simple, miter, bevel, round
   - Cap types at endpoints
   - Test various configurations

3. **Mesh shader integration**
   - Gradient support for mesh shapes
   - Batch management for mesh vs SDF

### Phase 3E: Layer System

1. **Multiple framebuffers**
   - Create/destroy layers
   - Per-layer clear color

2. **Layer targeting**
   - Draw commands target specific layer
   - Current layer state

3. **Composition**
   - Composite layers to screen
   - Per-layer blend mode and opacity

4. **Transform stack**
   - Matrix math utilities
   - Push/pop implementation
   - Integration with vertex generation

### Phase 3F: Polish and Verification

1. **API refinement**
   - Consistent parameter ordering
   - Sensible defaults
   - Error handling

2. **Performance verification**
   - Batch efficiency
   - Draw call counts
   - Memory usage

3. **Visual verification**
   - All shapes render correctly
   - Anti-aliasing quality
   - Gradient smoothness
   - Dash patterns

4. **Web build verification**
   - Test all features on WebGL
   - Shader compatibility
   - Performance comparison

---

## Testing & Verification

### Visual Test Cases

1. **Shape gallery** — All shapes at various sizes and rotations
2. **Anti-aliasing test** — Shapes at sub-pixel positions and sizes
3. **Gradient test** — All gradient types on all applicable shapes
4. **Dash test** — Various dash patterns on lines, arcs, rectangles
5. **Outline test** — Fill + outline on all shapes
6. **Texture test** — Texture fill with different mapping modes
7. **Blend mode test** — Overlapping shapes with different blend modes
8. **Transform test** — Shapes under various push/pop transforms
9. **Layer test** — Multiple layers with different blend modes

### Performance Benchmarks

1. **Many circles** — 1000+ discs, verify batching
2. **Mixed shapes** — Interleaved shape types, measure draw calls
3. **Complex polygon** — Large polygon with many holes
4. **Long polyline** — Polyline with many points and round joins

### Edge Cases

1. **Zero-size shapes** — Should not crash or render garbage
2. **Sub-pixel shapes** — Should fade smoothly
3. **Extreme transforms** — Very large scale, very small scale
4. **Degenerate polygons** — Collinear points, self-intersection
5. **Full-circle arcs** — start_angle ≈ end_angle + 2π

---

## Open Questions

### Coordinate System Convention

**Option A:** Position is center (like disc/ring naturally are)
```lua
game:rectangle(100, 100, 50, 30, color)  -- centered at (100, 100)
```

**Option B:** Position is top-left (like LÖVE/SDL)
```lua
game:rectangle(100, 100, 50, 30, color)  -- top-left at (100, 100)
```

**Recommendation:** Center for circular shapes (disc, ring, arc, pie), top-left for rectangular shapes (rectangle, rounded_rectangle). This matches intuition. Or provide `ox, oy` offset parameters.

### Angle Convention

**Radians** — Mathematically standard, what the shader uses internally
**Degrees** — More intuitive for most users
**Turns** — 0-1 range, sometimes convenient

**Recommendation:** Accept radians in the API (consistent with `math.sin`, `math.cos`), provide helpers like `an.deg2rad(45)` for convenience.

### Color Format

**RGBA hex** — `0xRRGGBBAA` (current plan)
**ARGB hex** — `0xAARRGGBB` (common in some systems)
**Table** — `{r=1, g=0, b=0, a=1}` or `{1, 0, 0, 1}`

**Recommendation:** RGBA hex as primary, support table format as alternative.
