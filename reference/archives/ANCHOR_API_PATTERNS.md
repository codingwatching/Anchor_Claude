# Anchor API Patterns (Speculative)

**Note:** These patterns were designed against a theoretical API that hasn't been implemented yet. They will need to be revised once the actual API is built. Archived here for reference.

---

## Code Patterns

### Comment Style

Use minimal single-line comments. Avoid multi-line decorative banners:

```c
// Bad
//----------------------------------------------------------
// Layer
//----------------------------------------------------------

// Good
// Layer
```

### Single-Letter Aliases

Anchor provides single-letter aliases that look like runes:

```lua
E = object                                        -- Entity/object
U = function(name_or_fn, fn) ... end              -- U(fn) or U('name', fn)
L = function(name_or_fn, fn) ... end              -- L(fn) or L('name', fn)
X = function(name, fn) return {[name] = fn} end   -- eXplicit/named
-- A is a method alias: self:A('tag') == self:all('tag')
```

In YueScript:
```yuescript
E = object
U = (name_or_fn, fn) -> ...  -- U(fn) or U('name', fn)
L = (name_or_fn, fn) -> ...  -- L(fn) or L('name', fn)
X = (name, fn) -> {[name]: fn}
```

**Future single-letter aliases** should prefer these characters, chosen for their angular, runic appearance (symmetrical, minimal roundness):

```
E, X, A, T, L, V, U, Y, I, H
```

### Object Construction

```lua
thing = class:class_new(object)
function thing:new(x, y, args)
    self:object('thing', args)
    self.x, self.y = x, y

    -- Add children
    self + collider('thing', 'dynamic', 'circle', 10)
    self + spring(1, 200, 10)
end

function thing:update(dt)
    self.x, self.y = self.collider:get_position()

    game:push(self.x, self.y, 0, self.spring.x, self.spring.x)
        game:circle(self.x, self.y, 10, self.color)
    game:pop()
end
```

### Inline Objects

```lua
-- With operators and single-letter alias
E() ^ {x = 100, y = 100}
   / function(self, dt)
         self.x = self.x + 50 * dt
         game:circle(self.x, self.y, 10, color)
     end
   >> arena
```

### Collision Handling (Rules-Based, in Arena)

Box2D 3.1 provides two event types:
- **Sensor events** — overlap detection, no physics response (for triggers, pickups)
- **Contact events** — physical collision with response (for walls, bouncing)

```lua
function arena:update(dt)
    -- Physical collisions (objects bounce)
    for _, c in ipairs(an:physics_get_contact_enter('player', 'enemy')) do
        local player, enemy = c.a, c.b
        player:take_damage(enemy.damage)
    end

    -- Sensor overlaps (objects pass through)
    for _, c in ipairs(an:physics_get_sensor_enter('player', 'pickup')) do
        local player, pickup = c.a, c.b
        pickup:collect()
    end

    -- High-speed impacts (for sounds/particles)
    for _, c in ipairs(an:physics_get_contact_hit('ball', 'wall')) do
        local speed = c.approach_speed
        an:sound_play(bounce_sound, {volume = speed / 100})
    end
end
```

### Timers and Springs

```lua
-- Timer is a child object
self + timer()

-- Timer methods: after(duration, [name], callback)
self.timer:after(0.5, function() self:explode() end)                    -- Anonymous
self.timer:after(0.15, 'flash', function() self.flashing = false end)   -- Named

-- Tween (provided by timer)
self.timer:tween(0.3, self, {x = 100}, math.cubic_out)

-- Springs are child objects (each spring object IS one spring)
self + spring(1, 200, 10)   -- initial, stiffness, damping (defaults to name 'spring')
self.spring:pull(0.5)

-- Use spring in drawing
game:push(self.x, self.y, 0, self.spring.x, self.spring.x)
```

### Death

```lua
-- kill() recursively sets .dead = true on self and all children
-- Actual removal happens at end-of-frame
self:kill()
```

---

## Naming Conventions

- Engine API: `an:something()`
- Layers: `game`, `effects`, `ui`
- Containers: named for contents (`enemies`, `projectiles`)
- Timer/tween tags: describe what they prevent (`'movement'`, `'attack_cooldown'`)

---

## File Organization

- Use `--{{{ }}}` fold markers for sections
- Keep files under ~1000 lines
- Single file per major system until unwieldy
- Comment non-obvious code

---

## Key Technical Decisions

### Engine vs Objects

**Engine (`an`):** C-backed services. Physics, rendering, input, audio, RNG. Always available, don't die.

**Objects:** Lua tables in a tree. Have state, update, die. When parent dies, children die immediately.

### Timers, Springs, Colliders

These are **engine objects** — child objects that wrap C-side resources. They die when their parent dies. No manual cleanup tracking.

Engine objects are named after themselves by default, so `@ + timer()` creates a child named `'timer'`, accessible via `@timer`. This pattern applies to all engine objects (timers, springs, colliders, and any future ones).

```lua
-- Engine objects as children (default names)
self + timer()                          -- Creates self.timer
self + spring(1, 200, 10)               -- Creates self.spring
self + collider('player', 'dynamic', 'circle', 10)  -- Creates self.collider

-- Multiple of same type (explicit names)
self + spring('attack', 1, 200, 10)     -- Creates self.attack
self + spring('hit', 1, 300, 15)        -- Creates self.hit
```

### Layers

Layers are **engine infrastructure**, created at startup. They're not tree objects. They don't die.

```lua
game = an:layer('game')        -- Once, at init
game:circle(x, y, r, color)    -- Use anywhere
```

### Draw Order

Draw order within a layer is **submission order** (when Lua calls draw functions), not tree order. This keeps drawing flexible — an object can draw to multiple layers, in any order.

### C/Lua Bindings

C exposes **plain functions** that take and return simple values or raw pointers (lightuserdata). No metatables, no userdata with methods, no global tables on the C side.

```lua
-- Raw C bindings (dumb, minimal)
local layer = layer_create('game')
layer_rectangle(layer, 10, 10, 50, 50, 0xFF0000FF)
layer_circle(layer, 100, 100, 25, rgba(255, 128, 0, 255))
```

The nice OOP API (`game:rectangle(...)`) is built later in YueScript on top of these primitives. This keeps the C side simple and puts the abstraction in YueScript where it belongs.

```yuescript
-- YueScript wrapper (built on raw bindings)
class Layer
  new: (name) => @_ptr = layer_create(name)
  rectangle: (x, y, w, h, color) => layer_rectangle(@_ptr, x, y, w, h, color)

game = Layer 'game'
game\rectangle 10, 10, 50, 50, 0xFF0000FF
```

---

## Common Mistakes to Avoid

1. **Creating layers as tree objects** — Layers are engine infrastructure, not gameplay objects

2. **Expecting tree order to control draw order** — It doesn't. Submission order does.

3. **Manual cleanup of timers/springs** — Not needed. They're children, they die with parent.

4. **Checking `if self.parent.dead`** — Not needed. If parent died, you're already dead.

5. **Confusing sensors vs contacts** — Sensors are for overlap detection (triggers), contacts are for physical collisions (bouncing).

6. **Redundant owner/target fields** — If an object is a child, use `@parent` to reference the parent. Don't create redundant fields like `@owner = parent` in the constructor.

7. **Named actions in class constructors** — For class-based objects, prefer class methods (`update:`, `draw:`) over named actions created in constructors (`@ / X 'update', ...`). Named actions with X are for inline anonymous objects or behavior that needs to be dynamically added/removed at runtime.

8. **Using link when tree would work** — If the dependency is vertical (one object conceptually belongs to another), use the tree. Link is for horizontal dependencies where tree structure doesn't fit.

### Tree Limitations

The tree handles parent-child dependendies automatically (children die with parents, `@parent` provides references). However, sibling or cousin dependencies - like an enemy checking if the player is dead - still require explicit checks. This isn't a failure to use the system; horizontal relationships aren't captured by vertical tree structure.

---

## Keeping Documentation Updated

**This is critical.** These documents must stay current as the engine is implemented.

After any session that results in:
- New patterns or conventions
- Changed APIs
- Architectural decisions
- Implementation details that future Claude instances need to know

**Remind the developer** to update `ANCHOR.md` and/or `CLAUDE.md`. Proactively suggest specific additions or changes.

The **Patterns** section below should grow as we implement the engine. When we establish how something actually works in practice (not just in theory), add it here with concrete code examples.

### What to Update Where

**ANCHOR.md:**
- Technical architecture changes
- API changes (what functions exist, what they do)
- New engine services or object types
- Changes to the object tree model
- Rendering, physics, audio implementation details

**CLAUDE.md:**
- New code patterns with examples
- New conventions or naming rules
- Changed working style preferences
- New common mistakes discovered
- Updates to the Patterns section below

---

## Patterns

*This section will grow as we implement the engine. Add concrete, tested patterns here — not theoretical ones.*

### Inline Single-Use Code

If a function is only called once, inline it instead of extracting it. This preserves locality — you see everything in one place without jumping around.

```lua
-- Bad: splits up code that's only used in one place
function arena:new(args)
    self:object('arena', args)
    self:setup_physics()
    self:setup_input()
    self:create_objects()
end

-- Good: everything visible in constructor
function arena:new(args)
    self:object('arena', args)

    -- Physics setup
    an:physics_set_gravity(0, 0)
    an:physics_enable_contact_between('player', {'enemy', 'wall'})
    an:physics_enable_sensor_between('player', {'pickup'})

    -- Input setup
    an:input_bind('move_left', {'key:a', 'key:left'})
    an:input_bind('move_right', {'key:d', 'key:right'})

    -- Create objects
    self + player(240, 135)
         + { wall('top'), wall('bottom'), goal('left'), goal('right') }
end
```

### Inline Objects with Operators

The operator chain reads left-to-right: create → configure → add behavior → flow to parent.

**With property table:**
```lua
E('ball') ^ {x = 240, y = 135, vx = 100, vy = 100}
          / function(self, dt)
                self.x = self.x + self.vx * dt
                game:circle(self.x, self.y, 8, an.colors.white)
            end
          >> arena

-- Later: arena.ball exists
-- Adding another 'ball' kills the old one
```

**With build function (for method calls):**
```lua
E() ^ function(self)
          self.x, self.y = 100, 200
          self + timer()
      end
    / function(self, dt)
          game:circle(self.x, self.y, 10, color)
      end
    >> arena
```

**With multiple action phases:**
```lua
E() ^ {x = 100, y = 200}
   / U(function(self, dt)
         -- early: before main updates
     end)
   / function(self, dt)
         -- main update
     end
   / L(function(self, dt)
          -- late: after main updates, good for drawing
      end)
   >> arena
```

**Multiple `^` operators:** When chaining multiple `^`, group all initialization before action operators:
```lua
-- Good: all init together, then actions
E() ^ {x = 100, y = 200}
   ^ function(self) self + timer() end
   / function(self, dt) ... end
   >> arena

-- Bad: init split across the chain
E() ^ {x = 100}
   / function(self, dt) ... end
   ^ function(self) self + timer() end -- init after action, harder to follow
   >> arena
```

### Named vs Anonymous

The tree automatically manages named children: adding a child with a name that already exists kills the old child first. This provides cancellation and replacement without explicit tag systems.

**Objects:**
```lua
-- Anonymous
E() ^ {x = 100} >> arena

-- Named (arena.ball points to this)
E('ball') ^ {x = 100} >> arena
```

**Timer callbacks:**
```lua
-- Anonymous
self.timer:after(0.5, function() self.can_shoot = true end)

-- Named (self.timer.flash points to this, can be killed/replaced)
self.timer:after(0.15, 'flash', function() self.flashing = false end)

-- Cancel
if self.timer.flash then self.timer.flash:kill() end

-- Replace (old 'flash' killed automatically)
self.timer:after(0.15, 'flash', function() self.flashing = false end)
```

**Actions:**
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

-- Cancel
self.water_sim:kill()
```

### Hierarchy Building with `+`

Use the `+` operator to build object hierarchies declaratively (parent-centric style):

```lua
-- In constructor
function arena:new(args)
    self:object('arena', args)

    self + timer()
         + paddle('left', 30, 135)
         + paddle('right', 450, 135)
         + ball()
         + { wall('top', 240, 5),
             wall('bottom', 240, 265),
             goal('left', -5, 135),
             goal('right', 485, 135) }
end

-- Dynamically adding during gameplay
function arena:spawn_enemy(x, y)
    self + enemy(x, y)
end

-- Multiple at once
function arena:spawn_wave()
    self + {
        enemy(100, 100),
        enemy(200, 100),
        enemy(300, 100),
    }
end
```

### Pipeline Style with `>>`

Use `>>` for object-centric pipeline construction:

```lua
-- Object flows into parent
E('ball') ^ {x = 100, y = 200}
          / function(self, dt)
                game:circle(self.x, self.y, 10, color)
            end
          >> arena
```

**Choosing between `+` and `>>`:**
- `+` is parent-centric: "arena gains these children"
- `>>` is object-centric: "this object flows into arena"

Use `+` for adding multiple siblings, `>>` for inline object construction.

### Mixing `+` with Inline Objects

When combining `+` with inline object construction, parentheses are needed:

```lua
arena + paddle('left', 30, 135)
      + paddle('right', 450, 135)
      + (E('ball') ^ {x = 240, y = 135}
                   / function(self, dt)
                         game:circle(self.x, self.y, 8, an.colors.white)
                     end)
```

### Bulk Operations with `all()` or `A()`

Use `all()` or the `A()` alias to operate on multiple objects:

```lua
-- Iteration for method calls or property setting
for _, enemy in ipairs(arena:A('enemy')) do
    enemy:take_damage(10)
end

-- Count checks
if #arena:A('enemy') == 0 then
    arena:next_wave()
end

-- Combined with conditionals
for _, item in ipairs(arena:A('item')) do
    if item.rarity == 'legendary' then
        item:sparkle()
    end
end
```

### Named Timers Pattern

Use named timer callbacks when you need to cancel or replace an ongoing timer:

```lua
function enemy:flash()
    self.flashing = true
    self.timer:after(0.15, 'flash', function()
        self.flashing = false
    end)
end

-- Calling flash() again replaces the old callback
-- So rapid hits extend the flash rather than stacking callbacks

function enemy:stun(duration)
    -- Cancel any existing stun
    if self.timer.stun then self.timer.stun:kill() end

    self.stunned = true
    self.timer:after(duration, 'stun', function()
        self.stunned = false
    end)
end
```

### Named Actions Pattern

Use named actions with the `U`, `L`, and `X` helpers when you need to cancel or replace ongoing behavior:

```lua
function arena:new()
    self:object('arena')

    -- Named early action
    self / U('water_sim', function(self, dt)
        -- propagate spring velocities
    end)

    -- Named late action
    self / L('water_draw', function(self, dt)
        -- build water surface polyline
    end)
end

function arena:pause_water()
    if self.water_sim then self.water_sim:kill() end
    if self.water_draw then self.water_draw:kill() end
end

function arena:resume_water()
    self / U('water_sim', function(self, dt)
        -- propagate spring velocities
    end)
    self / L('water_draw', function(self, dt)
        -- build water surface polyline
    end)
end
```

**YueScript equivalent:**
```yuescript
class arena extends object
  new: =>
    super 'arena'

    @ / U 'water_sim', (dt) =>
      -- propagate spring velocities

    @ / L 'water_draw', (dt) =>
      -- build water surface polyline

  pause_water: =>
    @water_sim\kill! if @water_sim
    @water_draw\kill! if @water_draw
```

### Horizontal Links

For sibling or cousin dependencies that the tree can't express:
```lua
-- Enemy dies when player dies (siblings under arena)
function enemy:new(x, y)
    self:object()
    self:link(self.arena.player)
end

-- With callback for custom death behavior
function enemy:new(x, y)
    self:object()
    self:link(self.arena.player, function()
        self:flee()  -- scatter instead of dying
    end)
end

-- Tether between two units (can't be child of both)
function tether:new(a, b)
    self:object()
    self:link(a)
    self:link(b)
end
```

**Don't use link when tree works:**
```lua
-- Bad: shield could just be a child
self:link(owner)

-- Good: use the tree
owner + shield()
```

*When implementing each system, replace the placeholder with the actual working pattern, including any gotchas or non-obvious details discovered during implementation.*
