# Anchor Review 3 - Findings

Review of `anchor.c` code quality, organization, and documentation accuracy.

---

## Code Quality Issues

### 1. Outdated FILE STRUCTURE Line Numbers

The header comment at lines 1-18 provides a file structure index with line numbers, but these are likely out of sync with the current file structure. This comment should be updated to reflect current line numbers, or removed if maintenance burden is too high.

### 2. Leftover "Step N:" Development Comments

Several functions contain remnant comments from phased development that could be cleaned up:

- Line 4698: `// Step 7: Body properties - Position/rotation setters`
- Line 4734: `// Step 7: Body properties - Velocity`
- Line 4777: `// Step 7: Body properties - Forces/impulses`
- Line 4848: `// Step 7: Body properties - Damping, gravity scale, fixed rotation, bullet`
- Line 4901: `// Step 7: Body properties - User data`
- Line 4922: `// Step 7: Shape properties - Friction and restitution`
- Line 5050: `// Step 8: Debug function to print event counts`

These could be:
- Removed entirely (the section banners already organize the file)
- Converted to more descriptive subsection comments

### 3. Outdated Cross-Reference Line Numbers

Line 5059 contains: `// See also: Event buffer structs at ~line 250, event processing at ~line 370`

These line references may be outdated and could cause confusion. Consider either:
- Removing these cross-references
- Using search terms instead (e.g., "see PhysicsContactBeginEvent struct" or "see process_physics_events")

---

## API Consistency Issue

### Polygon Vertex Format Inconsistency

**Critical**: Two functions use different vertex formats:

**`physics_add_polygon`** (line 4629-4695):
- Expects a **flat array**: `{x1, y1, x2, y2, x3, y3, ...}`
- Example: `physics_add_polygon(body, "wall", {0, 0, 50, 0, 25, 40})`

**`physics_query_polygon`** (line 5543-5595):
- Expects an **array of tables**: `{{x1, y1}, {x2, y2}, {x3, y3}, ...}`
- Example: `physics_query_polygon(100, 100, {{-20, -20}, {20, -20}, {0, 30}}, {"enemy"})`

This inconsistency is confusing for users. Recommended actions:

**Option A**: Unify to flat arrays (simpler, matches add_polygon)
**Option B**: Unify to nested tables (more readable, self-documenting)
**Option C**: Support both formats in both functions (more complex, but backwards compatible)

---

## Documentation Accuracy

### ENGINE_API.md Status

**All functions documented**: The documentation in ENGINE_API.md appears complete and accurate. All Lua-registered functions have corresponding documentation entries.

**Polygon vertex format**: ENGINE_API.md correctly documents the inconsistency:
- `physics_add_polygon`: "Vertices: {x1, y1, x2, y2, ...}" (flat array)
- `physics_query_polygon`: "Vertices are {{x,y}, {x,y}, ...}" (nested tables)

While accurately documented, this could be improved with a note highlighting the format difference.

### ENGINE_API_QUICK.md Status

Signatures match the implementation. All functions are present.

---

## Code Organization Assessment

### Positive Aspects

1. **Clear section banners** - The `// ============================================================================` banners with section titles make navigation easy

2. **Logical grouping** - Functions are grouped by system (rendering, physics, random, input)

3. **Consistent naming** - Functions follow clear prefixing patterns:
   - `l_*` for Lua binding functions
   - `layer_*` for layer operations
   - `physics_*` for physics operations
   - etc.

4. **Good error handling** - Most functions validate inputs and return meaningful Lua errors

5. **Consistent coordinate conversion** - Physics functions consistently convert between pixels and meters using `pixels_per_meter`

### Potential Improvements (Non-Critical)

1. **Shader uniform functions are split** - There are two sets:
   - Immediate: `l_shader_set_float`, `l_shader_set_vec2`, etc. (lines 3990-4036)
   - Deferred: `l_layer_shader_set_float`, `l_layer_shader_set_vec2`, etc. (lines 4038-4077)

   These are in the same area but could benefit from clearer separation or a comment explaining the difference.

2. **Query polygon vertex format** - Already discussed above.

---

## Summary

### High Priority
- **Polygon vertex format inconsistency** - User-facing API inconsistency that could cause bugs

### Low Priority (Cleanup)
- Outdated FILE STRUCTURE line numbers
- Leftover "Step N:" comments
- Outdated cross-reference line numbers

### Documentation
- ENGINE_API.md is accurate and complete
- ENGINE_API_QUICK.md is accurate and complete
- Consider adding a note about the polygon vertex format difference

---

## Recommended Actions

1. **Decide on polygon format** - Choose one format and update the inconsistent function
2. **Remove or update** the FILE STRUCTURE header line numbers
3. **Remove** the "Step N:" development comments
4. **Update or remove** the cross-reference line numbers at line 5059

---

## Actions Taken

All issues have been resolved:

1. **Polygon vertex format** - Unified to flat arrays. `physics_query_polygon` now uses `{x1, y1, x2, y2, ...}` format, matching `physics_add_polygon`.

2. **Immediate shader functions** - Renamed for clarity:
   - `shader_set_float` → `shader_set_float_immediate`
   - `shader_set_vec2` → `shader_set_vec2_immediate`
   - `shader_set_vec4` → `shader_set_vec4_immediate`
   - `shader_set_int` → `shader_set_int_immediate`

3. **FILE STRUCTURE header** - Removed line numbers, now just lists sections with "(search for section banners)" guidance.

4. **Step N comments** - All removed.

5. **Cross-reference comment** - Removed the outdated line number references.

6. **Documentation** - Updated both ENGINE_API.md and ENGINE_API_QUICK.md to reflect all changes.

7. **Build** - Engine compiles successfully.

---

## Framework Review (Continuation)

Reviewed all 14 YueScript framework files for issues and consistency.

### Issues Found and Fixed

**1. Polygon vertex format documentation (2 locations)**
- `init.yue` line 853: `query_polygon` example showed nested tables `{{0, 0}, {100, 0}, {50, 100}}`
- `collider.yue` line 128: `add_polygon` example showed nested tables `{{-16, -16}, ...}`
- **Fixed**: Both now show flat arrays `{0, 0, 100, 0, 50, 100}` with note "Vertices are a flat array: {x1, y1, x2, y2, ...}"

**2. PHASE_10_PROGRESS.md outdated shader function names**
- Lines 580 and 598 used `shader_set_vec2` instead of `shader_set_vec2_immediate`
- **Fixed**: Updated to use `_immediate` suffix

**3. CLAUDE.md missing framework API references**
- "Read First" section didn't mention the new framework API documentation
- **Fixed**: Added "Framework API" section with links to FRAMEWORK_API.md and FRAMEWORK_API_QUICK.md

### Documentation Created

**FRAMEWORK_API_QUICK.md** (~250 lines)
- Compact function signatures for all framework classes
- Same format as ENGINE_API_QUICK.md

**FRAMEWORK_API.md** (~1200 lines)
- Detailed documentation with examples for:
  - Root Object (an): resources, audio, physics, input, actions
  - Object: lifecycle, tags, linking, actions, aliases
  - Layer: drawing, transforms, shaders, rendering pipeline
  - Collider: physics body wrapper
  - Timer: delays, repeating, tweening, conditionals
  - Camera: viewport, coordinate conversion, following
  - Spring: damped spring animations
  - Shake: screen shake effects
  - Random: seeded RNG
  - Color: mutable RGB/HSL
  - Math: lerp, easing functions
  - Array: utilities

### Code Quality Assessment

**No issues found in:**
- object.yue - clean object tree implementation
- layer.yue - proper FBO wrapper
- image.yue, font.yue - simple, correct wrappers
- timer.yue - comprehensive timer system
- camera.yue - proper coordinate conversion
- math.yue - correct easing implementations
- spring.yue, shake.yue - clean animation systems
- random.yue - proper RNG wrapper
- color.yue - correct RGB/HSL synchronization
- array.yue - comprehensive utilities

### Remaining Work

**website/context/ needs sync**
- Missing FRAMEWORK_API.md and FRAMEWORK_API_QUICK.md
- Should be synced at session end using: `cp .claude/CLAUDE.md docs/* website/context/`
