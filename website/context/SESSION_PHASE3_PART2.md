# Phase 3 Part 2 — Session Progress

**This file tracks progress for the current session. Delete after session ends.**

---

## Starting State

- Layer struct with FBO, command queue, transform stack
- DrawCommand with type, blend_mode, color, transform, params
- Batch rendering (GL_TRIANGLES, 6 verts/quad)
- Rectangle rendering (geometry-based, no SDF yet)
- Lua bindings: `layer_create()`, `layer_rectangle()`, `rgba()`
- Verified on Windows and Web

### Current Vertex Format

```c
// 6 floats per vertex: x, y, r, g, b, a
#define VERTEX_FLOATS 6
```

**Problem:** No UV, no shape type, no SDF params. Cannot implement SDF shapes.

---

## Plan for Step 5

1. **Expand vertex format** to include UV, type, and shape params
2. **Modify shader** to uber-shader with type branching and SDF
3. **Convert rectangles to SDF** (not just geometry)
4. **Add circle SDF** and `layer_circle()` binding
5. **Test both shapes together**

### New Vertex Format (13 floats = 52 bytes)

```c
typedef struct {
    float x, y;        // vertex position (world space)
    float u, v;        // UV (0-1 across quad)
    float r, g, b, a;  // color
    float type;        // shape type (0=rect, 1=circle, 2=sprite)
    float shape[4];    // shape params
} Vertex;

// Shape params by type:
// RECT:   center.x, center.y, half_w, half_h
// CIRCLE: center.x, center.y, radius, (unused)
// SPRITE: min_u, min_v, max_u, max_v
```

### Key Decisions from Cute Framework

1. **Single uber-shader** for all shapes (not two shaders)
2. **Shape data in vertices** — uniform format, shader interprets based on type
3. **World-space SDF** — shape params in world coords, not UV-normalized
4. **Anti-aliasing** — `smoothstep(aa, -aa, distance)` with fixed AA width (~1.5px)

---

## Progress

- [ ] Update vertex format in anchor.c
- [ ] Update vertex attributes in VAO setup
- [ ] Update batch_add_vertex and related functions
- [ ] Write new uber-shader with SDF functions
- [ ] Convert process_rectangle to use new format + SDF
- [ ] Add process_circle
- [ ] Add layer_circle Lua binding
- [ ] Test on Windows
- [ ] Test on Web

---

## Notes

(Add implementation notes here as we go)
