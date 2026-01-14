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
│   │   └── font.yue
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

## YueScript Idioms

- Use `list[] = item` instead of `table.insert list, item`
- Use `global *` at top of file to make all definitions global
- Use `for item in *list` for array iteration (values only)
- Use `for i, item in ipairs list` for index-value pairs
- Use `\method!` for method calls (compiles to `obj:method()`)
- Use `@\method!` for self method calls in class methods
- Use `false` instead of `nil` in arrays to preserve iteration

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
| `layer` class (rectangle, circle, image, text, push/pop, draw) | Done |
| `image` class (width, height, handle wrapper) | Done |
| `font` class (text_width, char_width, glyph_metrics) | Done |
| Resource registration on `an` (layer, image, font) | Done |

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
| **input** | Input bindings context | `@\add input!` then `@input\is_pressed 'jump'` |
| **random** | Seeded RNG instance | `@\add random seed` then `@random\float 0, 1` |
| **timer** | Delays, repeating callbacks, tweens | `@\add timer!` then `@timer\after 2, -> ...` |
| **spring** | Damped spring animation | `@\add spring 1, 200, 10` then `@spring\pull 0.5` |
| **collider** | Box2D physics body | `@\add collider 'enemy', 'dynamic', 'circle', 16` |
| **camera** | Viewport with position, zoom, rotation | `an\add camera!` then `an.camera\follow player` |
| **animation** | Sprite animation | `@\add animation 'walk', 0.1` |
| **shake** | Shake effect | `@\add shake!` then `@shake\shake 10, 0.5` |

**Child object design principles:**
- Extend `object` class (or are created by factory functions that return configured objects)
- Are added via `parent\add child`
- Die automatically when parent dies
- Use `destroy()` method for cleanup when removed from tree (e.g., destroy physics body in C)
- Are self-contained — internal state doesn't leak into tree semantics

**input** as a child object enables:
- Multiple input contexts (Player 1 keyboard, Player 2 gamepad)
- Per-object bindings: `@input\bind 'jump', 'key:space'`
- Queries raw input from C, manages bindings in Lua

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
| **color** | Color with variations and operations | `red = color 1, 0, 0` then `game\circle x, y, 10, red[0]` |

**color** provides:
- Base color storage (RGBA)
- Indexed variations: `red[0]` (base), `red[3]` (lighter), `red[-2]` (darker)
- Alpha variations: `red.alpha[-3]` (semi-transparent)
- Operations: `red\blend blue, 0.5`, `red\darken 0.2`

### Pure Utilities (Stateless Global Functions)

These are just functions. No object wrapping, no state, no tree integration.

| Module | Description |
|--------|-------------|
| **math** | `math.lerp`, `math.angle`, `math.distance`, easing functions |
| **array** | Array manipulation functions |
| **string** | String utilities |
| **collision** | Geometric tests via [lua-geo2d](https://github.com/eigenbom/lua-geo2d) |

---

## What's Next

Implementation order for remaining Phase 10 work:

| Category | Items | Status |
|----------|-------|--------|
| **Pure utilities** | math (lerp, angle, easing), array, string | Not started |
| **Value objects** | color | Not started |
| **Resource manager** | sounds, music on `an` | Not started |
| **Child objects** | random, input, timer, spring, collider, camera, animation, shake | Not started |
| **External libs** | Integrate lua-geo2d for collision utilities | Not started |
