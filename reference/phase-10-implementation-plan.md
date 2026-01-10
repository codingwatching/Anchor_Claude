# Phase 10: YueScript Object System - Detailed Implementation Plan

## Overview

Phase 10 transforms Anchor from a C engine with Lua bindings into a full game framework. The core deliverable is the object tree system with operators, plus built-in objects (Timer, Spring, Collider, etc.) that wrap C resources.

---

## 10.1 YueScript Build Integration

**Goal:** `.yue` files compile to `.lua` with accurate line number reporting for debugging.

**Steps:**
1. Install YueScript compiler (via LuaRocks or standalone)
2. Create `compile-yue.bat` script that runs `yue -r` on framework and game folders
3. Test that syntax errors report correct `.yue` line numbers

### Repository Structure

Each game is its own repository with Anchor as a submodule:

```
emoji-ball-battles/              # Game repo (self-contained)
├── anchor/                      # Git submodule → Anchor repo
│   ├── engine/
│   │   ├── src/anchor.c
│   │   ├── yue/                 # Framework source files
│   │   │   ├── object.yue
│   │   │   ├── timer.yue
│   │   │   └── ...
│   │   └── build/
│   │       └── anchor.exe
│   └── ...
├── yue/                         # Junction → anchor/engine/yue (created by setup.bat)
├── main.yue                     # Game code
├── player.yue
├── assets/
│   └── emoji.png
└── setup.bat                    # Creates junction after clone
```

The Anchor repo itself contains just the engine (no games):

```
Anchor/
├── engine/
│   ├── src/anchor.c
│   ├── yue/                     # Framework source
│   │   ├── object.yue
│   │   ├── timer.yue
│   │   └── ...
│   └── build/
├── docs/
├── reference/
├── test/
└── website/
```

### Setup for New Game

```bash
git clone --recursive <game-repo>    # Clones game + Anchor submodule
setup.bat                            # Creates yue junction (Windows)
```

### How Requires Work

Game code uses:
```yuescript
object = require 'yue.object'
timer = require 'yue.timer'
```

The `yue/` junction points to `anchor/engine/yue/`, so Lua finds `./yue/object.lua`.

### Bundled .exe

When bundling for release:
1. Compile all `.yue` → `.lua`
2. Copy framework (`anchor/engine/yue/*.lua`) into staging as `yue/`
3. Copy game code and assets into staging
4. Zip and append to exe

The bundled structure mirrors development, so requires work identically.

---

## 10.2 Update Loop

**Goal:** Define the execution model that everything else plugs into.

The C engine calls into Lua each frame with a specific order. This is foundational — the action system, object methods, and built-in objects all depend on this execution model.

**Frame execution order:**
1. **Collect** — Traverse tree, gather all non-dead objects
2. **Early phase** — Run `early_update` methods, then early actions
3. **Main phase** — Run `update` methods, then main actions
4. **Late phase** — Run `late_update` methods, then late actions
5. **Cleanup** — Remove dead objects, call `destroy` hooks

**C side:**
```c
// In anchor.c main loop
void engine_update(double dt) {
    lua_getglobal(L, "engine_collect_objects");
    lua_call(L, 0, 0);

    lua_getglobal(L, "engine_run_early");
    lua_pushnumber(L, dt);
    lua_call(L, 1, 0);

    lua_getglobal(L, "engine_run_main");
    lua_pushnumber(L, dt);
    lua_call(L, 1, 0);

    lua_getglobal(L, "engine_run_late");
    lua_pushnumber(L, dt);
    lua_call(L, 1, 0);

    lua_getglobal(L, "engine_cleanup");
    lua_call(L, 0, 0);
}
```

**YueScript side:**
```yuescript
-- Global root object (TBD: may change)
root = object 'root'

engine_collect_objects = ->
  _collected = {}
  collect_recursive root

collect_recursive = (obj) ->
  return if obj.dead
  table.insert _collected, obj
  for child in *obj.children
    collect_recursive child

engine_run_early = (dt) ->
  for obj in *_collected
    continue if obj.dead
    obj\early_update dt if obj.early_update
    if obj.early_actions
      for action in *obj.early_actions
        continue if action.dead
        action.dead = true if action.fn(obj, dt) == true

engine_run_main = (dt) ->
  for obj in *_collected
    continue if obj.dead
    obj\update dt if obj.update
    if obj.actions
      for action in *obj.actions
        continue if action.dead
        action.dead = true if action.fn(obj, dt) == true

engine_run_late = (dt) ->
  for obj in *_collected
    continue if obj.dead
    obj\late_update dt if obj.late_update
    if obj.late_actions
      for action in *obj.late_actions
        continue if action.dead
        action.dead = true if action.fn(obj, dt) == true

engine_cleanup = ->
  cleanup_recursive root

cleanup_recursive = (obj) ->
  -- Remove dead children
  i = 1
  while i <= #obj.children
    child = obj.children[i]
    if child.dead
      child\destroy! if child.destroy
      obj[child.name] = nil if child.name
      table.remove obj.children, i
    else
      cleanup_recursive child
      i += 1

  -- Clean up dead actions
  remove_dead_actions obj.early_actions if obj.early_actions
  remove_dead_actions obj.actions if obj.actions
  remove_dead_actions obj.late_actions if obj.late_actions

remove_dead_actions = (list) ->
  i = 1
  while i <= #list
    if list[i].dead
      table.remove list, i
    else
      i += 1
```

**Key points:**
- Methods run before actions in each phase
- Actions return `true` to self-remove (one-shot behavior)
- Objects created mid-frame are collected next frame (don't run in creation frame)

---

## 10.3 Base Object Class

**Goal:** Implement `object` class with tree operations and operator syntax.

**File:** `yue/object.yue`

**Core properties (every object has these):**
- `name` — optional identifier string
- `parent` — reference to parent object (nil for root)
- `children` — array of child objects
- `dead` — boolean, set to true when killed
- `tags` — set of string tags

**Constructor:**
```yuescript
new: (name, args) =>
  @name = name
  @parent = nil
  @children = {}
  @tags = {}
  @dead = false
  if args
    for k, v in pairs args
      @[k] = v
```

**Tree operations:**

1. `add(child)` — Add child to tree
   - Appends to `@children`
   - Sets `child.parent = @`
   - If child has name: creates `@[child.name] = child`
   - If parent has name: creates `child[@name] = @`
   - **If name already exists:** kills existing child first (replacement semantics)
   - Returns `@` for chaining

2. `remove(child)` — Remove child from tree (internal, called during cleanup)
   - Removes from `@children` array
   - Clears `child.parent`
   - Clears named links

3. `kill()` — Mark for death
   - Sets `@dead = true`
   - **Immediately** propagates to all descendants (synchronous)
   - Triggers link callbacks for objects that linked to this
   - Actual removal happens at end-of-frame cleanup

**Tagging:**
- `tag(...)` — Add tags: `@\tag 'enemy', 'flying'`
- `is(name_or_tag)` — Check name or tag match
- `all(tag)` / `A(tag)` — Query all descendants matching tag

**Horizontal links:**
```yuescript
link: (target, callback) =>
  -- When target dies, callback runs (or kill() if no callback)
  @links = {} unless @links
  table.insert @links, {target: target, callback: callback}
```

---

## 10.4 Operators

**`^` (set/build):**
```yuescript
__pow: (other) =>
  if type(other) == 'function'
    other @          -- Run build function with self
  elseif type(other) == 'table'
    for k, v in pairs other
      @[k] = v       -- Assign properties
  @
```

**`/` (action):**
```yuescript
__div: (other) =>
  if type(other) == 'function'
    @\action other                              -- Anonymous main action
  elseif type(other) == 'table'
    if other.__early
      if type(other.__early) == 'function'
        @\early_action other.__early            -- U(fn)
      else
        @\early_action other.__early, other.__fn -- U('name', fn)
    elseif other.__late
      if type(other.__late) == 'function'
        @\late_action other.__late              -- L(fn)
      else
        @\late_action other.__late, other.__fn  -- L('name', fn)
    else
      name, fn = next other
      @\action name, fn if type(name) == 'string' -- X('name', fn)
  @
```

**`+` (add children):**
```yuescript
__add: (other) =>
  if type(other) == 'table'
    if other.children != nil  -- It's an object (has children array)
      @\add other
    else  -- It's an array of objects
      for child in *other
        @\add child
  @
```

**`>>` (flow to parent):**
```yuescript
__shr: (parent) =>
  parent\add @
  parent  -- Returns PARENT (critical for chaining)
```

---

## 10.5 Operator Inheritance

YueScript classes don't automatically inherit metamethods. We use `__inherited` to propagate:

```yuescript
@__inherited: (child) =>
  for mm in *{'__pow', '__div', '__add', '__shr'}
    unless rawget child.__base, mm
      child.__base[mm] = @__base[mm]
  unless rawget child, '__inherited'
    child.__inherited = @@__inherited
```

This ensures `player extends object` gets working `^`, `/`, `+`, `>>` operators.

---

## 10.6 Action System

Objects can have three types of actions:
- **Early actions** — run before main, for input/simulation prep
- **Main actions** — standard update logic
- **Late actions** — run after main, often for drawing

Each action can be:
- **Anonymous** — just a function, can't be cancelled
- **Named** — has a name, can be cancelled via `@name\kill!`

**Storage:**
```yuescript
@early_actions = []   -- Array of {name: string?, fn: function, dead: bool}
@actions = []         -- Main actions
@late_actions = []    -- Late actions
```

**Adding actions:**
```yuescript
action: (name_or_fn, fn) =>
  @actions = {} unless @actions
  if type(name_or_fn) == 'string'
    -- Named action: kill existing if present
    @[name_or_fn]\kill! if @[name_or_fn] and @[name_or_fn].kill
    action_obj = {name: name_or_fn, fn: fn, kill: => @dead = true}
    @[name_or_fn] = action_obj
    table.insert @actions, action_obj
  else
    -- Anonymous action
    table.insert @actions, {fn: name_or_fn}
  @
```

Similar for `early_action` and `late_action`.

---

## 10.7 Phase Helpers

```yuescript
E = object

U = (name_or_fn, fn) ->
  if fn then {__early: name_or_fn, __fn: fn}
  else {__early: name_or_fn}

L = (name_or_fn, fn) ->
  if fn then {__late: name_or_fn, __fn: fn}
  else {__late: name_or_fn}

X = (name, fn) -> {[name]: fn}
```

**Usage:**
```yuescript
@ / U (dt) => ...              -- Anonymous early
@ / U 'physics', (dt) => ...   -- Named early
@ / (dt) => ...                -- Anonymous main
@ / X 'update', (dt) => ...    -- Named main
@ / L (dt) => ...              -- Anonymous late
@ / L 'draw', (dt) => ...      -- Named late
```

---

## 10.8 Built-in Objects

Built-in objects are classes that extend `object` and wrap C-side resources. They come bundled with the engine. Examples:
- **Timer** — delayed and repeating callbacks, tweening
- **Spring** — damped spring animation
- **Collider** — Box2D physics body wrapper
- (more to come)

**Common pattern:**
1. They extend the base `object` class
2. They're instantiated and added as children: `@ + timer()`
3. They're named after themselves by default (e.g., `timer()` has `name = 'timer'`)
4. Parent accesses them via that name: `@timer\after ...`
5. They implement `destroy()` to clean up C-side resources when killed

**Implementation approach:** When we reach this phase, we'll go through each built-in object individually, comparing with the old Anchor code, and rebuild it to match the new Lua bindings and engine architecture.

---

## 10.9 Implementation Order

Each step produces a testable milestone:

1. **YueScript compilation** — Get `.yue` → `.lua` working in build
2. **Update loop** — C calls Lua phases, basic frame execution
3. **Base object + tree operations** — `add`, `remove`, `kill`, children array
4. **Named linking** — Auto-create `parent.child_name` and `child.parent_name`
5. **Tagging** — `tag`, `is`, `all` / `A`
6. **Operators** — `^`, `/`, `+`, `>>` (test with inline objects)
7. **Action system** — Early/main/late phases, named actions
8. **Phase helpers** — `U`, `L`, `X`
9. **Horizontal links** — `link()` for sibling dependencies
10. **Named child replacement** — Adding same name kills old
11. **Built-in objects** — Timer, Spring, Collider (one at a time, comparing with old Anchor)

---

## Open Questions

1. **Root object:** Old Anchor had `an` as both a global root and a "God object" with singleton-like behavior. TBD whether to do something different this time.
