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
│   └── build.bat          # Also copies exe to emoji-ball-battles/tools/
├── game/                   # Master framework (YueScript source)
│   ├── init.yue
│   └── object.yue
├── docs/
├── reference/
└── scripts/
    └── new-game.sh
```

- `engine/` contains only C code and build artifacts
- `game/` contains the master copy of the YueScript framework
- No `engine/yue/` or `engine/tools/` folders

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

---

## Build and Run Workflow

### Compiling YueScript

From the game directory:
```bash
tools/yue.exe -r anchor      # Compile framework
tools/yue.exe -r main.yue    # Compile game code
```

The `-r` flag preserves line numbers for error reporting.

### Running

```bash
tools/anchor.exe .
```

The C engine calls a global `update(dt)` function in Lua each frame.

### Engine Build

From `Anchor/engine/`:
```bash
./build.bat
```

This builds `anchor.exe` and automatically copies it to `emoji-ball-battles/tools/` if that folder exists.

---

## Creating New Games

The `scripts/new-game.sh` script creates new game projects:

```bash
# Using master framework from Anchor/game/
./scripts/new-game.sh my-new-game

# Copying framework from a previous game
./scripts/new-game.sh my-new-game --from emoji-ball-battles
```

The `--from` option copies the `anchor/` framework from a previous game, useful when developing sequentially and the previous game has framework improvements not yet in the master copy.

---

## Framework Architecture

### Global Update Function

The C engine calls a global `update(dt)` function. The Lua/YueScript side handles everything internally.

### init.yue

```yuescript
global *

require 'anchor.object'

an = object 'an'

update = (dt) ->
  all_objects = an\all!
  for obj in *all_objects
    continue if obj.dead
    obj\update dt if obj.update
```

- Creates global root object `an`
- Defines global `update` function called by C
- Collects all objects via `an\all!` and runs their update methods

### object.yue

```yuescript
global *

class object
  new: (name) =>
    @name = name
    @parent = nil
    @children = {}
    @dead = false
    @tags = {}

  add: (child) =>
    @children[] = child
    child.parent = @
    if child.name
      @[child.name] = child
    if @name
      child[@name] = @
    @

  all: (tag) =>
    nodes = {}
    stack = {}
    for i = #@children, 1, -1
      stack[] = @children[i]
    while #stack > 0
      node = table.remove stack
      if not node.dead
        if tag
          nodes[] = node if node.tags[tag]
        else
          nodes[] = node
        for i = #node.children, 1, -1
          stack[] = node.children[i]
    nodes
```

Key decisions:
- Uses `global *` instead of explicit exports
- `add` creates bidirectional named links (parent gets `@[child.name]`, child gets `@[parent.name]`)
- `all(tag)` uses iterative DFS (not recursive) for easier reasoning
- `all(tag)` only matches tags, not names (names are accessed directly)
- `all()` without argument returns all descendants
- Collection order is DFS left-to-right

---

## Death Semantics

From Phase 10 plan:

1. `kill()` immediately sets `.dead = true` on self AND all descendants (synchronous propagation)
2. Actual removal from tree happens at end-of-frame cleanup
3. Children never outlive parents
4. Dead objects are skipped during collection (and their children aren't traversed)

---

## Testing

Tests run from `emoji-ball-battles/`. The `all()` collection was tested with a tree:

```
      an
     / | \
    a  b  c
   /|     |
  d e     f
  |      /|\
  g     h i j
```

Expected DFS left-to-right order: `a, d, g, e, b, c, f, h, i, j` — verified working.

---

## YueScript Idioms

- Use `list[] = item` instead of `table.insert list, item`
- Use `global *` at top of file to make all definitions global
- Use `for item in *list` for array iteration
- Use `\method!` for method calls (compiles to `obj:method()`)

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
