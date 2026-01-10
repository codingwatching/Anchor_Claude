# Anchor Engine Specification

*Anchor is a 2D game engine built around locality of behavior and minimal cognitive overhead. Code that belongs together stays together.*

---

## Table of Contents

1. [Core Philosophy](#core-philosophy)
2. [The Object Tree](#the-object-tree)
3. [Lifecycle](#lifecycle)
4. [Colliders, Springs, and Timers](#colliders-springs-and-timers)
5. [Locality of Behavior](#locality-of-behavior)
6. [Drawing](#drawing)
7. [Lua Syntax Features](#lua-syntax-features)
8. [YueScript Alternative](#yuescript-alternative)
9. [YueScript Class System Integration](#yuescript-class-system-integration)
10. [Technology Stack](#technology-stack)
11. [Rendering](#rendering)
12. [Error Handling](#error-handling)
13. [Build and Distribution](#build-and-distribution)
14. [File Structure](#file-structure)
15. [Performance Path](#performance-path)
16. [Deferred Features](#deferred-features)

---

## Core Philosophy

### Locality of Behavior

Everything about an object should be visible where that object is defined. No hunting through multiple files to understand what a particle does.

This principle serves multiple purposes:

**Human cognition:** Working memory is limited. The average person can hold 4-7 items in working memory at once. When understanding code requires jumping between files, each file consumes a slot. When you can see all of an object's behavior in one place, you free that working memory for actual problem-solving instead of mental bookkeeping.

**LLM cognition:** Context windows are finite. Every time code references something defined elsewhere, the LLM either has to have that code in context (consuming tokens) or guess at what it does (introducing errors). Locality keeps related code together, making it easier for AI assistants to understand and modify code correctly.

**Solo developer workflow:** When you return to code months later, locality means you can understand what something does without archaeology. You don't need to remember which manager class handles this type of object, or which event bus it subscribes to. Everything is right there.

In practice, this means:
- An object's properties, behaviors, and tree position should be expressible in one place
- Avoid systems that require registering things in one place and defining them in another
- Prefer inline definitions over indirection through managers or registries
- If you need to look at multiple files to understand one object, something is wrong

The operator syntax (`^`, `/`, `>>`) exists specifically to enable locality — you can declare an object, configure it, add behavior, and place it in the tree in a single expression:

```yuescript
E 'player' ^ {x: 100, y: 100, hp: 100}
  / X 'movement', (dt) =>
      @x += @vx * dt
      @y += @vy * dt
  / X 'draw', (dt) =>
      game\circle @x, @y, 10, colors.white
  >> arena
```

Everything about this player — its initial state, its movement logic, its rendering — is visible in one place. Compare to architectures where you'd define a Player class in one file, register it with an entity manager, subscribe to input events through an event bus, and render it through a render system that queries for Renderable components. The expressiveness is similar, but the *understanding* is scattered across four or five locations.

### Tree-Based Ownership

Objects form a tree. When a parent dies, all descendants die automatically. No manual cleanup, no orphaned references.

### Action-Based, Not Rules-Based

Game logic lives in update functions attached to objects, not in centralized rule systems that scan the world. The code for a seeker enemy lives in the seeker, not in a "movement system."

### No Bureaucracy

Bureaucracy is code you write to satisfy the engine rather than to describe your game. It's the ceremony of getting things to work, not the work itself.

Examples of bureaucracy:
- **Import/export ceremonies:** When adding a new feature requires touching a module registry, updating an index file, or adding entries to multiple places before the code is even reachable.
- **Registration systems:** "Register this handler with the handler manager." "Add this system to the system list." "Subscribe this listener to the event bus." The feature doesn't exist until you've told three different systems about it.
- **Configuration objects:** Objects that exist solely to satisfy an API shape. `new PlayerConfig({ settings: new PlayerSettings({ ... }) })` when you just want to set a few properties.
- **Dependency injection:** When getting access to a service requires declaring it as a constructor parameter, registering providers, and understanding scope rules.
- **Factory patterns when a constructor would do:** `PlayerFactory.createPlayer(PlayerType.BASIC)` instead of `Player()`.

These patterns have their place — large teams need boundaries, enterprise software needs flexibility. But for solo development, they're pure friction. Every layer of indirection is a place where bugs can hide and a concept you must hold in your head.

Anchor actively rejects these patterns. Instead:
- **Globals are available:** `an`, `E`, `X`, `U`, `L`, `A` are just there. No imports needed. You can use them from anywhere, in any file, without setup.
- **Direct mutation:** Set properties directly. `self.hp = 100`. No setter methods, no signals, no observers pattern. If you want to react to a change, you check the value in your update function.
- **Explicit calls:** When something happens, you call a function. `enemy:take_damage(10)`. No event buses, no implicit subscriptions, no wondering "what else triggers when this happens?"
- **Zero setup:** Drop a `.ttf` in a folder and it loads. Create an object and it exists. The engine assumes you want things to work, not that you want to configure how they work.

The test: can a new piece of game logic be added in one place, with one line of code? If adding a "flash white when hit" feature requires changes to three files, question whether that complexity is necessary.

---

## The Object Tree

### Structure

```
arena
├── player
│   ├── collider
│   ├── main_spring
│   └── shoot_spring
├── enemies
│   ├── seeker_1
│   │   └── collider
│   ├── seeker_2
│   │   └── collider
│   └── spawner_timer
├── projectiles
│   ├── bullet_1
│   │   └── collider
│   └── bullet_2
│       └── collider
└── arena_timer
```

### Automatic Named Links

When an object is added to another, named links are automatically created based on the `.name` property of each object:

```lua
-- Given:
-- enemies.name = 'enemies'
-- seeker_1.name = 'seeker_1'

enemies:add(seeker_1)

-- These links are created automatically:
enemies.seeker_1 = seeker_1     -- Parent can reference child by child's name
enemies.children[1] = seeker_1  -- Child also in children array
seeker_1.parent = enemies       -- Child has parent reference
seeker_1.enemies = enemies      -- Child can reference parent by parent's name
```

The `.name` property is a real field on every object. Both the parent-to-child link (`enemies.seeker_1`) and child-to-parent link (`seeker_1.enemies`) are derived from this field. If an object has no name (anonymous), only `child.parent` and `parent.children[n]` links exist.

### Key Properties

Every object has:

- `name` — optional identifier for referencing (e.g., `arena.player`)
- `parent` — reference to parent object (nil for root)
- `children` — array of child objects
- `dead` — boolean, set to true when killed
- `tags` — set of string tags for querying

### Tree Operations

```lua
-- Adding children
parent:add(child)

-- Querying
local enemies = arena:all('enemy')  -- all descendants with tag 'enemy'
local player = arena.player         -- direct child by name

-- Killing (immediate recursive death propagation, removal at end-of-frame)
enemy:kill()
```

### Named Child Behavior

Adding a child with a name that already exists on the parent **kills the existing child first**:

```lua
arena:add(object('ball'))  -- arena.ball = this ball
arena:add(object('ball'))  -- old ball killed, arena.ball = new ball
```

This enables replacement patterns without explicit cleanup.

### Horizontal Links

The tree handles vertical dependencies automatically — when a parent dies, children die. For horizontal dependencies (siblings, cousins), use `link`:
```lua
-- I die when player dies (both are children of arena)
self:link(self.arena.player)

-- With callback (runs instead of default kill)
self:link(self.arena.player, function() self:die() end)

-- Tether dies if either endpoint dies
self:link(unit_a)
self:link(unit_b)
```

When the linked target dies, the callback runs (or `kill()` if no callback provided). Links are cleaned up automatically if the linking object dies first.

**When to use link vs tree:**
- If A should die when B dies, and A could logically be B's child → make A a child of B
- If A should die when B dies, but they're siblings or the hierarchy doesn't fit → use link
```lua
-- Use tree: shield orbits player, conceptually "belongs to" player
player + shield_orb()

-- Use link: enemy and player are siblings under arena
enemy:link(arena.player)

-- Use link: tether connects two peers (can't be child of both)
tether:link(unit_a)
tether:link(unit_b)
```

---

## Lifecycle

### Update Order

Each frame:

1. **Collect all objects** — Traverse tree, gather all non-dead objects
2. **Early actions** (`/ U`) — Run all early_action functions (tree order)
3. **Main actions** (`/`) — Run all action functions (tree order)
4. **Late actions** (`/ L`) — Run all late_action functions (tree order)
5. **Cleanup** — Remove dead objects, remove dead actions, call destroy() hooks

Objects created during the frame are collected in the next frame (they don't run actions in the frame they're created).

### Action Phase Execution

- All early actions complete before any main actions begin
- All main actions complete before any late actions begin
- Within each phase, actions execute in tree order (depth-first)
- Actions return `true` to remove themselves (one-shot behavior)

### Death Semantics

When `kill()` is called, death propagates **immediately** (synchronously) down the tree:

```lua
-- kill() recursively sets .dead = true on self and all descendants
-- This happens immediately, in the same call
enemy:kill()

-- After kill(), within the same frame:
-- - enemy.dead == true
-- - All descendants also have .dead == true
-- - enemy still exists in tree (not yet removed)
-- - enemy's actions won't run (dead objects are skipped)

-- At end of frame:
-- - enemy:destroy() called (cleanup hook)
-- - enemy removed from parent's children
-- - enemy's children also destroyed
```

This immediate propagation means there are no "zombie states" during a frame — after `kill()` returns, the entire subtree is marked dead and will be skipped by the update loop. You never have to worry about a child object trying to access its dead parent.

**Links** are also processed during death:
```lua
enemy:kill()

-- In the same call:
-- 1. enemy.dead = true (and all descendants)
-- 2. Any objects that called link(enemy, callback) have their callbacks invoked
-- 3. Objects that called link(enemy) with no callback are killed
```

---

## Colliders, Springs, and Timers

Everything in Anchor is an object in the tree. Colliders, springs, and timers are objects with specialized functionality — they're added as children just like any other object and die when their parent dies.

These are examples of **engine objects** — classes that wrap C-side resources (physics bodies, audio handles, etc.). All engine objects follow the same pattern:

1. They're instantiated and added as children: `@ + timer()`
2. They're named after themselves by default, so `timer()` creates an object with `name = 'timer'`
3. The parent can access them via that name: `@timer:after(0.5, fn)`

This applies to colliders, springs, timers, and any future engine objects.

### Collider

A collider object manages a Box2D physics body:

```lua
-- Single collider (defaults to name 'collider')
self + collider('player', 'dynamic', 'circle', 12)  -- tag, body_type, shape_type, ...
self.collider:get_position()

-- Multiple colliders (explicit names)
self + collider('hitbox', 'player', 'dynamic', 'circle', 12)
self + collider('hurtbox', 'player_hurt', 'dynamic', 'circle', 20)
self.hitbox:get_position()
self.hurtbox:get_position()
```

Methods:
- `get_position()` → x, y
- `set_position(x, y)`
- `get_velocity()` → vx, vy
- `set_velocity(vx, vy)`
- `apply_force(fx, fy)`
- `apply_impulse(ix, iy)`

When killed, automatically destroys the Box2D body.

### Spring

A spring object provides damped spring animation:

```lua
-- Single spring (defaults to name 'spring')
self + spring(1, 200, 10)   -- initial, stiffness, damping
self.spring:pull(0.5)       -- Displace from target
self.spring.x               -- Current value (updated each frame)

-- Multiple springs (explicit names)
self + spring('attack', 1, 200, 10)
self + spring('hit', 1, 300, 15)
self.attack:pull(0.5)
self.hit:pull(0.3)
```

Each spring object represents one spring with its own value.

### Timer

A timer object provides delayed and repeating callbacks, plus tweening. Add a timer as a child, then call its methods:

```lua
-- Add timer child (typically in constructor)
self + timer()

-- One-shot (anonymous)
self.timer:after(0.5, function() self.can_shoot = true end)

-- One-shot (named — can be cancelled/replaced)
self.timer:after(0.15, 'flash', function() self.flashing = false end)

-- Repeating (anonymous)
self.timer:every(2, function() self:spawn_enemy() end)

-- Repeating (named)
self.timer:every(1, 'regen', function() self.hp = self.hp + 1 end)

-- Cancel a named callback
self.timer.flash:kill()

-- Tween
self.timer:tween(0.5, self, {x = 100, y = 200}, math.cubic_in_out, function()
    self:kill()
end)
```

Timer signature: `after(duration, [name], callback)` — the name parameter is optional and appears in the middle position.

---

## Locality of Behavior

### Action-Based Objects

Most game objects follow this pattern: create → configure → add behavior → add to tree.

```lua
-- Everything about this particle is RIGHT HERE
emoji_particle = class:class_new(object)
function emoji_particle:new(x, y, args)
    self:object(nil, args)
    self.x, self.y = x, y
    self.r = self.r or an:random_angle()
    self.v = self.v or an:random_float(75, 150)
    self.duration = self.duration or an:random_float(0.4, 0.6)

    self + timer()
    self + spring(1, 200, 10)
    self.spring:pull(0.5)

    self.timer:tween(self.duration, self, {v = 0, sx = 0, sy = 0}, math.linear, function()
        self:kill()
    end)
end

function emoji_particle:update(dt)
    self.x = self.x + self.v * math.cos(self.r) * dt
    self.y = self.y + self.v * math.sin(self.r) * dt
    effects:draw_image(self.emoji, self.x, self.y, self.r, self.sx * self.spring.x, self.sy)
end
```

Even rules-based code benefits from locality:

```lua
-- All mana logic together in one place
function arena:spend_mana(amount)
    if self.mana >= amount then
        self.mana = self.mana - amount
        return true
    end
    return false
end

function arena:refresh_mana()
    self.max_mana = self.max_mana + 1
    self.mana = self.max_mana
end
```

The key insight: **rules-based code can live inside objects when the rule is self-contained and doesn't need to be split across multiple objects.** The water simulation in Tidal-Waver is rules-based (mathematical propagation across springs), but it's contained in one action on the arena object, so it maintains locality.

---

## Drawing

Objects draw themselves in their update function. Drawing is decoupled from tree structure — an object can draw to any layer.

```lua
function player:update(dt)
    -- Update logic
    self.x, self.y = self.collider:get_position()

    -- Drawing to game layer (spring used for scale)
    game:push(self.x, self.y, self.r, self.spring.x, self.spring.x)
        game:circle(self.x, self.y, 10, self.color)
    game:pop()

    -- Drawing to effects layer
    if self.flashing then
        effects:circle(self.x, self.y, 15, colors.white)
    end
end
```

Draw order within a layer is determined by submission order (the order Lua calls draw functions), not tree order.

---

## Lua Syntax Features

*These features were developed after analyzing [amulet.xyz](http://amulet.xyz), Ian MacLarty's Lua game engine. Amulet uses elegant operator overloading for scene graph construction. We adapted the patterns that fit Anchor's model while rejecting those that don't (like transforms-as-nodes and coroutine-based actions).*

### Operator Overview

Anchor uses Lua's operator overloading to create a concise, pipeline-style syntax for object construction:

| Operator | Operation | Description |
|----------|-----------|-------------|
| `^` | set/build | Assign properties (table) or run init code (function) |
| `/` | action | Add action (runs every frame) |
| `+` | add children | Add child objects to parent (parent-centric) |
| `>>` | flow to parent | Add self to parent, **returns parent** (object-centric) |

The `/` operator handles all action phases. Use the `U` and `L` helpers to mark early/late phases:

| Pattern | Phase | When it runs |
|---------|-------|--------------|
| `/ U fn` | early | Before main actions |
| `/ fn` | main | The main update phase |
| `/ L fn` | late | After main actions (often drawing) |

Operators are ordered by precedence (highest to lowest: `^`, then `/`, then `+`, then `>>`), so they chain naturally left-to-right without parentheses.

### Single-Letter Aliases

For elegance, Anchor provides single-letter aliases that look like runes:

| Alias | Purpose | Mnemonic |
|-------|---------|----------|
| `E` | Object constructor | Entity |
| `U` | Early action helper | Under/underlying |
| `L` | Late action helper | Late |
| `X` | Named action helper | eXplicit |
| `A` | Query descendants | All |

These are defined in the engine:

```lua
E = object
U = function(name_or_fn, fn) ... end  -- U(fn) or U('name', fn)
L = function(name_or_fn, fn) ... end  -- L(fn) or L('name', fn)
X = function(name, fn) return {[name] = fn} end
-- A is a method alias: self:A('tag') == self:all('tag')
```

**Future single-letter aliases** should prefer these characters, chosen for their angular, runic appearance (symmetrical, minimal roundness):

```
E, X, A, T, L, V, U, Y, I, H
```

### Action Helpers: `U`, `L`, and `X`

The `/` operator handles all action phases. Use helpers to specify phase or name:

**Phase helpers (`U` and `L`):**
```lua
self / U(function(self, dt) ... end)  -- early action
self / function(self, dt) ... end     -- main action (default)
self / L(function(self, dt) ... end)  -- late action
```

**Named action helper (`X`):**
```lua
-- Anonymous action (no name, can't be cancelled)
self / function(self, dt) ... end

-- Named action using X helper (self.seek points to this)
self / X('seek', function(self, dt) ... end)

-- Cancel later
self.seek:kill()
```

**Combining phase and name** — use `U` or `L` with a name parameter:
```lua
self / U('water_sim', early_fn)   -- named early action
self / X('update', update_fn)     -- named main action
self / L('draw', draw_fn)         -- named late action
```

This allows all action types to participate in operator pipelines:

```lua
E() ^ {x = 100, y = 200}
   / X('move', function(self, dt)
         self.x = self.x + 50 * dt
     end)
   / L('draw', function(self, dt)
         game:circle(self.x, self.y, 10, color)
     end)
   >> arena
```

### The `^` Operator: Set and Build

The `^` operator initializes an object. It detects the type of its right operand:

**With a table** — assigns properties:
```lua
object() ^ {x = 100, y = 200, speed = 50}
```

**With a function** — runs immediately (build/init):
```lua
object() ^ function(self)
               self.x, self.y = 100, 200
               self:add(timer())
           end
```

### The `/` Operator: Actions

The `/` operator adds actions. Use `U` and `L` helpers to specify early/late phases:

```lua
object() ^ {x = 100, y = 200}
         / U(function(self, dt)
               -- early: runs first
           end)
         / function(self, dt)
               -- main update
               game:circle(self.x, self.y, 10, color)
           end
         / L(function(self, dt)
               -- late: runs last, good for drawing that needs final positions
           end)
```

Actions return `true` to remove themselves (one-shot behavior). Returning `true` only removes the action, not the object.

### The `>>` Operator: Flow to Parent

The `>>` operator adds the left object as a child of the right object and **returns the parent**:

```lua
object() ^ {x = 100, y = 200}
         / function(self, dt)
               game:circle(self.x, self.y, 10, color)
           end
         >> arena
-- Result: the newly created object is added to arena
-- Return value: arena (the parent)
```

This reads as a pipeline: "create object, configure it, flow it into arena."

### The `+` Operator: Add Children

The `+` operator adds children to a parent and returns the parent:

```lua
arena + paddle('left', 30, 135)
      + paddle('right', 450, 135)
      + ball()
```

With a plain table, each element is added as a child:

```lua
arena + { wall('top'), wall('bottom'), goal('left'), goal('right') }
```

### Choosing Between `+` and `>>`

Both add objects to parents, but they have different perspectives:

**`+` is parent-centric** — good for adding multiple siblings:
```lua
arena + paddle('left')
      + paddle('right')
      + ball()
```

**`>>` is object-centric** — good for pipeline construction of inline objects:
```lua
object() ^ {x = 100}
         / update_fn
         >> arena
```

### Named vs Anonymous

The tree automatically manages named children: adding a child with a name that already exists kills the old child first. This enables cancellation and replacement without explicit tag systems.

**Objects** are named via their constructor:
```lua
-- Anonymous (can't be referenced or replaced)
object() ^ {x = 100} >> arena

-- Named (arena.ball points to this, adding another 'ball' replaces it)
object('ball') ^ {x = 100} >> arena
```

**Timer callbacks** are named via an optional second argument:
```lua
-- Anonymous
self.timer:after(0.5, function() self.can_shoot = true end)

-- Named (self.timer.flash points to this, can be killed/replaced)
self.timer:after(0.15, 'flash', function() self.flashing = false end)
```

**Actions** can use the `U`, `L`, and `X` helpers, or methods:
```lua
-- Anonymous actions with phase helpers
E() / U(early_fn) / update_fn / L(late_fn) >> arena

-- Named actions with phase helpers
self / U('water_sim', fn)   -- named early
self / X('update', fn)      -- named main
self / L('draw', fn)        -- named late

-- Named with methods (alternative)
self:action('update', fn)
self:early_action('water_sim', fn)
self:late_action('water_draw', fn)

-- Cancel later
self.water_sim:kill()
```

The helpers are simply:
```lua
U = function(name_or_fn, fn) ... end  -- marks early phase
L = function(name_or_fn, fn) ... end  -- marks late phase
X = function(name, fn) return {[name] = fn} end  -- marks name only
```

The `/` operator detects these patterns and creates the appropriate action type.

### Complete Examples

**Simple particle (anonymous):**
```lua
E() ^ {x = x, y = y, r = an:random_angle(), duration = 0.5}
   / function(self, dt)
         self.x = self.x + 50 * math.cos(self.r) * dt
         self.y = self.y + 50 * math.sin(self.r) * dt
         self.duration = self.duration - dt
         if self.duration <= 0 then return true end
         effects:circle(self.x, self.y, 3, color)
     end
   >> arena
```

**Named ball (can be referenced/replaced):**
```lua
E('ball') ^ {x = 240, y = 135, vx = 100, vy = 100}
          / function(self, dt)
                self.x = self.x + self.vx * dt
                self.y = self.y + self.vy * dt
                game:circle(self.x, self.y, 8, an.colors.white)
            end
          >> arena

-- Later: arena.ball refers to this object
-- Adding another 'ball' kills the old one automatically
```

**Spawning with automatic replacement:**
```lua
function arena:spawn_ball()
    -- Create named ball, add to self
    -- The >> operator returns self (arena), but the ball is created and added
    E('ball') ^ {x = 240, y = 135, vx = 100, vy = 100}
              / function(self, dt)
                    self.x = self.x + self.vx * dt
                    self.y = self.y + self.vy * dt
                    game:circle(self.x, self.y, 8, an.colors.white)
                end
              >> self
    -- self.ball now references the newly created ball
end

-- Each call replaces the previous ball
arena:spawn_ball()
arena:spawn_ball()  -- old ball killed, new ball created
```

**Query with A (alias for all):**
```lua
-- Iterate
for _, enemy in ipairs(arena:A('enemy')) do
    enemy:explode()
end

-- Count
if #arena:A('enemy') == 0 then
    arena:next_wave()
end
```

### The `all()` Function

`object:all(name_or_tag)` returns all descendants matching the name or tag as a plain array:

```lua
-- Iterate over all enemies
for _, enemy in ipairs(arena:all('enemy')) do
    enemy:explode()
end

-- Count
if #arena:all('enemy') == 0 then
    arena:next_wave()
end

-- Set property on each (manual loop)
for _, enemy in ipairs(arena:all('enemy')) do
    enemy.dead = true
end
```

**What `all()` does NOT do:**

- Does not aggregate reads (`arena:all('enemy').hp` returns nil)
- Does not call methods directly on the result
- Does not recurse into matched objects' children (flat search from self downward)

### The `set()` Function

`object:set(table)` assigns all key-value pairs from the table to the object and returns self for chaining:

```lua
object('player')
    :set({x = 100, y = 200, hp = 100, speed = 150})
    :tag('friendly', 'controllable')
```

---

## YueScript Alternative

*YueScript (evolved from MoonScript) is a language that compiles to Lua. It offers significant syntactic improvements: significant whitespace, `@` for self, `=>` for methods with implicit self, no `end` keywords, `+=`/`-=` operators, and optional parentheses. The operator-based object construction style fits YueScript's aesthetic particularly well.*

### Single-Letter Aliases in YueScript

The same single-letter aliases work in YueScript:

```yuescript
E = object                        -- Entity/object constructor
U = (name_or_fn, fn) -> ...       -- U(fn) or U('name', fn)
L = (name_or_fn, fn) -> ...       -- L(fn) or L('name', fn)
X = (name, fn) -> {[name]: fn}    -- named action helper (eXplicit)
-- A is a method alias: @\A 'tag' == @\all 'tag'
```

### Syntax Comparison

| Lua | YueScript |
|-----|-----------|
| `function(self, dt) ... end` | `(dt) => ...` |
| `self.x` | `@x` |
| `self:method()` | `@\method!` |
| `layer:draw()` | `layer\draw!` |
| `self.x = self.x + 1` | `@x += 1` |
| `if cond then return true end` | `return true if cond` |
| `for _, item in ipairs(list)` | `for item in *list` |

### Examples in YueScript

**Simple particle (anonymous):**
```yuescript
E! ^ {x: x, y: y, r: an\random_angle!, duration: 0.5}
   / (dt) =>
       @x += 50 * math.cos(@r) * dt
       @y += 50 * math.sin(@r) * dt
       @duration -= dt
       return true if @duration <= 0
       effects\circle @x, @y, 3, color
   >> arena
```

**Named ball (can be referenced/replaced):**
```yuescript
E 'ball' ^ {x: 240, y: 135, vx: 100, vy: 100}
         / (dt) =>
             @x += @vx * dt
             @y += @vy * dt
             game\circle @x, @y, 8, an.colors.white
         >> arena

-- Later: arena.ball refers to this object
```

**With build function:**
```yuescript
E! ^ =>
       @x, @y = 100, 200
       @ + timer!
   / (dt) =>
       @x += 50 * dt
       game\circle @x, @y, 10, color
   >> arena
```

**Named actions with helpers:**
```yuescript
class arena extends object
  new: =>
    super 'arena'
    @water_springs_count = 52

    -- Named early action
    @ / U 'water_sim', (dt) =>
      for k = 1, 8
        for i = 1, @water_springs_count
          -- propagate spring velocities

    -- Named late action
    @ / L 'water_draw', (dt) =>
      @water_surface = {}
      for spring in *@water_springs.children
        -- build polyline using * iteration

-- Cancel later
@water_sim\kill!
```

**Player example:**
```yuescript
class player extends object
  new: (x, y, args) =>
    super 'player', args
    @x, @y = x, y

    -- Add collider and springs as children
    @ + collider 'player', 'dynamic', 'circle', 12
    @ + spring 'main', 1, 200, 10
    @ + spring 'shoot', 1, 300, 15

    -- Named action for input
    @ / X 'input', (dt) =>
      return if @stunned
      @vx = -@speed if an\is_down 'left'
      @vx = @speed if an\is_down 'right'

    -- Anonymous action (always runs)
    @ / (dt) =>
      @x, @y = @collider\get_position!

    -- Named late action for drawing
    @ / L 'draw', (dt) =>
      game\push @x, @y, 0, @main.x, @main.x
      game\circle @x, @y, 12, colors.green
      game\pop!
```

**Query with A (alias for all):**
```yuescript
-- Iterate with * syntax
for enemy in *@\A 'enemy'
  enemy\explode!
```

**Multiple children with `+`:**
```yuescript
arena + paddle 'left', 30, 135
      + paddle 'right', 450, 135
      + (E 'ball' ^ {x: 240, y: 135}
                  / (dt) => game\circle @x, @y, 8, an.colors.white)
```

**Spawning with automatic replacement:**
```yuescript
arena.spawn_ball = =>
  E 'ball' ^ {x: 240, y: 135, vx: 100, vy: 100}
           / (dt) =>
               @x += @vx * dt
               @y += @vy * dt
               game\circle @x, @y, 8, an.colors.white
           >> @
  -- @ball now exists due to named child auto-linking

-- Each call replaces the previous ball
arena\spawn_ball!
arena\spawn_ball!  -- old ball killed, new ball created
```

### Side-by-Side Comparison

**Lua (14 lines):**
```lua
E() ^ function(self)
          self.x, self.y = x, y
          self.v = an:random_float(50, 100)
          self.r = an:random_angle()
          self.duration = 0.5
      end
    / function(self, dt)
          self.x = self.x + self.v * math.cos(self.r) * dt
          self.y = self.y + self.v * math.sin(self.r) * dt
          self.duration = self.duration - dt
          if self.duration <= 0 then return true end
          effects:circle(self.x, self.y, 3, color)
      end
    >> arena
```

**YueScript (12 lines):**
```yuescript
E! ^ =>
       @x, @y = x, y
       @v = an\random_float 50, 100
       @r = an\random_angle!
       @duration = 0.5
   / (dt) =>
       @x += @v * math.cos(@r) * dt
       @y += @v * math.sin(@r) * dt
       @duration -= dt
       return true if @duration <= 0
       effects\circle @x, @y, 3, color
   >> arena
```

YueScript's compression comes from: `@` for self, `=>` for implicit self binding, `+=`/`-=` operators, postfix conditionals, no `end` keywords, optional parentheses, and `*` for array iteration. The operator-based construction style creates an almost DSL-like syntax for object definition.

---

## YueScript Class System Integration

Anchor uses YueScript's native class system with inheritance. This section details how the operator-based syntax integrates with YueScript classes.

### Base Object Class

The `object` class is the root of all game objects. It provides the core functionality and operators:

```yuescript
class object
  -- Class-level callback: propagate metamethods to child classes
  @__inherited: (child) =>
    -- Copy operator metamethods to child's base
    for mm in *{'__pow', '__div', '__add', '__shr'}
      unless rawget child.__base, mm
        child.__base[mm] = @__base[mm]
    -- Also propagate __inherited itself
    unless rawget child, '__inherited'
      child.__inherited = @@__inherited

  -- Constructor
  new: (name, args) =>
    @name = name
    @children = {}
    @tags = {}
    @dead = false
    if args
      for k, v in pairs args
        @[k] = v

  -- Tree operations
  add: (child) =>
    table.insert @children, child
    child.parent = @
    -- Create named links
    if child.name
      @[child.name] = child
    if @name
      child[@name] = @
    @

  remove: (child) =>
    for i, c in ipairs @children
      if c == child
        table.remove @children, i
        break
    child.parent = nil
    if child.name
      @[child.name] = nil

  -- Tagging
  tag: (...) =>
    for t in *{...}
      @tags[t] = true
    @

  is: (tag) =>
    @tags[tag] or @name == tag

  -- Query all descendants matching tag
  all: (tag) =>
    result = {}
    @\_collect_tagged tag, result
    result

  A: (tag) => @\all tag  -- Alias

  _collect_tagged: (tag, result) =>
    for child in *@children
      if child\is tag
        table.insert result, child
      child\_collect_tagged tag, result

  -- Kill (immediate recursive death propagation)
  kill: =>
    @dead = true
    for child in *@children
      child\kill!

  -- Actions
  action: (name_or_fn, fn) =>
    @actions = {} unless @actions
    if type(name_or_fn) == 'string'
      name, func = name_or_fn, fn
      @[name]\kill! if @[name] and @[name].kill
      action_obj = {name: name, fn: func, kill: -> @dead = true}
      @[name] = action_obj
      table.insert @actions, action_obj
    else
      table.insert @actions, {fn: name_or_fn}
    @

  early_action: (name_or_fn, fn) =>
    @early_actions = {} unless @early_actions
    if type(name_or_fn) == 'string'
      name, func = name_or_fn, fn
      @[name]\kill! if @[name] and @[name].kill
      action_obj = {name: name, fn: func, kill: -> @dead = true}
      @[name] = action_obj
      table.insert @early_actions, action_obj
    else
      table.insert @early_actions, {fn: name_or_fn}
    @

  late_action: (name_or_fn, fn) =>
    @late_actions = {} unless @late_actions
    if type(name_or_fn) == 'string'
      name, func = name_or_fn, fn
      @[name]\kill! if @[name] and @[name].kill
      action_obj = {name: name, fn: func, kill: -> @dead = true}
      @[name] = action_obj
      table.insert @late_actions, action_obj
    else
      table.insert @late_actions, {fn: name_or_fn}
    @

  -- Operators
  __pow: (other) =>
    if type(other) == 'function'
      other @
    elseif type(other) == 'table'
      for k, v in pairs other
        @[k] = v
    @

  __div: (other) =>
    -- Detect phase helpers (U, L) and named helper (X)
    if type(other) == 'function'
      @\action other
    elseif type(other) == 'table'
      if other.__early
        -- U helper: early action
        if type(other.__early) == 'function'
          @\early_action other.__early
        else
          -- U('name', fn) form
          @\early_action other.__early, other.__fn
      elseif other.__late
        -- L helper: late action
        if type(other.__late) == 'function'
          @\late_action other.__late
        else
          -- L('name', fn) form
          @\late_action other.__late, other.__fn
      else
        -- X helper: named main action
        name, fn = next other
        @\action name, fn if type(name) == 'string' and type(fn) == 'function'
    @

  __add: (other) =>
    if type(other) == 'table'
      if other.name != nil or other.children != nil  -- It's an object
        @\add other
      else  -- It's an array
        for child in *other
          @\add child
    @

  __shr: (other) =>
    other\add @
    other  -- Returns PARENT

-- Set up aliases
E = object
U = (name_or_fn, fn) ->
  if fn then {__early: name_or_fn, __fn: fn}
  else {__early: name_or_fn}
L = (name_or_fn, fn) ->
  if fn then {__late: name_or_fn, __fn: fn}
  else {__late: name_or_fn}
X = (name, fn) -> {[name]: fn}
```

### The `__inherited` Hook

The critical mechanism for operator support in child classes is `__inherited`. When you write:

```yuescript
class player extends object
```

YueScript automatically calls `object.__inherited(player)`. Our implementation copies the operator metamethods to the child class's base table, ensuring all descendants have working operators.

### Game Classes

Game classes extend `object` and automatically get all operators:

```yuescript
class player extends object
  new: (x, y, args) =>
    super 'player', args  -- Call parent constructor
    @x, @y = x, y
    @speed = 200
    
    -- Operators work! Inherited via __inherited callback
    @ + collider 'player', 'dynamic', 'circle', 12
    @ + spring 'main', 1, 200, 10
    
    @ / X 'input', (dt) =>
      @vx = -@speed if an\is_down 'left'
      @vx = @speed if an\is_down 'right'
```

### Deep Inheritance

Operators propagate through any inheritance depth:

```yuescript
class entity extends object
  new: (name, x, y, args) =>
    super name, args
    @x, @y = x, y
    @hp = 100

class enemy extends entity
  new: (x, y, args) =>
    super nil, x, y, args
    @\tag 'enemy'

class seeker extends enemy
  new: (x, y, args) =>
    super x, y, args
    @speed = 80
    
    -- Operators work at any depth
    @ / X 'seek', (dt) =>
      target = @arena.player
      return unless target
      -- Move toward target...
```

### Constructor Pattern

Always call `super` in child constructors:

```yuescript
class enemy extends object
  new: (x, y) =>
    super nil, {x: x, y: y}  -- or super 'enemy', args
    -- Enemy-specific init...
```

Forgetting `super` means no `@children`, `@tags`, etc.

### Special Methods

The engine automatically calls these methods on objects if they exist:

- `early_update(dt)` — called during early phase
- `update(dt)` — called during main phase
- `late_update(dt)` — called during late phase (often used for drawing)
- `destroy()` — called when object is removed from tree

Define them as regular class methods:
```yuescript
class enemy extends object
  update: (dt) =>
    @x, @y = @collider\get_position!

  late_update: (dt) =>
    game\circle @x, @y, @radius, @color
```

**Methods and actions both run.** If an object has both an `update:` method and `/` actions, the method runs first, then the actions. This allows class-defined behavior to coexist with dynamically added behavior.

### Colliders, Springs, and Timers Are Tree Objects

Colliders, springs, and timers are all proper tree objects (classes extending `object`). They live in `@children` like any other child, and die when their parent dies.

```yuescript
class collider extends object
  new: (tag, body_type, shape_type, ...) =>
    super 'collider'
    @body_id = an\physics_create_body tag, body_type, shape_type, ...

  get_position: => an\physics_get_position @body_id
  set_velocity: (vx, vy) => an\physics_set_velocity @body_id, vx, vy

  destroy: =>
    an\physics_destroy_body @body_id

-- Usage: added as child via + operator
@ + collider 'player', 'dynamic', 'circle', 12
```

The `destroy` method is called automatically when the collider dies (when parent dies or when explicitly killed).

### Method vs Property Access

In YueScript:
- `@foo` = `self.foo` (property access)
- `@\foo!` = `self:foo()` (method call)

```yuescript
@add_child child   -- WRONG: tries to access property, then call it
@\add_child child  -- Correct: method call
```

---

## Technology Stack

- **Language:** C99 with Lua 5.4 scripting (YueScript compiled to Lua)
- **Window/Input:** SDL2
- **Audio:** TBD (miniaudio or SoLoud — needs pitch shifting support)
- **Graphics:** OpenGL 3.3 Core Profile (WebGL 2.0 compatible)
- **Physics:** Box2D 3.1
- **Platforms:** Windows, Web (Emscripten)

---

## Engine State

The engine exposes timing and frame information to Lua:

```lua
an.frame      -- Render frame count (increments each rendered frame at 60Hz)
an.step       -- Physics step count (increments each physics tick at 120Hz)
an.game_time  -- Accumulated game time in seconds
```

Useful for:
- Deterministic effects (use `an.step` instead of random for consistent patterns)
- Debugging and profiling
- Replay systems (verify sync with step count)

---

## Rendering

Anchor uses OpenGL 3.3 Core Profile (WebGL 2.0 compatible) for rendering, targeting 480×270 or 640×360 base resolution (per-game configurable). The low resolution is rendered to a framebuffer texture, then scaled to fill the window while maintaining aspect ratio (letterboxing when needed), with nearest-neighbor filtering.

### Layers

Each layer is a framebuffer object (FBO). Draw commands are batched per layer:

1. Lua calls `layer:circle()`, `layer:draw_image()`, etc.
2. Geometry is batched into vertex buffers (batch breaks on texture/shader/blend mode changes)
3. At end of frame, layers are rendered via draw calls
4. Post-processing shaders are applied per-layer
5. Layers composite to screen in creation order (first created = bottom, last = top)

Layer order can be overridden if needed:
```lua
an:set_layer_order({'game', 'effects', 'ui'})
```

```lua
game = an:layer('game')
effects = an:layer('effects')
ui = an:layer('ui')

game:circle(x, y, radius, color)
game:rectangle(x, y, w, h, color)
game:rounded_rectangle(x, y, w, h, rx, ry, color)
game:line(x1, y1, x2, y2, color, width)
game:draw_image(img, x, y, r, sx, sy, ox, oy, color)
```

### Transform Stack

```lua
game:push(x, y, r, sx, sy)
    game:circle(0, 0, 10, color)  -- draws at (x, y) with rotation and scale
game:pop()
```

Transforms are computed on CPU and uploaded to shaders as uniforms.

### Sprites

Sprites are rendered as textured quads with smooth rotation (handled by the GPU). No precomputed rotation frames needed.

### Blend Modes

Two blend modes available per draw call:

```lua
layer:set_blend_mode('alpha')     -- default (standard transparency)
layer:set_blend_mode('additive')  -- for glows, explosions, energy effects
```

### Effects

Effects are fragment shaders applied to layer framebuffers. Shaders are loaded from files and controlled from Lua — there are no built-in effects in C.

**Fully deferred pipeline:** All shader operations (uniform setting, shader application) are queued during update and processed at frame end. This ensures all draw commands are complete before effects are applied.

**Layer effects (post-processing):**

Each layer has a ping-pong buffer system (`color_texture` ↔ `effect_texture`). When `layer_apply_shader()` is called, it queues a command that renders the current texture to the other buffer using the shader, then swaps. This allows effect chaining.

```lua
-- Load shaders from files
local outline_shader = shader_load_file('shaders/outline.frag')
local shadow_shader = shader_load_file('shaders/shadow.frag')

-- Deferred uniform setting (queues command, processed at frame end)
layer_shader_set_vec2(outline_layer, outline_shader, 'u_pixel_size', 1/480, 1/270)

-- Apply shader to layer (deferred, ping-pong)
layer_apply_shader(outline_layer, outline_shader)
layer_apply_shader(shadow_layer, shadow_shader)

-- Manual layer compositing with offset (for shadow positioning)
layer_draw(bg_layer)
layer_draw(shadow_layer, 4, 4)  -- offset shadow by 4 pixels
layer_draw(outline_layer)
layer_draw(game_layer)
```

**Per-object flash (vertex attribute):**

Flash is baked into the uber-shader via a per-vertex additive color attribute (`vAddColor`). This avoids shader swapping, so many flashing objects stay in one batched draw call. The vertex format is 16 floats: x, y, u, v, r, g, b, a, type, shape[4], addR, addG, addB.

**Layer-level vs per-object effects:**

- **Layer effects** (outline, shadow, CRT) are post-processing — they can sample neighboring pixels because they run after all objects are drawn to the layer.
- **Per-object effects** (flash) run during object drawing — they can only access the object's own texture, not neighbors.

### Colors

Colors are 32-bit RGBA values (e.g., `0xFF0000FF` for red). A built-in palette is available:

```lua
an.colors.white
an.colors.black
an.colors.red
-- etc.
```

### Draw Order

Draw order within a layer is submission order (when Lua calls draw functions), not tree order.

### WebGL Compatibility

OpenGL 3.3 Core Profile maps cleanly to WebGL 2.0. Avoid geometry shaders, compute shaders, and extension-dependent features.

---

## Error Handling

LÖVE-style protected calls. Errors display stacktrace on screen; application continues running in error state.

```c
if (lua_pcall(L, nargs, nresults, error_handler) != LUA_OK) {
    strncpy(error_message, lua_tostring(L, -1), sizeof(error_message));
    app_state = STATE_ERROR;
}
```

No restart on keypress — user closes and reopens the application.

---

## Build and Distribution

### Resolution

Fixed resolution per game (480×270 or 640×360), scaled to fill window while maintaining aspect ratio with letterboxing. Nearest-neighbor filtering preserves pixel look.

### Build Scripts

- **Windows:** `build.bat` — compiles anchor.c, links against static libraries
- **Web:** `build-web.bat` (Emscripten)
- **Dependencies:** CMake used to build SDL2 as a static library (one-time setup)

### Static Linking

All libraries are statically linked to produce a single executable with no DLL dependencies:
- **SDL2** — built from source as static library
- **Lua** — compiled directly into the engine
- **Audio library** — (TBD) will also be static

### Distribution

**Single executable** with all game content embedded. No external files, no DLLs.

**Packaging method:** Zip-append. Game content (Lua files, assets) is packed into a zip archive and appended to the executable. The engine reads itself, finds the zip at the end, and loads content from there. Standard zip tools (7-Zip, WinRAR) can open the exe and extract content. During development, content loads from disk; release builds use the appended zip.

---

## File Structure

```
Anchor/
├── .claude/                # Claude Code config
├── docs/                   # Documentation (ANCHOR.md, etc.)
├── engine/                 # Engine code + builds
│   ├── src/
│   │   └── anchor.c        # Single monolithic C file
│   ├── include/            # Vendored headers (SDL2, Lua, glad, stb)
│   ├── lib/                # Vendored libraries (SDL2.lib)
│   ├── build/              # Windows build output (anchor.exe)
│   ├── build-web/          # Web build output (anchor.html, etc.)
│   ├── build.bat           # Windows build script
│   ├── build-web.sh        # Web build script (takes game folder arg)
│   ├── run-web.bat         # Run web build locally
│   └── shell.html          # Emscripten HTML template
├── test/                   # Test game folder
│   ├── main.lua            # Test entry point
│   └── assets/             # Test assets (images, sounds)
├── reference/              # Reference materials
│   ├── love-compare/       # LÖVE comparison project
│   └── *.md, *.yue         # Notes and examples
├── scripts/                # Utility scripts
└── website/                # Blog/website (pushed to Blot)
```

### Running Games

The engine takes a game folder as argument (like LÖVE):

```bash
# Windows
./engine/build/anchor.exe test

# Web (bundles game folder at build time)
./engine/build-web.sh ../test
```

Game folders contain `main.lua` at root, with assets in subdirectories.

---

## Performance Path

Start with everything in Lua/YueScript. Measure. If bottlenecks appear:

1. Move hot loops to C (timer updates, spring updates)
2. Keep tree structure in Lua
3. Lua objects become thin wrappers around C resources

The tree stays in Lua. Only inner loops move to C based on profiling data.

---

## Deferred Features

Not implementing now:

- Hot reloading
- Replay system
- Visual debugger
- Console/REPL
- Positional audio (panning, 3D)
- SDF text rendering (for website)
- High-resolution text layer
