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
11. [Engine Services (C Side)](#engine-services-c-side)
12. [Rendering Architecture](#rendering-architecture)
13. [Software Renderer Implementation](#software-renderer-implementation)
14. [Error Handling](#error-handling)
15. [Resolution and Scaling](#resolution-and-scaling)
16. [Build System](#build-system)
17. [File Structure](#file-structure)
18. [Performance Path](#performance-path)
19. [Migration Path](#migration-path)
20. [Deferred Features](#deferred-features)

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
- **Globals are available:** `an`, `E`, `X`, `A` are just there. No imports needed. You can use them from anywhere, in any file, without setup.
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
2. **Early actions** (`%` operator) — Run all early_action functions (tree order)
3. **Main actions** (`/` operator) — Run all action functions (tree order)
4. **Late actions** (`//` operator) — Run all late_action functions (tree order)
5. **Cleanup** — Remove dead objects, call destroy() hooks

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

### Collider

A collider object manages a Box2D physics body. Created via the `collider` factory function:

```lua
-- @ + collider(tag, body_type, shape_type, ...)
self + collider('player', 'dynamic', 'circle', 12)
self + collider('wall', 'static', 'rectangle', 100, 20)
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

A spring object provides damped spring animation. Created via the `spring` factory function:

```lua
-- Each spring object IS a spring
self + spring('main', 1, 200, 10)   -- name, initial, stiffness, damping
self + spring('shoot', 1, 300, 15)

-- Usage
self.main:pull(0.5)   -- Displace from target
self.main.x           -- Current value (updated each frame)
```

Each spring object represents one spring with its own value.

### Timer

A timer object provides delayed and repeating callbacks, plus tweening. Created via the `timer` method:

```lua
-- Enable timer functionality (adds a timer object as child)
self:timer()

-- One-shot (anonymous)
self:after(0.5, function() self.can_shoot = true end)

-- One-shot (named — can be cancelled/replaced)
self:after(0.15, 'flash', function() self.flashing = false end)

-- Repeating (anonymous)
self:every(2, function() self:spawn_enemy() end)

-- Repeating (named)
self:every(1, 'regen', function() self.hp = self.hp + 1 end)

-- Cancel a named timer
self.flash:kill()

-- Tween (also provided by timer)
self:tween(0.5, self, {x = 100, y = 200}, math.cubic_in_out, function()
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
    
    self.main = self:add(spring('main', 1, 200, 10))
    self.main:pull(0.5)
    
    self:tween(self.duration, self, {v = 0, sx = 0, sy = 0}, math.linear, function()
        self:kill()
    end)
end

function emoji_particle:update(dt)
    self.x = self.x + self.v * math.cos(self.r) * dt
    self.y = self.y + self.v * math.sin(self.r) * dt
    effects:draw_image(self.emoji, self.x, self.y, self.r, self.sx * self.main.x, self.sy)
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
    self.x, self.y = self.body:get_position()
    
    -- Drawing to game layer
    game:push(self.x, self.y, self.r, self.main.x, self.main.x)
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
| `%` | early_action | Add anonymous early action (runs before update) |
| `/` | action | Add anonymous action (runs every frame) |
| `//` | late_action | Add anonymous late action (runs after update) |
| `+` | add children | Add child objects to parent (parent-centric) |
| `>>` | flow to parent | Add self to parent, **returns parent** (object-centric) |

Operators are ordered by precedence (highest to lowest: `^`, then `%` `/` `//`, then `+`, then `>>`), so they chain naturally left-to-right without parentheses.

### Single-Letter Aliases

For elegance, Anchor provides three single-letter aliases that look like runes:

| Alias | Purpose | Mnemonic |
|-------|---------|----------|
| `E` | Object constructor | Entity |
| `X` | Named action helper | eXplicit |
| `A` | Query descendants | All |

These are defined in the engine:

```lua
E = object
X = function(name, fn) return {[name] = fn} end
-- A is a method alias: self:A('tag') == self:all('tag')
```

**Future single-letter aliases** should prefer these characters, chosen for their angular, runic appearance (symmetrical, minimal roundness):

```
E, X, A, T, L, V, U, Y, I, H
```

### Named Actions with Operators: The `X` Helper

By default, operators create anonymous actions. The `X` helper enables named actions with operators by wrapping the name and function in a table that the operator detects:

```lua
-- Anonymous action (no name, can't be cancelled)
self / function(self, dt) ... end

-- Named action using X helper (self.seek points to this)
self / X('seek', function(self, dt) ... end)

-- Cancel later
self.seek:kill()
```

The pattern works with all action operators:

```lua
self % X('water_sim', early_fn)   -- named early action
self / X('update', update_fn)     -- named action
self // X('draw', draw_fn)        -- named late action
```

This allows named actions to participate in operator pipelines:

```lua
E() ^ {x = 100, y = 200}
   / X('move', function(self, dt)
         self.x = self.x + 50 * dt
     end)
   // X('draw', function(self, dt)
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
               self:timer()
           end
```

### The Action Operators: `%`, `/`, `//`

Three operators for the three update phases. All create anonymous actions by default.

| Operator | Phase | When it runs |
|----------|-------|--------------|
| `%` | early_action | Before `update()`, before all regular actions |
| `/` | action | During `update()`, the main update phase |
| `//` | late_action | After `update()`, after all regular actions |

```lua
object() ^ {x = 100, y = 200}
         % function(self, dt)
               -- early: runs first
           end
         / function(self, dt)
               -- main update
               game:circle(self.x, self.y, 10, color)
           end
         // function(self, dt)
               -- late: runs last, good for drawing that needs final positions
           end
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

**Timers** are named via an optional second argument:
```lua
-- Anonymous
self:after(0.5, function() self.can_shoot = true end)

-- Named (self.flash points to this, can be killed/replaced)
self:after(0.15, 'flash', function() self.flashing = false end)
```

**Actions** can use operators with the `X` helper for named, or methods:
```lua
-- Anonymous (operators without X)
E() % early_fn / update_fn // late_fn >> arena

-- Named with X helper (preferred for operator pipelines)
self / X('seek', fn)
self % X('water_sim', fn)
self // X('draw', fn)

-- Named with methods (alternative)
self:action('update', fn)
self:early_action('water_sim', fn)
self:late_action('water_draw', fn)

-- Cancel later
self.water_sim:kill()
```

The `X` helper is simply:
```lua
X = function(name, fn) return {[name] = fn} end
```

The operator detects this pattern (single string key + function value) and creates a named action.

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
                game:circle(self.x, self.y, 8, an.colors.white[0])
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
                    game:circle(self.x, self.y, 8, an.colors.white[0])
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
    :timer()
```

---

## YueScript Alternative

*YueScript (evolved from MoonScript) is a language that compiles to Lua. It offers significant syntactic improvements: significant whitespace, `@` for self, `=>` for methods with implicit self, no `end` keywords, `+=`/`-=` operators, and optional parentheses. The operator-based object construction style fits YueScript's aesthetic particularly well.*

### Single-Letter Aliases in YueScript

The same three single-letter aliases work in YueScript:

```yuescript
E = object                       -- Entity/object constructor
X = (name, fn) -> {[name]: fn}   -- named action helper (eXplicit)
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
             game\circle @x, @y, 8, an.colors.white[0]
         >> arena

-- Later: arena.ball refers to this object
```

**With build function:**
```yuescript
E! ^ =>
       @x, @y = 100, 200
       @\timer!
   / (dt) =>
       @x += 50 * dt
       game\circle @x, @y, 10, color
   >> arena
```

**Named actions with X helper:**
```yuescript
class arena extends object
  new: =>
    super 'arena'
    @water_springs_count = 52
    
    -- Named early action
    @ % X 'water_sim', (dt) =>
      for k = 1, 8
        for i = 1, @water_springs_count
          -- propagate spring velocities
    
    -- Named late action
    @ // X 'water_draw', (dt) =>
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
    @ // X 'draw', (dt) =>
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
                  / (dt) => game\circle @x, @y, 8, an.colors.white[0])
```

**Spawning with automatic replacement:**
```yuescript
arena.spawn_ball = =>
  E 'ball' ^ {x: 240, y: 135, vx: 100, vy: 100}
           / (dt) =>
               @x += @vx * dt
               @y += @vy * dt
               game\circle @x, @y, 8, an.colors.white[0]
           >> @
  -- @ball now exists due to named child auto-linking

-- Each call replaces the previous ball
arena\spawn_ball!
arena\spawn_ball!  -- old ball killed, new ball created
```

### Side-by-Side Comparison

**Lua (15 lines):**
```lua
E() ^ function(self)
          self.x, self.y = x, y
          self.v = an:random_float(50, 100)
          self.r = an:random_angle()
          self.duration = 0.5
          self:timer()
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

**YueScript (13 lines):**
```yuescript
E! ^ =>
       @x, @y = x, y
       @v = an\random_float 50, 100
       @r = an\random_angle!
       @duration = 0.5
       @\timer!
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
    for mm in *{'__pow', '__mod', '__div', '__idiv', '__add', '__shr'}
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
  add_child: (child) =>
    table.insert @children, child
    child.parent = @
    -- Create named links
    if child.name
      @[child.name] = child
    if @name
      child[@name] = @
    @
  
  remove_child: (child) =>
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
  
  -- Timers
  after: (delay, name_or_fn, fn) =>
    @\timer! unless @timers
    if type(name_or_fn) == 'string'
      name, func = name_or_fn, fn
      @[name]\kill! if @[name] and @[name].kill
      timer_obj = {name: name, delay: delay, elapsed: 0, fn: func, kill: -> @dead = true}
      @[name] = timer_obj
      table.insert @timers, timer_obj
    else
      table.insert @timers, {delay: delay, elapsed: 0, fn: name_or_fn}
    @
  
  every: (interval, name_or_fn, fn) =>
    @\timer! unless @timers
    if type(name_or_fn) == 'string'
      name, func = name_or_fn, fn
      @[name]\kill! if @[name] and @[name].kill
      timer_obj = {name: name, interval: interval, elapsed: 0, fn: func, repeating: true, kill: -> @dead = true}
      @[name] = timer_obj
      table.insert @timers, timer_obj
    else
      table.insert @timers, {interval: interval, elapsed: 0, fn: name_or_fn, repeating: true}
    @
  
  -- Operators
  __pow: (other) =>
    if type(other) == 'function'
      other @
    elseif type(other) == 'table'
      for k, v in pairs other
        @[k] = v
    @
  
  __mod: (other) =>
    if type(other) == 'function'
      @\early_action other
    elseif type(other) == 'table'
      name, fn = next other
      @\early_action name, fn if type(name) == 'string' and type(fn) == 'function'
    @
  
  __div: (other) =>
    if type(other) == 'function'
      @\action other
    elseif type(other) == 'table'
      name, fn = next other
      @\action name, fn if type(name) == 'string' and type(fn) == 'function'
    @
  
  __idiv: (other) =>
    if type(other) == 'function'
      @\late_action other
    elseif type(other) == 'table'
      name, fn = next other
      @\late_action name, fn if type(name) == 'string' and type(fn) == 'function'
    @
  
  __add: (other) =>
    if type(other) == 'table'
      if other.name != nil or other.children != nil  -- It's an object
        @\add_child other
      else  -- It's an array
        for child in *other
          @\add_child child
    @
  
  __shr: (other) =>
    other\add_child @
    other  -- Returns PARENT

-- Set up aliases
E = object
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

No registration needed. The object system handles dispatch.

### Colliders and Springs Are Factory Functions

Colliders and springs are created by factory functions that return objects with specialized functionality:

```yuescript
-- collider is a factory function that returns an object table
collider = (tag, body_type, shape_type, ...) ->
  {
    tag: tag
    body_id: an\physics_create_body tag, body_type, shape_type, ...
    get_position: => an\physics_get_position @body_id
    set_velocity: (vx, vy) => an\physics_set_velocity @body_id, vx, vy
    kill: => an\physics_destroy_body @body_id
    -- etc.
  }

-- Usage: added as child via + operator
@ + collider 'player', 'dynamic', 'circle', 12
```

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
- **Window/Input/Audio:** SDL2, SDL_mixer
- **Graphics:** Software renderer (CPU-based, 480×270 or 640×360 target resolution)
- **Physics:** Box2D 3.1
- **Platforms:** Windows (primary), Web (Emscripten)

---

## Engine Services (C Side)

### Physics

Box2D 3.1 provides two types of shape interactions:

**Sensors** (overlap detection, no physics response):
- Objects pass through each other
- Used for triggers, detection zones, pickups

**Contacts** (physical collision with response):
- Objects bounce off each other
- Used for walls, projectiles, physical interactions

```lua
an:physics_set_gravity(0, 500)

-- Configure collision categories
an:physics_enable_sensor_between('player', {'pickup', 'trigger_zone'})
an:physics_enable_contact_between('ball', {'wall', 'paddle'})

-- Per-frame event queries
for _, event in ipairs(an:physics_get_sensor_enter('player', 'pickup')) do
    local player, pickup = event.a, event.b
    pickup:collect()
end

for _, event in ipairs(an:physics_get_contact_enter('ball', 'wall')) do
    local ball, wall = event.a, event.b
    -- Play bounce sound
end

for _, event in ipairs(an:physics_get_contact_hit('ball', 'wall')) do
    local speed = event.approach_speed
    -- Play impact sound scaled to speed
end

-- On-demand queries
local enemies = an:physics_query_circle(x, y, radius, {'enemy'})
local hit = an:physics_raycast_closest(x1, y1, x2, y2, {'wall', 'enemy'})
local all_hits = an:physics_raycast(x1, y1, x2, y2, {'wall'})
local in_area = an:physics_query_aabb(x, y, w, h, {'pickup'})

-- Current sensor overlaps
local overlaps = an:physics_get_sensor_overlaps(sensor_collider)
```

### Input

```lua
an:input_bind('shoot', {'mouse:1', 'key:space', 'gamepad:a'})
an:input_bind('move_left', {'key:a', 'key:left', 'gamepad:leftx-'})

if an:is_pressed('shoot') then self:shoot() end
if an:is_down('move_left') then self.vx = -self.speed end
```

### Audio

```lua
local shoot_sound = an:sound_load('shoot.ogg')
an:sound_play(shoot_sound)
an:sound_volume(shoot_sound, 0.5)

local bgm = an:music_load('bgm.ogg')
an:music_play(bgm)
an:music_volume(0.5)
```

### Random (Deterministic)

```lua
an:random_seed(12345)
local x = an:random_float(0, 100)
local i = an:random_int(1, 10)
local r = an:random_angle()
local s = an:random_sign()
```

### Rendering

```lua
-- Layers created at startup
game = an:layer('game')
effects = an:layer('effects')

-- Drawing primitives
game:circle(x, y, radius, color)
game:rectangle(x, y, w, h, rx, ry, color)
game:line(x1, y1, x2, y2, color, width)
game:draw_image(image, x, y, r, sx, sy, ox, oy, color)
game:draw_text(text, font, x, y, color)

-- Transform stack
game:push(x, y, r, sx, sy)
game:pop()
```

### Text Rendering

TTF fonts are loaded at specific sizes and baked to texture atlases:

```lua
an:font_load('default', 'assets/fonts/myfont.ttf', 24)  -- name, path, size
an:font_load('large', 'assets/fonts/myfont.ttf', 48)    -- same TTF, different size
an:font_get_text_width('default', 'HP: 3')
layer:draw_text('HP: 3', 'default', x, y)
layer:draw_text('99', 'large', x, y, r, sx, sy)  -- with optional transform
```

---

## Rendering Architecture

### Software Renderer

Anchor uses a CPU-based software renderer targeting 480×270 or 640×360 resolution (depending on game). This approach provides direct pixel manipulation, simpler debugging, and consistent behavior across platforms.

### Layers = Framebuffers

Each layer is a framebuffer. Draw commands are batched per layer and flushed at end of frame:

1. Lua calls `layer:circle()`, `layer:draw_image()`, etc.
2. Each call appends to that layer's command buffer
3. At end of frame, C processes all commands in a tight loop
4. Effects are applied per-layer (C functions, configured from Lua)
5. Layers composite to screen via alpha blending

### Sprites

Sprites use precomputed rotations (16 or 32 angles) with runtime scaling. This trades memory for computation and creates a distinctive aesthetic with discrete rotation angles.

### Effects

Post-processing effects are implemented in C and configured from Lua:

```lua
game:set_effect('outline', {color = colors.black, thickness = 1})
game:set_effect('brightness', {factor = 1.2})
game:set_effect('blur', {radius = 2})
game:clear_effect()
```

Available effects:
- `brightness` — Multiply RGB values
- `tint` — Blend toward a color
- `blur` — Box blur
- `outline` — Edge detection outline
- `grayscale` — Desaturate
- `posterize` — Reduce color levels
- `crt_scanlines` — Retro CRT effect
- `chromatic_aberration` — RGB channel offset
- `pixelate` — Blocky pixels

### Draw Order

Draw order within a layer is submission order (when Lua calls draw functions), not tree order.

---

## Software Renderer Implementation

This section provides detailed C implementation guidance for the software renderer.

### Core Data Structures

```c
typedef struct {
    uint32_t* pixels;
    int width, height, stride;
} Framebuffer;

typedef struct {
    float tx, ty;      // translation
    float cos_r, sin_r; // rotation (precomputed)
    float sx, sy;      // scale
} Transform;

typedef enum {
    CMD_PUSH, CMD_POP,
    CMD_CIRCLE, CMD_RECTANGLE, CMD_LINE,
    CMD_SPRITE, CMD_TEXT
} CommandType;

typedef struct {
    CommandType type;
    union {
        struct { float x, y, r, sx, sy; } push;
        struct { float x, y, r; uint32_t color; } circle;
        struct { float x, y, w, h; uint32_t color; } rect;
        struct { float x, y, angle, sx, sy; int sprite_id; uint32_t tint; } sprite;
        // ... other command data
    };
} DrawCommand;

typedef struct {
    const char* name;
    Framebuffer* fb;
    DrawCommand* commands;
    int command_count, command_capacity;
    Transform transform_stack[32];
    int transform_depth;
    Transform current_transform;
    EffectType effect_type;
    EffectParams effect_params;
} Layer;
```

### Frame Flow

```
1. Clear all layer framebuffers
   for each layer:
       framebuffer_clear(layer->fb, 0x00000000)  // transparent black

2. Lua update runs
   - Objects call layer:push(), layer:circle(), layer:pop(), etc.
   - Each call appends to that layer's command buffer

3. Flush all layers (process commands, draw to framebuffers)
   for each layer:
       layer_flush(layer)  // tight C loop

4. Composite layers to screen
   for each layer in composition order:
       alpha_blend(screen, layer->fb)

5. Present screen to window
   SDL_UpdateTexture + SDL_RenderCopy
```

### Command Processing

```c
void layer_flush(Layer* layer) {
    layer->transform_depth = 0;
    layer->current_transform = transform_identity();
    
    for (int i = 0; i < layer->command_count; i++) {
        DrawCommand* cmd = &layer->commands[i];
        
        switch (cmd->type) {
            case CMD_PUSH: {
                layer->transform_stack[layer->transform_depth++] = layer->current_transform;
                Transform t = transform_make(cmd->push.x, cmd->push.y, 
                                             cmd->push.r, cmd->push.sx, cmd->push.sy);
                layer->current_transform = transform_compose(&layer->current_transform, &t);
                break;
            }
            case CMD_POP: {
                layer->current_transform = layer->transform_stack[--layer->transform_depth];
                break;
            }
            case CMD_CIRCLE: {
                float tx, ty;
                transform_point(&layer->current_transform, cmd->circle.x, cmd->circle.y, &tx, &ty);
                float tr = cmd->circle.r * transform_get_scale(&layer->current_transform);
                draw_circle_fill(layer->fb, (int)tx, (int)ty, (int)tr, cmd->circle.color);
                break;
            }
            // ... other commands
        }
    }
    
    if (layer->effect_type != EFFECT_NONE) {
        effect_apply(layer->fb, layer->effect_type, &layer->effect_params);
    }
    
    layer->command_count = 0;
}
```

### Sprite System with Precomputed Rotations

```c
typedef struct {
    Framebuffer* rotations[32];  // 32 angles (11.25° each)
    int base_width, base_height;
    int ox, oy;  // origin offset
} RotatedSprite;

RotatedSprite* sprite_create(Framebuffer* source, int ox, int oy) {
    RotatedSprite* s = malloc(sizeof(RotatedSprite));
    s->base_width = source->width;
    s->base_height = source->height;
    s->ox = ox;
    s->oy = oy;
    
    for (int i = 0; i < 32; i++) {
        float angle = i * (2.0f * M_PI / 32.0f);
        s->rotations[i] = rotate_framebuffer(source, angle);
    }
    return s;
}

void blit_rotated_scaled(Framebuffer* dst, RotatedSprite* sprite,
                         int x, int y, float angle, float sx, float sy, uint32_t tint) {
    // Quantize angle to nearest precomputed rotation
    int angle_index = ((int)(angle * 32.0f / (2.0f * M_PI)) + 32) % 32;
    Framebuffer* src = sprite->rotations[angle_index];
    
    // Scale and blit with tint
    blit_scaled_tinted(dst, src, x, y, sx, sy, tint);
}
```

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

## Resolution and Scaling

Fixed resolution per game (480×270 or 640×360), integer scaled with letterboxing. Nearest-neighbor filtering for crisp pixels.

---

## Build System

Simple build scripts, no CMake:

**Windows:** `build.bat`
**Web:** `build.sh` (Emscripten)

---

## File Structure

```
anchor/
├── src/                    # C source
│   ├── main.c
│   ├── an_render.c
│   ├── an_physics.c
│   ├── an_audio.c
│   ├── an_input.c
│   └── an_lua_bindings.c
├── yue/                    # YueScript engine code
│   ├── object.yue
│   ├── init.yue
│   └── ...
├── game/                   # Game code (YueScript)
│   └── main.yue
├── assets/
├── build.bat
└── build.sh
```

---

## Performance Path

Start with everything in Lua/YueScript. Measure. If bottlenecks appear:

1. Move hot loops to C (timer updates, spring updates)
2. Keep tree structure in Lua
3. Lua objects become thin wrappers around C resources

The tree stays in Lua. Only inner loops move to C based on profiling data.

---

## Migration Path

1. **Skeleton:** C main loop, SDL2 window, Lua init
2. **Rendering:** Software renderer, layers, command buffers, sprites with precomputed rotations
3. **Input:** SDL2 input with binding system
4. **Audio:** SDL_mixer sound and music
5. **Physics:** Box2D 3.1 wrapper with sensor/contact events and queries
6. **Random:** Deterministic PRNG
7. **Text:** TTF font loading and rendering
8. **Object system:** Tree with immediate kill propagation (YueScript)
9. **Timer/Spring/Collider:** As child objects
10. **Error handling:** Protected calls, error screen
11. **Test game:** Build Emoji Ball Battles to validate

Each step testable independently.

---

## Deferred Features

Not implementing now:

- Hot reloading
- Replay system
- Visual debugger
- Console/REPL
- Advanced audio (pitch shifting, positional)
- SDF text rendering (for website)
- High-resolution text layer
