# Phase 10 Progress

This document captures the current state of Phase 10 implementation and decisions made.

---

## Project Structure

We moved away from submodules and symlinks to a simpler copy-based approach.

### Anchor Repository

```
Anchor/
├── engine/
│   ├── src/anchor.c
│   ├── build/
│   │   └── anchor.exe
│   ├── build.bat           # Build C code (desktop)
│   ├── build-web.sh        # Build C code (web)
│   └── run.bat             # Run engine with framework/ (no yue compile)
├── framework/              # Framework testing environment
│   ├── anchor/             # Master framework (YueScript source)
│   │   ├── init.yue
│   │   ├── object.yue
│   │   ├── layer.yue
│   │   ├── image.yue
│   │   ├── font.yue
│   │   ├── timer.yue
│   │   ├── collider.yue
│   │   ├── spring.yue
│   │   ├── camera.yue
│   │   ├── shake.yue
│   │   ├── random.yue
│   │   ├── math.yue
│   │   ├── color.yue
│   │   └── array.yue
│   ├── assets/             # Test assets
│   ├── main.yue            # Test file
│   ├── yue.exe             # YueScript compiler
│   ├── run.bat             # Compile .yue + run desktop
│   └── run-web.bat         # Compile .yue + build web + run browser
├── docs/
├── reference/
└── scripts/
```

- `engine/` contains C code, build artifacts, and engine-level scripts
- `framework/` contains the master framework and test environment
- `framework/anchor/` is the master copy of framework classes
- Test files use `require 'anchor'` (same as games)

### Game Repository

```
emoji-ball-battles/         # (or any game)
├── tools/
│   ├── anchor.exe          # Copied from Anchor/engine/build/
│   └── yue.exe             # YueScript compiler
├── anchor/                 # Framework (copied from Anchor/framework/anchor/)
│   ├── init.yue
│   ├── object.yue
│   ├── layer.yue
│   └── ...
├── main.yue                # Game code
└── assets/
```

- Each game is self-contained
- No submodules, no symlinks
- Framework files are copied, not linked
- Both games and Anchor use `require 'anchor'` (same require path)

---

## Build and Run Workflow

### Framework Development (YueScript)

From `Anchor/framework/`:
```bash
./run.bat           # Compile .yue + run desktop
./run-web.bat       # Compile .yue + build web + run browser
```

The `run.bat` script:
1. Compiles `main.yue` and `anchor/*.yue` with `-r` flag (preserves line numbers)
2. Runs `../engine/build/anchor.exe .`

### Engine Development (C)

From `Anchor/engine/`:
```bash
./build.bat         # Build engine for desktop
./run.bat           # Run engine with ../framework (no yue compile)
./build-web.sh ../framework   # Build for web with framework folder
```

The C engine calls a global `update(dt)` function in Lua each frame.

---

## Framework Architecture

### Global Update Function

The C engine calls a single global `update(dt)` function. Everything else happens on the Lua/YueScript side. The C side will not change further.

### init.yue

```yuescript
global *

require 'anchor.object'
require 'anchor.layer'
require 'anchor.image'
require 'anchor.font'
require 'anchor.timer'
require 'anchor.math'
require 'anchor.collider'
require 'anchor.spring'

an = object 'an'
an.layers = {}
an.images = {}
an.fonts = {}

-- Resource registration methods
an.layer = (name) => ...
an.image = (name, path) => ...
an.font = (name, path, size) => ...

update = (dt) ->
  all_objects = {an}
  all_objects[] = obj for obj in *an\all!
  obj\_early_update dt for obj in *all_objects
  obj\_update dt for obj in *all_objects
  obj\_late_update dt for obj in *all_objects
  an\cleanup!
```

- Creates global root object `an`
- Defines global `update` function called by C
- Collects all objects (including `an`) via `an\all!`
- Runs three phases: early, main, late
- Calls cleanup at end of frame

### object.yue

The object class is now fully documented with comments for each method. Key methods:

**Tree Management:**
- `new(name)` — Creates object with optional name, initializes parent/children/dead/tags
- `add(child)` — Adds child with bidirectional named links, kills existing child with same name
- `all(tag)` — Returns ALL descendants (including dead) via iterative DFS, optional tag filter
- `kill(tag)` — Marks self and descendants as dead; with tag, kills matching objects and their subtrees

**Tagging:**
- `tag(...)` — Adds one or more tags (set semantics: `@tags[t] = true`)
- `is(name_or_tag)` — Returns truthy if name matches OR tag exists

**Horizontal Links:**
- `link(target, callback)` — When target dies, callback runs (or self dies if no callback)

**Initialization:**
- `set(properties)` — Assigns properties from a table to the object
- `build(build_function)` — Runs a build function with self as argument
- `flow_to(parent)` — Adds self to parent (reverse of add, for fluent chaining)

**Actions:**
- `early_action(name_or_fn, fn)` — Adds action for early phase
- `action(name_or_fn, fn)` — Adds action for main phase
- `late_action(name_or_fn, fn)` — Adds action for late phase

**Internal:**
- `_early_update(dt)` — Runs early_update method + early_actions
- `_update(dt)` — Runs update method + actions
- `_late_update(dt)` — Runs late_update method + late_actions
- `cleanup()` — Removes marked actions and dead children from tree

---

## Action System

### Storage

Actions are stored as parallel arrays:
- `@actions` — Array of functions
- `@action_names` — Array of strings (or `false` for anonymous)

Named actions are also accessible as `@[name]`.

### Anonymous vs Named

```yuescript
-- Anonymous action
@\action -> print "runs every frame"

-- Named action (accessible as @move)
@\action 'move', -> @x += @speed * dt

-- One-shot (returns true to be removed)
@\action -> @lifetime -= dt; @lifetime <= 0
```

### Replacement

When adding a named action that already exists:
1. Find index where name matches
2. Overwrite `@actions[i]` in place (old function garbage collected)
3. Update `@[name]` reference

No array manipulation needed.

### Removal

When an action returns `true`:
1. Index is added to `@actions_to_remove` array
2. At end of frame in cleanup, marked indices are removed (in reverse order)
3. Named actions have `@[name]` cleared

### Three Phases

1. **Early** — Input handling, simulation prep (`early_action`, `_early_update`)
2. **Main** — Standard update logic (`action`, `_update`)
3. **Late** — Drawing, cleanup work (`late_action`, `_late_update`)

Each object can have custom `early_update`, `update`, `late_update` methods that run before actions.

---

## Death Semantics

1. `kill()` immediately sets `.dead = true` on self AND all descendants (synchronous propagation)
2. `kill(tag)` kills all objects matching the tag AND their descendants (children never outlive parents)
3. Actual removal from tree happens at end-of-frame in `cleanup()`
4. `all()` returns ALL descendants including dead ones (dead check is caller's responsibility)
5. Update loop skips dead objects via `return if @dead` in internal methods

---

## Horizontal Links

Horizontal links create death notification relationships between objects (typically siblings or unrelated objects).

### API

```yuescript
@\link target                -- kill self when target dies (default)
@\link target, => @\kill!    -- same as above, explicit
@\link target, => @homing = false  -- callback runs, object survives
```

### Behavior

1. When target dies (`target\kill!`), callbacks run **immediately** (before `target.dead = true` propagates)
2. If no callback provided, linker is killed (default behavior)
3. Callback receives `self` as argument — target is not passed (use closures if needed)
4. Links don't create named references — store references yourself if needed

### Storage

Links are stored bidirectionally for efficient lookup and cleanup:
- `@links` — Array of outgoing links `{target, callback}`
- `target.linked_from` — Array of incoming links `{source, callback}`

### Circular Links

Circular links (A links to B, B links to A) are safe:
1. `@dead = true` is set **before** processing `linked_from`
2. When notifying linked objects, dead sources are skipped
3. Both objects end up dead, no infinite recursion

### Cleanup

When an object is removed from the tree:
1. Remove self from each target's `linked_from` (outgoing links)
2. Remove self from each source's `links` (incoming links)

---

## Cleanup

The `cleanup` method handles three tasks:

1. **Remove marked actions** — For each object, remove actions that returned `true`
2. **Clean up links** — Remove dead objects from link arrays (both directions)
3. **Remove dead children** — Iterate in reverse (children-first) for proper destroy order

When removing dead children:
- Calls `child\destroy!` if child has a destroy method
- Clears `parent[child.name]` reference
- Clears `child[parent.name]` reference
- Clears `child.parent` reference

---

## Short Aliases

Single-letter aliases provide a compact API for common operations. A global `T` function creates objects, and single-letter methods handle initialization and tree operations.

### Reference

| Alias | Method | Purpose |
|-------|--------|---------|
| `T` | `object` | Create object (global function) |
| `Y` | `set` | Set properties from table |
| `U` | `build` | Run build function |
| `E` | `early_action` | Early phase action |
| `X` | `action` | Main phase action |
| `L` | `late_action` | Late phase action |
| `A` | `add` | Add child |
| `F` | `flow_to` | Add self to parent |
| `K` | `link` | Link to target |

### Usage Example

```yuescript
-- Create a player with properties and actions
p = T 'player'
p\Y {x: 100, y: 200, hp: 50}
p\U =>
  @speed = @x + @y
p\X 'move', (dt) =>
  @x += @vx * dt
p\L 'draw', (dt) =>
  game\circle @x, @y, 10, colors.white
p\A timer!
p\F arena
```

### Why Not Operators?

We initially explored custom operators (`^`, `/`, `+`, `>>`) but abandoned them because:

1. **Expression context only** — YueScript doesn't allow standalone operator expressions as statements
2. **Right-associativity** — Lua's `^` operator is right-associative, breaking chaining patterns
3. **Short methods are nearly as compact** — Single letters achieve similar brevity without language limitations

---

## Testing

Tests run from `Anchor/framework/` using `main.yue`. The test runner is an action on `an`:

```yuescript
an\action ->
  frame += 1
  if frame == 1
    test_complex_tree!
  elseif frame == 2
    test_bidirectional!
    test_tags!
  -- etc.
```

### Test Coverage (42 tests)

**Tree & Tags (1-8):**
1. Complex tree (4 levels deep)
2. Bidirectional named links
3. Tags and is() method
4. Kill middle of tree (branch)
5. After cleanup (branch removed)
6. Named child replacement
7. Kill by tag
8. After tag kill cleanup

**Actions (9-20):**
9. One-shot action (returns true)
10. After one-shot (removed)
11. Named action
12. Named action runs each frame
13. Replace named action
14. Replaced action runs
15. Early and late actions
16. Named early/late actions
17. Action execution order (early, main, late)
18. Named early/late run each frame
19. One-shot early/late actions
20. After one-shot early/late

**Horizontal Links (21-31):**
21. Link with callback (object survives)
22. After link callback (bullet still alive)
23. Link with callback that kills self
24. After callback kill (both removed)
25. Link without callback (default kill)
26. After default kill (both removed)
27. Circular links (no infinite loop)
28. After circular kill (both removed)
29. Link cleanup when linker dies
30. After linker cleanup (linked_from cleaned)
31. After linker cleanup verify

**Short Aliases (32-41):**
32. T alias (object creation)
33. Y alias (set properties)
34. U alias (build function)
35. A alias (add child)
36. E, X, L aliases (action phases)
37. After action aliases (waiting)
38. After action aliases (order check)
39. F alias (flow_to)
40. K alias (link)
41. After K alias (cleanup)

**Final:**
42. Final state

---

## Timer Module

The `timer` class is a child object that provides time-based callbacks, tweening, and state watching.

### Design Decisions

1. **Array-based storage** — Timers stored in `@entries` array (not hash table) for deterministic iteration order, enabling reproducible replays
2. **Optional named timers** — Name is always the second argument: `timer\after 1, 'name', callback`. Named timers automatically replace existing timers with the same name.
3. **Anonymous timer UIDs** — Anonymous timers get auto-generated unique IDs (`_timer_1`, `_timer_2`, etc.) to support `find` operations
4. **Cancelled flag** — Safe iteration when callbacks cancel other timers; cancelled entries are skipped and removed at end of update
5. **Multiplier support** — `set_multiplier` allows dynamic speed adjustment for slow-mo effects
6. **Edge triggers** — `watch` and `when` fire once when condition changes, not continuously while true

### API Reference

**Basic Timers:**
```yuescript
timer\after delay, [name], callback                    -- Fire once after delay
timer\every interval, [name], callback, [count], [after]  -- Fire repeatedly
timer\during duration, [name], callback, [after]       -- Fire every frame for duration
```

**Tweening:**
```yuescript
timer\tween duration, [name], target, properties, [easing], [after]
-- Example: timer\tween 1, obj, {x: 100, y: 50}, math.cubic_out
```

**State Watching:**
```yuescript
timer\watch field_name, callback              -- Fire when @[field] changes
timer\when condition_function, callback       -- Fire once when condition becomes true
timer\cooldown interval, name, condition, callback  -- Fire on interval while condition true
```

**Stepped Timers:**
```yuescript
timer\every_step start_delay, end_delay, count, callback  -- Varying intervals
timer\during_step start_delay, end_delay, duration, callback, [after]
```

**Control:**
```yuescript
timer\cancel name           -- Remove timer by name
timer\trigger name          -- Fire timer immediately (useful for testing or manual triggers)
timer\set_multiplier value  -- Speed up (>1) or slow down (<1) all timers
timer\get_time_left name    -- Query remaining time on a timer
```

### Entry Types

Each timer entry has a `mode` field:

| Mode | Fields | Behavior |
|------|--------|----------|
| `after` | delay, time, callback, after | Fires callback when time >= delay |
| `every` | interval, time, callback, count, after | Repeats; count limits iterations |
| `during` | duration, time, callback, after | Runs callback(dt, progress) each frame |
| `tween` | duration, time, target, initial, properties, easing, after | Interpolates properties |
| `watch` | field, previous, callback | Compares @[field] each frame |
| `when` | condition, callback | Checks condition each frame |
| `cooldown` | interval, time, condition, callback | Fires while condition true |
| `every_step` | delays (array), index, time, callback | Uses varying delays |
| `during_step` | delays (array), duration, time, callback, after | Varies frame timing |

### Easing Functions

The `math.yue` module provides easing functions for tweens:

- **Linear:** `math.linear`
- **Polynomial:** `quad`, `cubic`, `quart`, `quint` (each with `_in`, `_out`, `_in_out`, `_out_in`)
- **Trigonometric:** `sine` (all variants)
- **Exponential:** `expo` (all variants)
- **Circular:** `circ` (all variants)
- **Bounce:** `bounce` (all variants)
- **Back:** `back` (overshoots, all variants)
- **Elastic:** `elastic` (springy, all variants)

---

## Spring Module

The `spring` class is a child object that provides damped spring animations for juicy visual effects.

### Design Decisions

1. **Container pattern** — One spring object holds multiple named springs (like timer holds multiple timers)
2. **Default 'main' spring** — Every spring object starts with a 'main' spring at value 1 (useful for scale effects)
3. **Direct property access** — Springs accessible as `@spring.name.x` for clean usage
4. **Early update** — Springs update in early phase so values are ready for main/late phases
5. **Standard damped spring physics** — Uses equation `a = -k*(x - target) - d*v`

### API Reference

```yuescript
@\add spring!                        -- Add spring child (creates default 'main' at value 1)
@spring\add 'scale', 1, 200, 10      -- Add named spring: name, initial, stiffness, damping
@spring\pull 'main', 0.5             -- Apply impulse (adds to current value)
@spring\pull 'scale', 0.3, 200, 5    -- Pull with custom k/d
@spring\set_target 'main', 2         -- Change resting point (animates toward new value)
@spring\at_rest 'main'               -- Check if spring has settled
@spring.main.x                       -- Read current value
@spring.scale.x                      -- Read named spring value
```

### Spring Properties

Each spring entry has:
- `x` — Current value
- `target_x` — Resting point (value spring settles toward)
- `v` — Current velocity
- `k` — Stiffness (default 100, higher = faster oscillation)
- `d` — Damping (default 10, higher = less bouncy)

### Physics

The spring uses standard damped harmonic oscillator physics:
```
acceleration = -k * (x - target_x) - d * velocity
velocity += acceleration * dt
x += velocity * dt
```

For critical damping (no overshoot): `d = 2 * sqrt(k)`

### Usage Example

```yuescript
-- Add spring to an object
@\add spring!
@spring\add 'hit_scale', 1, 200, 10

-- On hit, pull the spring
@spring\pull 'hit_scale', 0.3

-- In draw, apply spring value to scale
layer\push @x, @y, 0, @spring.hit_scale.x, @spring.hit_scale.x
layer\rectangle -@w/2, -@h/2, @w, @h, @color
layer\pop!
```

---

## Layer Rendering Pipeline

The rendering pipeline uses explicit control via a global `draw()` function called by C after the update phase completes.

### Three-Phase Architecture

1. **Queue commands** (during update) — Draw calls queue commands to layers
2. **Render to FBOs** (in draw) — Process queued commands to layer framebuffers
3. **Composite to screen** (in draw) — Draw layer textures to screen in desired order

### Layer Methods

| Method | Purpose |
|--------|---------|
| `layer\render!` | Process queued commands to FBO (clears first) |
| `layer\clear!` | Clear FBO contents to transparent black |
| `layer\draw_from source, shader` | Copy source layer's texture to this layer's FBO |
| `layer\draw x, y` | Queue layer for compositing to screen |

### Shader Uniforms

For shaders used with `draw_from`, set uniforms immediately before the call:

```yuescript
shader_set_vec2 an.shaders.outline, "u_pixel_size", 1/W, 1/H
outline\draw_from game, an.shaders.outline
```

### Example draw() Function

```yuescript
draw = ->
  -- 1. Render source layers (process queued commands to FBOs)
  bg\render!
  game\render!
  ui\render!

  -- 2. Create derived layers (copy from game through shaders)
  shadow\clear!
  shadow\draw_from game, an.shaders.shadow

  outline\clear!
  shader_set_vec2 an.shaders.outline, "u_pixel_size", 1/W, 1/H
  outline\draw_from game, an.shaders.outline

  -- 3. Composite to screen (visual back-to-front order)
  bg\draw!
  shadow\draw 4, 4
  outline\draw!
  game\draw!
  ui\draw!
```

### Alpha Blending Fix

When drawing to FBOs with standard `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`, the alpha channel gets incorrectly squared:

- **Problem:** FBO.alpha = src.alpha × src.alpha (e.g., 0.125 × 0.125 = 0.0156)
- **Expected:** FBO.alpha = src.alpha (e.g., 0.125)

The fix uses `glBlendFuncSeparate` to handle RGB and alpha channels differently:

```c
// RGB: standard alpha blend
// Alpha: preserve source alpha correctly
glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,  // RGB
                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);       // Alpha
```

This ensures semi-transparent colors composite correctly when drawn through multiple FBOs.

---

## YueScript Idioms

- Use `list[] = item` instead of `table.insert list, item`
- Use `global *` at top of file to make all definitions global
- Use `for item in *list` for array iteration (values only)
- Use `for i, item in ipairs list` for index-value pairs
- Use `\method!` for method calls (compiles to `obj:method()`)
- Use `@\method!` for self method calls in class methods
- Use `false` instead of `nil` in arrays to preserve iteration
- Use explicit `local` inside functions when variable name matches a global (with `global *`, assignments to existing globals update them instead of creating locals)

---

## Decisions Made

1. **No submodules** — Too much friction updating during active development
2. **No symlinks/junctions** — Complicated, not flexible
3. **Copy-based framework** — Each game has its own copy of the framework
4. **Master framework in Anchor/framework/anchor/** — New games copy from here, or from previous game
5. **Single update entry point** — C only calls `update(dt)`, Lua handles phases internally
6. **Root object named `an`** — May change later, works for now
7. **Iterative DFS** — Easier to reason about than recursive
8. **Tags only in all(tag)** — Names accessed directly, not via query
9. **Actions as plain functions** — Not objects, just stored in parallel arrays
10. **`false` for anonymous action names** — Preserves array iteration
11. **`all()` returns dead objects** — Dead check is caller's responsibility
12. **Children-first destroy order** — Iterate objects in reverse for cleanup
13. **Link callbacks run immediately** — During `kill()`, not deferred to cleanup
14. **Default link behavior is kill** — No callback means linker dies when target dies
15. **Link callback receives only self** — Target not passed; use closures if needed
16. **Links don't create named refs** — Unlike `add()`, links are just death notifications
17. **No custom operators** — YueScript limitations make operators impractical; short methods used instead
18. **Single-letter aliases** — T, Y, U, E, X, L, A, F, K provide compact API without language hacks
19. **Timer name as second argument** — `timer\after 1, 'name', callback` reads like English ("after 1 second, named X, do Y")
20. **Array-based timer storage** — Deterministic iteration order for reproducible replays
21. **`trigger` for immediate fire** — Rejected `_now` suffix variants; separate method is clearer
22. **`watch` and `when` as edge triggers** — Fire once when state changes, not continuously while condition holds
23. **Event normalization** — `collision_begin_events 'a', 'b'` guarantees `event.a` has tag 'a' and `event.b` has tag 'b'; Box2D returns bodies in arbitrary order
24. **Collider IDs via integers** — Use `physics_set_user_data` with incrementing integers; Lua userdata comparison fails because new objects are created each time
25. **Explicit `local` with `global *`** — When using `global *`, explicitly declare `local` for variables inside functions that share names with top-level globals
26. **Explicit layer rendering** — C calls a global `draw()` function instead of auto-rendering layers; gives full control over render order, derived layers (shadow/outline), and compositing
27. **glBlendFuncSeparate for FBO alpha** — Standard `glBlendFunc` incorrectly squares alpha when drawing to FBOs; use separate blend for RGB vs alpha channels to preserve correct alpha values

---

## What's Implemented

| Feature | Status |
|---------|--------|
| Project structure (copy-based) | Done |
| YueScript compilation | Done |
| `object` class (name, parent, children, dead, tags) | Done |
| `add(child)` with bidirectional named links | Done |
| Named child replacement | Done |
| `all(tag)` iterative DFS collection | Done |
| `kill(tag)` with propagation to descendants | Done |
| `tag(...)` and `is(name_or_tag)` | Done |
| Action system (early/main/late, named/anonymous) | Done |
| Three-phase update loop | Done |
| End-of-frame cleanup | Done |
| Horizontal links (`link(target, callback)`) | Done |
| Initialization methods (`set`, `build`, `flow_to`) | Done |
| Short aliases (T, Y, U, E, X, L, A, F, K) | Done |
| Documentation comments in object.yue | Done |
| Test suite (42 tests) | Done |
| `layer` class (rectangle, circle, image, text, push/pop, draw, render, clear, draw_from) | Done |
| Shader resource registration (`an\shader`, `an\shader_string`) | Done |
| Explicit layer rendering pipeline (global `draw()` function) | Done |
| `image` class (width, height, handle wrapper) | Done |
| `font` class (text_width, char_width, glyph_metrics) | Done |
| Resource registration on `an` (layer, image, font) | Done |
| `timer` class (after, every, during, tween, watch, when, cooldown, every_step, during_step, cancel, trigger, set_multiplier, get_time_left) | Done |
| `math` module (lerp, easing functions: linear, sine, quad, cubic, quart, quint, expo, circ, bounce, back, elastic) | Done |
| Physics world on `an` (physics_init, physics_set_gravity, physics_tag, physics_collision, physics_sensor, physics_hit) | Done |
| Collision query methods on `an` (collision_begin_events, collision_end_events, sensor_begin_events, sensor_end_events, hit_events) | Done |
| `collider` class (body creation, shapes, position/velocity, forces, properties, destroy) | Done |
| `collider` sensor shape support via opts table `{sensor: true}` | Done |
| Event normalization (a/b match query tag order) | Done |
| Spatial queries on `an` (query_point, query_circle, query_aabb, query_box, query_capsule, query_polygon, raycast, raycast_all) | Done |
| `spring` class (add, pull, set_target, at_rest, early_update) | Done |
| `color` class (RGB/HSL, operators, clone, invert, mix) | Done |
| `array` module (predicates, aggregates, search, modification, random) | Done |
| Input wrappers on `an` (bind, is_pressed, is_down, key_*, mouse_*, gamepad_*) | Done |
| Engine state on `an` (frame, step, time, width, height, scale, fullscreen, fps, draw_calls, platform) | Done |

---

## Module Architecture

The remaining framework modules fall into four categories, each requiring a different implementation strategy. **Not everything should be an object in the tree.** Each module should be self-contained and not leak into other systems.

### Resource Manager (`an`)

The root object `an` manages all loaded resources. Resources are created through methods on `an` and stored in registries for later access. The C engine handles the actual resource data; Lua tracks handles and provides convenient access by name.

| Resource | Load Method | Storage | Usage |
|----------|-------------|---------|-------|
| **sounds** | `an\sound 'name', 'path'` | `an.sounds.name` | `an.sounds.jump\play 1, 1.05` |
| **music** | `an\music 'name', 'path'` | `an.music.name` | `an\music_play 'bgm'` |
| **images** | `an\image 'name', 'path'` | `an.images.name` | `game\draw_texture an.images.player, x, y` |
| **layers** | `an\layer 'name'` | `an.layers.name` | `game\circle x, y, r, color` |
| **fonts** | `an\font 'name', 'path', size` | `an.fonts.name` | `game\draw_text 'hello', 'main', x, y` |
| **shaders** | `an\shader 'name', 'path'` | `an.shaders.name` | `shadow\draw_from game, an.shaders.shadow` |

**Why resources on `an`:**
- Resources need handles tracked somewhere (C returns handles, Lua stores them)
- Centralized registry allows access by name (`an.sounds.jump` instead of raw handles)
- Layers are typically assigned to globals for convenience: `game = an\layer 'game'`

**Physics** is also configured through `an`:
- `an\physics_init!` — Initialize the Box2D world
- `an\physics_set_gravity gx, gy` — Configure gravity
- The physics world itself lives in C; Lua configures and queries it

### Child Objects (Tree Lifecycle)

These are proper tree objects added as children to other objects. Their lifecycle is tied to their parent — when the parent dies, they die automatically. They benefit from the tree's automatic cleanup.

| Object | Description | Usage |
|--------|-------------|-------|
| **random** | Seeded RNG instance | `@\add random seed` then `@random\float 0, 1` |
| **timer** | Delays, repeating callbacks, tweens | `@\add timer!` then `@timer\after 2, -> ...` |
| **spring** | Damped spring animation | `@\add spring!` then `@spring\pull 'main', 0.5` |
| **collider** | Box2D physics body | `@\add collider 'enemy', 'dynamic', 'circle', 16` |
| **camera** | Viewport with position, zoom, rotation | `an\add camera!` then `an.camera\follow player` |
| **shake** | Shake effect (child of camera) | `an.camera\add shake!` then `an.camera.shake\shake 10, 0.5` |

**Child object design principles:**
- Extend `object` class (or are created by factory functions that return configured objects)
- Are added via `parent\add child`
- Die automatically when parent dies
- Use `destroy()` method for cleanup when removed from tree (e.g., destroy physics body in C)
- Are self-contained — internal state doesn't leak into tree semantics

**random** as a child object enables:
- Seeded RNG for deterministic replays
- Multiple RNGs (gameplay vs cosmetic effects)
- Encapsulates seed: `@random\get_seed!` for replay storage

**camera** is typically added to `an`, but the object design supports multiple cameras if needed.

**timer** internal design:
- Stores timers in internal table keyed by name
- Named timers automatically replace previous timers with same name
- No `:kill()` exposed on timer entries — just `@timer\cancel 'name'`
- Timer entries don't leak into tree semantics

### Value Objects (Stateful, Not in Tree)

These are objects with state and methods, but they're not part of the tree hierarchy. You don't add them as children — you create them and use them directly.

| Object | Description | Usage |
|--------|-------------|-------|
| **color** | Mutable color with RGB/HSL | `red = color 255, 0, 0` then `layer\circle x, y, 10, red!` |

**color** provides:
- RGB and HSL properties (r, g, b, a, h, s, l)
- Automatic RGB/HSL synchronization
- Arithmetic operators (*, /, +, -) that mutate in place
- Methods: `clone`, `invert`, `mix`
- Packed value via `color!` for C functions

### Global Systems (Not Objects)

Some systems are implemented as method wrappers on `an` rather than child objects:

| System | Description | Usage |
|--------|-------------|-------|
| **input** | Input binding and queries | `an\bind 'jump', 'key:space'` then `an\is_pressed 'jump'` |

**input** as global wrappers:
- Simpler than child object pattern for typical single-player games
- All C input functions exposed as `an\*` methods
- Supports action binding, raw input, gamepad, capture mode

### Pure Utilities (Stateless Global Functions)

These are just functions. No object wrapping, no state, no tree integration.

| Module | Description |
|--------|-------------|
| **math** | `math.lerp`, `math.angle`, `math.distance`, easing functions |
| **array** | Array manipulation functions |

---

## Physics Event System Testing

This session focused on testing and fixing the physics event system with a visual demo.

### C Engine Modification

Added contact point and normal data to `collision_begin_events`. Previously, collision events only had body/shape handles. Now they include:
- `point_x`, `point_y` — Contact point location
- `normal_x`, `normal_y` — Contact normal direction

Implementation in `anchor.c`:
- Called `b2Shape_GetContactData` on the two colliding shapes to get the `b2Manifold`
- Extracted the first `b2ManifoldPoint` from the manifold
- Web searched "Box2D 3.1 contact manifold" and fetched Box2D documentation to find the correct API

### Fixes Applied

**Event Normalization:**
- All event query functions (`collision_begin_events`, `collision_end_events`, `sensor_begin_events`, `sensor_end_events`, `hit_events`) now normalize the returned `a` and `b` objects to match the query order
- When you call `collision_begin_events 'ball', 'wall'`, `event.a` is guaranteed to be the ball and `event.b` the wall
- Previously, Box2D could return bodies in either order, causing bugs when applying effects to the wrong object

**Collider ID Registration:**
- Fixed body lookup using unique integer IDs instead of body handles
- Lua creates new userdata objects for `b2BodyId` each time, so direct comparison fails
- Debug output revealed the problem: event body addresses (`000001D9CA433B68`) didn't match any registered collider addresses (`000001D9C7981388`, etc.)
- Now uses `physics_set_user_data` / `physics_get_user_data` with incrementing integer IDs
- Collider constructor assigns `@id = collider_next_id` and registers in `an.colliders[@id]`

**Sensor Shape Support:**
- Added opts table support to collider class: `collider 'tag', 'static', 'box', w, h, {sensor: true}`
- Last argument can be an options table with `sensor`, `offset_x`, `offset_y`, `angle`

### Visual Test (main.yue)

The test demonstrates all physics event types:

1. **Collision Events (impulse_block):**
   - Blue block at bottom-left applies rightward impulse to balls on first contact
   - Uses `collision_begin_events 'ball', 'impulse_block'`

2. **Sensor Events (slowing_zone):**
   - Blue transparent zone slows balls to 10% speed and reduces gravity to 10%
   - On exit, restores original speed (maintaining current direction) and normal gravity
   - Uses `sensor_begin_events` and `sensor_end_events`

3. **Hit Events (wall flash):**
   - Balls flash white for 0.15 seconds when hitting walls above approach_speed threshold (300)
   - Uses `hit_events 'ball', 'wall'` with `event.approach_speed`
   - Integrates with timer module for flash duration

### YueScript Scoping Discovery

Discovered that `global *` at the top of a file makes ALL variable assignments global, including those inside nested functions. This caused a bug where `ball = event.a` inside a for loop overwrote the global `ball` class.

**Solution:** Use explicit `local` for variables inside functions that share names with globals:
```yuescript
for event in *an\collision_begin_events 'ball', 'wall'
  local ball = event.a  -- explicit local to avoid shadowing global class
  ball.collider\set_velocity 0, 0
```

This behavior is documented in the [YueScript source code](https://github.com/pigpigyyy/Yuescript) - variable assignments check if the name exists in outer scopes and update the existing variable rather than creating a new local.

---

## Camera Module

The `camera` class is a child object that provides viewport control with position, zoom, rotation, and effects.

### Design Decisions

1. **Child object pattern** — Camera is added to `an` and layers reference it via `layer.camera`
2. **Effect composition** — Camera collects transform offsets from child objects implementing `get_transform()`
3. **Coordinate conversion** — `to_world` and `to_screen` methods for mouse picking and UI positioning
4. **Follow with lead** — Camera can follow a target with optional velocity-based lead
5. **Bounds clamping** — Optional camera bounds to constrain movement

### API Reference

```yuescript
an\add camera!                           -- Add camera (uses global W, H)
an.camera.x, an.camera.y = 100, 200      -- Set position
an.camera.zoom = 2                       -- Set zoom
an.camera.rotation = math.pi / 4         -- Set rotation

an.camera\follow player                  -- Follow target
an.camera\follow player, 0.9, 0.5        -- Follow with lerp (90% distance in 0.5s)
an.camera\follow player, 0.9, 0.5, 0.1   -- Follow with lead (look ahead based on velocity)

an.camera\set_bounds min_x, max_x, min_y, max_y  -- Constrain camera
an.camera\set_bounds!                    -- Remove bounds

world_x, world_y = an.camera\to_world screen_x, screen_y  -- Screen to world
screen_x, screen_y = an.camera\to_screen world_x, world_y -- World to screen
an.camera.mouse.x, an.camera.mouse.y     -- Mouse in world coordinates (updated each frame)
```

### Layer Integration

Layers reference `an.camera` by default. Set `layer.camera = nil` for screen-space UI:

```yuescript
ui = an\layer 'ui'
ui.camera = nil  -- UI stays in screen space
```

### Effect System

Child objects of camera can implement `get_transform()` to contribute effects:

```yuescript
get_transform: =>
  {x: offset_x, y: offset_y, rotation: rotation_offset, zoom: zoom_offset}
```

The camera sums all child effects in `get_effects()` and applies them during `attach()`.

---

## Math Module Additions

Added framerate-independent interpolation and angle utilities:

```yuescript
-- Framerate-independent lerp
-- "Cover p% of the distance in t seconds"
math.lerp_dt(p, t, dt, source, destination)
-- Example: math.lerp_dt(0.9, 0.5, dt, @x, target_x)  -- 90% of distance in 0.5s

-- Angle interpolation (handles wraparound)
math.lerp_angle(t, source, destination)
math.lerp_angle_dt(p, t, dt, source, destination)

-- Loop value within range (like modulo but always positive)
math.loop(t, length)  -- Returns value in [0, length)
```

---

## Spring Module Updates

Changed spring API from physics constants (k, d) to intuitive parameters (frequency, bounce):

### Old API (Internal)
```yuescript
@spring\add 'scale', 1, 200, 10  -- value, stiffness k, damping d
```

### New API
```yuescript
@spring\add 'scale', 1, 5, 0.5   -- value, frequency (Hz), bounce (0-1)
@spring\pull 'main', 0.5, 8, 0.7 -- force, optional new frequency/bounce
```

**Parameters:**
- `frequency` — Oscillations per second (default 5)
- `bounce` — Bounciness 0-1 (default 0.5, where 0=no overshoot, 1=infinite oscillation)

**Internal conversion:**
```
k = (2π × frequency)²
d = 4π × (1 - bounce) × frequency
```

---

## Shake Module

The `shake` class is a child object added to camera that provides various screen shake effects.

### Design Decisions

1. **Camera child** — Shake is added to camera and implements `get_transform()`
2. **Multiple instance types** — Each shake type (trauma, shake, sine, square) supports multiple simultaneous instances
3. **Duration per call** — Trauma and other effects specify duration per call, allowing different decay rates
4. **Spring reuse** — Push shake reuses the spring module for natural oscillation

### API Reference

**Trauma (Perlin noise, accumulating):**
```yuescript
shake\trauma 0.5, 0.3           -- amount, duration (instances sum together)
shake\trauma 1, 1               -- full trauma over 1 second
shake\set_trauma_parameters {x: 24, y: 24, rotation: 0.2, zoom: 0.2}
```

**Push (spring-based directional):**
```yuescript
shake\push angle, amount                    -- directional impulse
shake\push angle, amount, frequency, bounce -- with custom spring params
shake\push 0, 20                            -- rightward push
shake\push math.pi, 15, 8, 0.7              -- leftward with custom spring
```

**Shake (random jitter):**
```yuescript
shake\shake amplitude, duration             -- random displacement
shake\shake amplitude, duration, frequency  -- with jitter rate (changes/sec)
shake\shake 15, 0.5                         -- 15 pixels, 0.5 seconds
shake\shake 20, 0.5, 30                     -- slower jitter (30 Hz)
```

**Sine (smooth oscillation):**
```yuescript
shake\sine angle, amplitude, frequency, duration
shake\sine 0, 15, 8, 0.5        -- horizontal, 15px, 8 Hz, 0.5s
```

**Square (sharp oscillation):**
```yuescript
shake\square angle, amplitude, frequency, duration
shake\square 0, 15, 8, 0.5      -- horizontal, 15px, 8 Hz, 0.5s
```

**Handcam (continuous subtle motion):**
```yuescript
shake\handcam true                          -- enable with defaults
shake\handcam true, {x: 5, y: 5, rotation: 0.02, zoom: 0.02}, 0.5  -- custom
shake\handcam false                         -- disable
```

### Shake Types Summary

| Method | Description | Key Feature |
|--------|-------------|-------------|
| `trauma` | Perlin noise shake | Accumulates, quadratic intensity |
| `push` | Spring-based directional | Natural oscillation and settle |
| `shake` | Random jitter | Chaotic, jittery feel |
| `sine` | Sinusoidal oscillation | Smooth, rhythmic |
| `square` | Square wave oscillation | Sharp, snappy |
| `handcam` | Continuous subtle motion | Always-on ambient effect |

---

## Random Module

The `random` class is a child object that provides seeded random number generation.

### Design Decisions

1. **Child object pattern** — Random is added to objects and dies with parent
2. **Wraps C functions** — All randomness comes from C engine's RNG
3. **Default on `an`** — A global random is created on `an` for convenience
4. **Seeded for replays** — Seed can be stored and restored for deterministic replays

### API Reference

```yuescript
@\add random!           -- unseeded (uses os.time)
@\add random 12345      -- seeded for deterministic replays

@random\float!          -- 0 to 1
@random\float 10        -- 0 to 10
@random\float 5, 10     -- 5 to 10

@random\int 10          -- 1 to 10
@random\int 5, 10       -- 5 to 10

@random\angle!          -- 0 to 2π
@random\sign!           -- -1 or 1 (50% each)
@random\sign 75         -- 75% chance of 1
@random\bool!           -- true/false (50% each)
@random\bool 10         -- 10% chance true

@random\normal!             -- mean 0, stddev 1
@random\normal 100, 15      -- mean 100, stddev 15

@random\choice enemies              -- random element from array
@random\choices loot_table, 3       -- 3 unique random elements
@random\weighted {1, 2, 7}          -- weighted index (10%, 20%, 70%)

@random\get_seed!       -- get current seed
@random\set_seed 12345  -- reset with new seed
```

---

## Sound and Music System

Sounds and music are registered on `an` and played through methods.

### Sound API

```yuescript
-- Registration
an\sound 'jump', 'assets/jump.wav'
an\sound 'hit', 'assets/hit.ogg'

-- Playback
an\sound_play 'jump'                    -- play at default volume/pitch
an\sound_play 'hit', 0.5, 1.2           -- volume 0.5, pitch 1.2

-- Master volume
an\sound_set_volume 0.8
```

### Music API

```yuescript
-- Registration
an\music 'menu', 'assets/menu.ogg'
an\music 'battle', 'assets/battle.ogg'

-- Playback
an\music_play 'menu'                    -- play on channel 0
an\music_play 'menu', true              -- loop
an\music_play 'menu', true, 1           -- play on channel 1

-- Control
an\music_stop!                          -- stop all channels
an\music_stop 0                         -- stop channel 0 only
an\music_set_volume 0.5                 -- master music volume
an\music_set_volume 0.5, 0              -- channel 0 volume

-- Crossfade (two-channel system)
an\music_crossfade 'battle', 2          -- crossfade to battle over 2 seconds
```

### Playlist API

```yuescript
-- Setup
an\playlist_set {'menu', 'battle', 'boss'}

-- Control
an\playlist_play!                       -- start playlist
an\playlist_stop!                       -- stop playlist
an\playlist_next!                       -- skip to next track
an\playlist_prev!                       -- go to previous track

-- Options
an\playlist_shuffle true                -- enable shuffle
an\playlist_shuffle false               -- disable shuffle
an\playlist_set_crossfade 2             -- 2 second crossfade between tracks
an\playlist_set_crossfade 0             -- instant switch (default)

-- Query
an\playlist_current_track!              -- get current track name
```

### Two-Channel Music System

The C engine supports two music channels for crossfade effects:
- Channel 0 and 1 can play simultaneously
- `music_crossfade` fades out one channel while fading in the other
- Playlist tracks which channel is active and swaps on crossfade completion
- Same track on multiple channels shares one `ma_sound`, so stopping one channel checks if another needs it

---

## Input System

Input is implemented as thin wrappers on `an` that call the C input functions directly. This is simpler than the child object pattern originally planned, since most games only need one input context.

### Action Binding

```yuescript
an\bind 'jump', 'key:space'
an\bind 'jump', 'button:a'           -- multiple bindings per action
an\bind 'fire', 'mouse:1'
an\bind 'move_left', 'axis:leftx-'

an\unbind 'jump', 'key:space'        -- remove specific binding
an\unbind_all 'jump'                 -- remove all bindings
an\bind_all!                         -- bind all common keys/buttons
```

### Action Queries

```yuescript
an\is_pressed 'jump'                 -- true on frame pressed
an\is_down 'jump'                    -- true while held
an\is_released 'jump'                -- true on frame released
an\get_axis 'left', 'right'          -- -1, 0, or 1
an\get_vector 'left', 'right', 'up', 'down'  -- normalized x, y
```

### Advanced Bindings

```yuescript
an\bind_chord 'super_jump', {'shift', 'jump'}           -- all held at once
an\bind_sequence 'dash', {'right', 0.3, 'right'}        -- timed sequence
an\bind_hold 'charge', 1.0, 'attack'                    -- hold for duration
an\get_hold_duration 'charge'                           -- query hold time
```

### Raw Input

```yuescript
an\key_is_down 'space'
an\key_is_pressed 'escape'
an\key_is_released 'space'

an\mouse_position!                   -- x, y
an\mouse_delta!                      -- dx, dy
an\mouse_wheel!                      -- wx, wy
an\mouse_is_down 1                   -- 1=left, 2=middle, 3=right
an\mouse_is_pressed 1
an\mouse_is_released 1
an\mouse_set_visible false
an\mouse_set_grabbed true

an\gamepad_is_connected!
an\gamepad_get_axis 'leftx'          -- -1 to 1
an\get_last_input_type!              -- 'keyboard', 'mouse', or 'gamepad'
```

### Input Capture (Rebinding)

```yuescript
an\start_capture!
captured = an\get_captured!          -- returns control string or nil
an\stop_capture!
an\set_deadzone 0.2
```

---

## Color Module

The `color` class is a mutable color object with RGB and HSL access.

### Design Decisions

1. **Mutable in place** — All operations modify the color, no new objects created
2. **HSL synchronization** — Changing RGB updates HSL and vice versa
3. **Packed value via call** — Use `color!` to get packed RGBA integer for C functions
4. **Operators return self** — Allows chaining: `red = red * 0.5 + 20`

### API Reference

```yuescript
-- Creation
red = color 255, 0, 0                -- RGB, alpha defaults to 255
transparent = color 255, 0, 0, 128   -- RGBA

-- Properties (readable and writable)
red.r, red.g, red.b, red.a           -- RGB 0-255, alpha 0-255
red.h, red.s, red.l                  -- hue 0-360, saturation 0-1, lightness 0-1

-- Packed value for C functions
layer\circle x, y, r, red!

-- Operators (mutate in place, return self)
red = red * 0.5                      -- darken by half
red = red / 2                        -- same effect
red = red + 50                       -- brighten all channels
red = red - 20                       -- darken all channels
red = red * other_color              -- component-wise multiply
red = red + other_color              -- component-wise add

-- Methods
copy = red\clone!                    -- independent copy
red\invert!                          -- flip RGB (255 - value)
red\mix blue, 0.5                    -- lerp toward blue (mutates red)
```

### HSL Examples

```yuescript
red = color 255, 0, 0
red.l = 0.75                         -- lighten (becomes pink)
red.l = 0.25                         -- darken (becomes dark red)
red.h = 120                          -- shift hue to green
red.s = 0                            -- desaturate to gray
```

---

## Array Module

The `array` module provides utility functions for array manipulation. Functions that can be easily expressed with YueScript comprehensions are omitted.

### Design Decisions

1. **In-place modification** — Most operations modify the array, use comprehension to copy first
2. **Optional RNG parameter** — Random functions default to `an.random`
3. **YueScript idioms preferred** — Use `[f(v) for v in *t]` for map, `[v for v in *t when cond]` for filter

### Omitted Functions (Use YueScript)

```yuescript
-- map: use comprehension
result = [f(v) for v in *t]

-- filter/select: use comprehension with when
result = [v for v in *t when v > 5]

-- copy: use comprehension
copy = [v for v in *t]

-- indexes: use comprehension
indices = [i for i, v in ipairs t when v == target]
```

### Predicate Functions

```yuescript
array.all t, (v) -> v > 0            -- true if all pass
array.any t, (v) -> v > 0            -- true if any pass
array.has t, value                   -- true if value exists
array.has t, (v) -> v > 5            -- true if predicate matches
```

### Aggregate Functions

```yuescript
array.sum t                          -- sum of values
array.sum t, (v) -> v.score          -- sum with accessor
array.average t                      -- average of values
array.max t                          -- maximum value
array.max t, (v) -> v.priority       -- max by accessor
array.count t                        -- length (#t)
array.count t, value                 -- count occurrences
array.count t, (v) -> v > 5          -- count matching predicate
```

### Search Functions

```yuescript
array.index t, value                 -- first index of value
array.index t, (v) -> v > 5          -- first index matching predicate
array.get t, 1                       -- element at index
array.get t, -1                      -- last element (negative index)
array.get t, 1, 3                    -- range {t[1], t[2], t[3]}
array.get_circular_buffer_index t, i -- wrap index for circular buffer
```

### Modification Functions

```yuescript
array.delete t, value                -- remove all instances, returns count
array.remove t, i                    -- remove at index (table.remove)
array.reverse t                      -- reverse in place
array.reverse t, 1, 3                -- reverse range
array.rotate t, 1                    -- shift right: {4,1,2,3}
array.rotate t, -1                   -- shift left: {2,3,4,1}
array.shuffle t                      -- randomize order
array.shuffle t, rng                 -- with custom RNG
```

### Random Functions

```yuescript
array.random t                       -- random element
array.random t, 3                    -- 3 unique random elements
array.remove_random t                -- remove and return random element
array.remove_random t, 3             -- remove and return 3 random elements
```

### Utility Functions

```yuescript
array.flatten t                      -- flatten nested arrays
array.flatten t, 1                   -- flatten one level only
array.join t                         -- concatenate: "123"
array.join t, ", "                   -- with separator: "1, 2, 3"
array.print t                        -- debug print, returns string
```

### Table Utilities

```yuescript
table.copy t                         -- deep copy
table.tostring t                     -- string representation
```

---

## Phase 10 Complete

All Phase 10 modules are implemented:

| Category | Items | Status |
|----------|-------|--------|
| **Pure utilities** | math (lerp, easing, lerp_dt, lerp_angle, loop) | Done |
| **Pure utilities** | array (all, any, sum, max, has, index, reverse, shuffle, etc.) | Done |
| **Value objects** | color (RGB/HSL, operators, clone, invert, mix) | Done |
| **Resource manager** | sounds, music on `an` | Done |
| **Child objects** | timer | Done |
| **Child objects** | collider | Done |
| **Child objects** | spring (with frequency/bounce API) | Done |
| **Child objects** | camera (follow, bounds, lead, coordinate conversion) | Done |
| **Child objects** | shake (trauma, push, shake, sine, square, handcam) | Done |
| **Child objects** | random | Done |
| **Global system** | input (wrappers on `an` for all C input functions) | Done |
| **Physics** | Spatial queries on `an` (query_point, query_circle, raycast, etc.) | Done |

---

## Phase 11: Engine State Exposure

Exposed engine state values to Lua via properties on `an`.

### Static Values (set once at init)

| Property | Description |
|----------|-------------|
| `an.width` | Game resolution width (e.g., 480) |
| `an.height` | Game resolution height (e.g., 270) |
| `an.dt` | Physics delta time (constant 1/120) |
| `an.platform` | `'windows'` or `'web'` |

### Dynamic Values (updated each frame)

| Property | Description |
|----------|-------------|
| `an.frame` | Render frame count (60Hz) |
| `an.step` | Physics step count (120Hz) |
| `an.time` | Total elapsed game time (seconds) |
| `an.window_width` | Current window width in pixels |
| `an.window_height` | Current window height in pixels |
| `an.scale` | Current integer scaling factor |
| `an.fullscreen` | Whether in fullscreen mode |
| `an.fps` | Frames per second |
| `an.draw_calls` | Batch draw calls this frame |

### Implementation

**C Engine:**
- Added `draw_calls` counter in `batch_flush()`
- Added `fps` calculation from dt_history average
- Added 12 getter functions registered to Lua

**Framework (init.yue):**
- Static values set once when `an` is created
- Dynamic values updated at start of `update()` before processing objects

---

## Phase 11: Asset Packaging

Single executable distribution for desktop builds using the LÖVE-style zip-append approach.

### How It Works

1. **Packaging:** A script creates a zip of game assets and appends it to the engine executable
2. **Detection:** On startup, the engine checks if a zip is appended to itself
3. **Loading:** All asset loading functions check the embedded zip first, fall back to disk

### Usage

```bash
# Package a game
./scripts/package.bat framework          # Creates release/framework.exe
./scripts/package.bat ../my-game my-game # Creates release/my-game.exe

# Or with bash
./scripts/package.sh ../my-game my-game
```

The output executable is self-contained and can be distributed without additional files.

### Implementation

**C Engine Changes:**
- Added miniz.h (single-header zip library)
- `zip_init()` — Detects and opens zip appended to executable
- `zip_read_file()` — Reads file from zip or falls back to disk
- `zip_shutdown()` — Cleanup on exit

**Modified Asset Loading:**
- Lua loading: Uses `zip_read_file()` + `luaL_loadbuffer()`
- Texture loading: Uses `zip_read_file()` + `stbi_load_from_memory()`
- Font loading: Uses `zip_read_file()` + `FT_New_Memory_Face()`
- Sound loading: Uses `zip_read_file()` + `ma_decoder_init_memory()`
- Music loading: Uses `zip_read_file()` + `ma_decoder_init_memory()`
- Shader loading: Uses `zip_read_file()` (read_file_to_string)

**Web Builds:**
- Unaffected — Emscripten uses `--preload-file` for asset bundling
- `zip_read_file()` on Emscripten falls back to regular file I/O

### Scripts

- `scripts/package.bat` — Windows batch script
- `scripts/package.sh` — Bash script (MSYS2/WSL/Linux/Mac)

Both create a zip of the game folder (excluding .yue files, .git, etc.) and concatenate it to the engine exe.
