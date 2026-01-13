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
│   └── build.bat
├── game/                   # Master framework (YueScript source)
│   ├── init.yue
│   ├── init.lua
│   ├── object.yue
│   └── object.lua
├── main.yue                # Test file (runs from Anchor/ root)
├── main.lua
├── yue.exe                 # YueScript compiler
├── assets/                 # Test assets
├── docs/
├── reference/
└── scripts/
```

- `engine/` contains only C code and build artifacts
- `game/` contains the master copy of the YueScript framework
- Test files live in Anchor/ root, using `require 'game.init'`

### Game Repository

```
emoji-ball-battles/         # (or any game)
├── tools/
│   ├── anchor.exe          # Copied from Anchor/engine/build/
│   └── yue.exe             # YueScript compiler
├── anchor/                 # Framework (copied from Anchor/game/ or previous game)
│   ├── init.yue
│   ├── init.lua
│   ├── object.yue
│   └── object.lua
├── main.yue                # Game code
├── main.lua
└── assets/
```

- Each game is self-contained
- No submodules, no symlinks
- Framework files are copied, not linked
- Games use `require 'anchor.object'`, framework uses `require 'game.object'`

---

## Build and Run Workflow

### Compiling YueScript

From Anchor/:
```bash
./yue.exe -r game/init.yue
./yue.exe -r game/object.yue
./yue.exe -r main.yue
```

The `-r` flag preserves line numbers for error reporting.

### Running

From Anchor/:
```bash
./engine/build/anchor.exe .
```

The C engine calls a global `update(dt)` function in Lua each frame.

### Engine Build

From `Anchor/engine/`:
```bash
./build.bat
```

---

## Framework Architecture

### Global Update Function

The C engine calls a single global `update(dt)` function. Everything else happens on the Lua/YueScript side. The C side will not change further.

### init.yue

```yuescript
global *

require 'game.object'

an = object 'an'

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

## Cleanup

The `cleanup` method handles two tasks:

1. **Remove marked actions** — For each object, remove actions that returned `true`
2. **Remove dead children** — Iterate in reverse (children-first) for proper destroy order

When removing dead children:
- Calls `child\destroy!` if child has a destroy method
- Clears `parent[child.name]` reference
- Clears `child[parent.name]` reference
- Clears `child.parent` reference

---

## Testing

Tests run from Anchor/ root using `main.yue`. The test runner is an action on `an`:

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

### Test Coverage (21 tests)

1. Complex tree (4 levels deep)
2. Bidirectional named links
3. Tags and is() method
4. Kill middle of tree (branch)
5. After cleanup (branch removed)
6. Named child replacement
7. Kill by tag
8. After tag kill cleanup
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
21. Final state

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
4. **Master framework in Anchor/game/** — New games copy from here, or from previous game
5. **Single update entry point** — C only calls `update(dt)`, Lua handles phases internally
6. **Root object named `an`** — May change later, works for now
7. **Iterative DFS** — Easier to reason about than recursive
8. **Tags only in all(tag)** — Names accessed directly, not via query
9. **Actions as plain functions** — Not objects, just stored in parallel arrays
10. **`false` for anonymous action names** — Preserves array iteration
11. **`all()` returns dead objects** — Dead check is caller's responsibility
12. **Children-first destroy order** — Iterate objects in reverse for cleanup

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
| Documentation comments in object.yue | Done |
| Test suite (21 tests) | Done |

---

## What's Next

| Feature | Status |
|---------|--------|
| Operators (`^`, `/`, `+`, `>>`) | Not started |
| Operator inheritance (`__inherited`) | Not started |
| Phase helpers (`U`, `L`, `X`, `E`) | Not started |
| Horizontal links (`link(target, callback)`) | Not started |
| Built-in objects (Timer, Spring, Collider) | Not started |
