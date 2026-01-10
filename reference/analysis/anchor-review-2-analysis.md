# Anchor.c Analysis Report

## Overall Structure

The file is well-organized into logical sections:
1. Includes, defines, stb setup (1-100)
2. Structs and constants (100-500)
3. Physics/input structs (500-800)
4. Font/audio/input internals (800-2700)
5. Rendering pipeline (2700-3200)
6. Lua bindings by subsystem (3200-5900)
7. Main loop and initialization (5900-6857)

---

## Issues Found

### 1. Redundant Forward Declaration (line 2899-2900)

```c
// Forward declaration of batch_flush (needed for process_sprite)
static void batch_flush(void);
```

But `batch_flush` is already **fully defined** at lines 2789-2808, earlier in the file. This forward declaration is unnecessary and misleading.

### 2. Outdated Comment (line 2633)

```c
// Will return analog values when gamepad support is added (Step 6)
static float input_get_axis(const char* negative, const char* positive) {
```

Gamepad support **was** added in Phase 5. But `input_get_axis` still only checks digital on/off via `action_is_down`. It doesn't actually query `gamepad_axes[]` for analog stick values. This is either:
- An incomplete feature (the analog stick doesn't feed into this function)
- Or working as intended (actions are digital), but the comment is wrong

### 3. Unused Static Functions (lines 6166-6196)

```c
static void shader_set_float(GLuint shader, const char* name, float value) {...}
static void shader_set_vec2(GLuint shader, const char* name, float x, float y) {...}
static void shader_set_vec4(GLuint shader, const char* name, float x, float y, float z, float w) {...}
static void shader_set_int(GLuint shader, const char* name, int value) {...}
static void shader_set_texture(GLuint shader, const char* name, GLuint texture, int unit) {...}
```

These are never called. The engine uses deferred uniform setting via `layer_shader_set_*` (the command queue approach). These immediate-mode setters should either be removed or exposed to Lua for edge cases.

### 4. Naming Inconsistencies

| Function | Current Name | Expected Pattern |
|----------|--------------|------------------|
| `l_is_down` | no prefix | `l_input_is_down` |
| `l_is_pressed` | no prefix | `l_input_is_pressed` |
| `l_is_released` | no prefix | `l_input_is_released` |
| `l_rgba` | no prefix | `l_color_rgba` or just keep as utility |
| `l_noise` | no prefix | `l_random_noise` or `l_perlin_noise` |

The registered Lua names are fine (`is_down`, `rgba`, `noise`), but the C function names should follow the `l_<subsystem>_<operation>` pattern for internal consistency.

### 5. Field Repurposing (Distasteful Pattern)

In the DrawCommand struct, `texture_id` is repurposed to store shader handles for uniform commands:

```c
cmd->texture_id = shader;  // Reuse texture_id field for shader handle (line 2675)
cmd->color = (uint32_t)loc;  // Store uniform location (line 2689)
```

This works but is confusing. A union would be cleaner, or separate command types with appropriate fields.

### 6. Missing Resource Cleanup

**Textures:** No `texture_unload` function exists. All textures loaded via `texture_load` live until program exit. If a game loads/unloads many textures dynamically, this could leak memory.

**Effect shaders:** `effect_shader_destroy` exists but there's no registry or automatic cleanup on shutdown.

### 7. Engine State Not Exposed to Lua

The docs mention `an.frame`, `an.step`, `an.game_time` but these globals (lines 5889-5891) are never exposed to Lua:

```c
static Uint64 step = 0;
static double game_time = 0.0;
static Uint64 frame = 0;
```

### 8. physics_raycast Tag Filtering Issue

In `l_physics_raycast` (line 4987-4993):

```c
// Check tag match (CastRayClosest doesn't do custom filtering)
int tag_index = (int)(uintptr_t)b2Shape_GetUserData(result.shapeId);
PhysicsTag* tag = physics_tag_get(tag_index);
if (!tag || (tag->category_bit & mask) == 0) {
    lua_pushnil(L);
    return 1;
}
```

If the closest shape doesn't match the tag filter, the function returns nil — even if there are *other* shapes along the ray that would match. This differs from `physics_raycast_all` which properly filters all results.

This might be intentional (first hit or nothing), but it's subtly different from what users might expect.

### 9. PI Redefinition

Line 5091:
```c
#define PI 3.14159265358979323846
```

This is defined locally near the random functions. It could conflict with `M_PI` from math.h. Should use a consistent definition at the top.

### 10. Silent Failures in Input System

Several input functions print warnings but return success:

```c
// line 2020-2022
printf("Warning: Invalid control string '%s'\n", control_str);
return false;
```

This is fine, but Lua code calling these won't see the warning unless watching stdout. Consider logging to a Lua-accessible error buffer.

### 11. Event Buffer Overflow

Physics events have fixed limits (e.g., `MAX_PHYSICS_EVENTS = 512`). Overflow just prints a warning and drops events:

```c
if (contact_begin_count >= MAX_PHYSICS_EVENTS) {
    printf("Warning: Contact begin event buffer full!\n");
    // event dropped
}
```

For most games this is fine, but high-entity-count games could silently lose collision data.

---

## What's Working Well

1. **Error handling in Lua bindings** is consistent — most use `luaL_error` for fatal errors
2. **Resource cleanup** in `engine_shutdown` is thorough for what it tracks
3. **Deferred rendering pipeline** is well-implemented with the command queue
4. **Physics event system** design is clean (tags + masks + event buffers)
5. **Input system** is comprehensive (actions, chords, sequences, holds, capture mode)
6. **Code is readable** — minimal comments, but logic is clear

---

## Recommendations

### Must Fix
1. Remove redundant `batch_flush` forward declaration
2. Update or remove outdated gamepad comment
3. Delete unused `shader_set_*` functions (or expose them)
4. Expose `frame`, `step`, `game_time` to Lua

### Should Fix
5. Add `texture_unload` function
6. Rename internal functions for consistency (`l_is_down` → `l_input_is_down`, etc.)
7. Consider the `physics_raycast` behavior — document it or change it

### Nice to Have
8. Use a union for DrawCommand to avoid field repurposing
9. Move `PI` definition to top with other defines, or use `M_PI`
10. Consider a `GAME_WIDTH`/`GAME_HEIGHT` Lua getter
