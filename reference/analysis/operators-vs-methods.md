# Operators vs Short Methods Comparison

This document shows ANCHOR.md examples rewritten using single-letter method names.

---

## Method Aliases

| Operator | Method | Short |
|----------|--------|-------|
| `object 'name'` | `object 'name'` | `T 'name'` |
| `^ {props}` | `\set {props}` | `\Y {props}` |
| `^ => ...` | `\build => ...` | `\U => ...` |
| `/ U fn` | `\early_action fn` | `\E fn` |
| `/ fn` or `/ X 'n', fn` | `\action fn` | `\X fn` or `\X 'n', fn` |
| `/ L fn` | `\late_action fn` | `\L fn` |
| `+ child` | `\add child` | `\A child` |
| `>> parent` | `\flow_to parent` | `\F parent` |
| `\link target` | `\link target` | `\K target` |

---

## Core Philosophy Example

**With operators:**
```yuescript
E 'player' ^ {x: 100, y: 100, hp: 100}
  / X 'movement', (dt) =>
      @x += @vx * dt
      @y += @vy * dt
  / L 'draw', (dt) =>
      game\circle @x, @y, 10, colors.white
  >> arena
```

**With short methods:**
```yuescript
p = T 'player'
p\Y {x: 100, y: 100, hp: 100}
p\X 'movement', (dt) =>
  @x += @vx * dt
  @y += @vy * dt
p\L 'draw', (dt) =>
  game\circle @x, @y, 10, colors.white
p\F arena
```

---

## Simple Particle (Anonymous)

**With operators:**
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

**With short methods:**
```yuescript
p = T!
p\Y {x: x, y: y, r: an\random_angle!, duration: 0.5}
p\X (dt) =>
  @x += 50 * math.cos(@r) * dt
  @y += 50 * math.sin(@r) * dt
  @duration -= dt
  return true if @duration <= 0
  effects\circle @x, @y, 3, color
p\F arena
```

---

## Named Ball

**With operators:**
```yuescript
E 'ball' ^ {x: 240, y: 135, vx: 100, vy: 100}
         / (dt) =>
             @x += @vx * dt
             @y += @vy * dt
             game\circle @x, @y, 8, an.colors.white
         >> arena
```

**With short methods:**
```yuescript
b = T 'ball'
b\Y {x: 240, y: 135, vx: 100, vy: 100}
b\X (dt) =>
  @x += @vx * dt
  @y += @vy * dt
  game\circle @x, @y, 8, an.colors.white
b\F arena
```

---

## With Build Function

**With operators:**
```yuescript
E! ^ =>
       @x, @y = 100, 200
       @ + timer!
   / (dt) =>
       @x += 50 * dt
       game\circle @x, @y, 10, color
   >> arena
```

**With short methods:**
```yuescript
o = T!
o\U =>
  @x, @y = 100, 200
  @\A timer!
o\X (dt) =>
  @x += 50 * dt
  game\circle @x, @y, 10, color
o\F arena
```

---

## Named Actions (Water Simulation)

**With operators:**
```yuescript
class arena extends object
  new: =>
    super 'arena'
    @water_springs_count = 52

    @ / U 'water_sim', (dt) =>
      for k = 1, 8
        for i = 1, @water_springs_count
          -- propagate spring velocities

    @ / L 'water_draw', (dt) =>
      @water_surface = {}
      for spring in *@water_springs.children
        -- build polyline
```

**With short methods:**
```yuescript
class arena extends object
  new: =>
    super 'arena'
    @water_springs_count = 52

    @\E 'water_sim', (dt) =>
      for k = 1, 8
        for i = 1, @water_springs_count
          -- propagate spring velocities

    @\L 'water_draw', (dt) =>
      @water_surface = {}
      for spring in *@water_springs.children
        -- build polyline
```

---

## Player Class

**With operators:**
```yuescript
class player extends object
  new: (x, y, args) =>
    super 'player', args
    @x, @y = x, y

    @ + collider 'player', 'dynamic', 'circle', 12
    @ + spring 'main', 1, 200, 10
    @ + spring 'shoot', 1, 300, 15

    @ / X 'input', (dt) =>
      return if @stunned
      @vx = -@speed if an\is_down 'left'
      @vx = @speed if an\is_down 'right'

    @ / (dt) =>
      @x, @y = @collider\get_position!

    @ / L 'draw', (dt) =>
      game\push @x, @y, 0, @main.x, @main.x
      game\circle @x, @y, 12, colors.green
      game\pop!
```

**With short methods:**
```yuescript
class player extends object
  new: (x, y, args) =>
    super 'player', args
    @x, @y = x, y

    @\A collider 'player', 'dynamic', 'circle', 12
    @\A spring 'main', 1, 200, 10
    @\A spring 'shoot', 1, 300, 15

    @\X 'input', (dt) =>
      return if @stunned
      @vx = -@speed if an\is_down 'left'
      @vx = @speed if an\is_down 'right'

    @\X (dt) =>
      @x, @y = @collider\get_position!

    @\L 'draw', (dt) =>
      game\push @x, @y, 0, @main.x, @main.x
      game\circle @x, @y, 12, colors.green
      game\pop!
```

---

## Multiple Children

**With operators:**
```yuescript
arena + paddle 'left', 30, 135
      + paddle 'right', 450, 135
      + ball!
```

**With short methods:**
```yuescript
arena\A paddle 'left', 30, 135
arena\A paddle 'right', 450, 135
arena\A ball!
```

---

## Spawning with Replacement

**With operators:**
```yuescript
arena.spawn_ball = =>
  E 'ball' ^ {x: 240, y: 135, vx: 100, vy: 100}
           / (dt) =>
               @x += @vx * dt
               @y += @vy * dt
               game\circle @x, @y, 8, an.colors.white
           >> @
```

**With short methods:**
```yuescript
arena.spawn_ball = =>
  b = T 'ball'
  b\Y {x: 240, y: 135, vx: 100, vy: 100}
  b\X (dt) =>
    @x += @vx * dt
    @y += @vy * dt
    game\circle @x, @y, 8, an.colors.white
  b\F @
```

---

## Full Particle Example

**With operators (12 lines):**
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

**With short methods (14 lines):**
```yuescript
p = T!
p\U =>
  @x, @y = x, y
  @v = an\random_float 50, 100
  @r = an\random_angle!
  @duration = 0.5
p\X (dt) =>
  @x += @v * math.cos(@r) * dt
  @y += @v * math.sin(@r) * dt
  @duration -= dt
  return true if @duration <= 0
  effects\circle @x, @y, 3, color
p\F arena
```

---

## Summary

**Character count comparison (class constructor):**
```yuescript
@ + timer!      -- 10 chars (operator)
@\A timer!      -- 10 chars (short method)

@ / X 'input', fn   -- 15 chars (operator)
@\X 'input', fn     -- 15 chars (short method)

@ / U 'sim', fn     -- 13 chars (operator)
@\E 'sim', fn       -- 13 chars (short method)
```

**Visual comparison:**

| Operators | Short Methods |
|-----------|---------------|
| `object 'player'` | `T 'player'` |
| `o ^ {x: 1}` | `o\Y {x: 1}` |
| `o ^ => ...` | `o\U => ...` |
| `@ + timer!` | `@\A timer!` |
| `@ / U 'sim', fn` | `@\E 'sim', fn` |
| `@ / X 'input', fn` | `@\X 'input', fn` |
| `@ / L 'draw', fn` | `@\L 'draw', fn` |
| `o >> arena` | `o\F arena` |
| `o\link target` | `o\K target` |

**Short method reference:**
| Letter | Method | Purpose |
|--------|--------|---------|
| T | object | Create object |
| Y | set | Set properties |
| U | build | Run build function |
| E | early_action | Early phase action |
| X | action | Main phase action |
| L | late_action | Late phase action |
| A | add | Add child |
| F | flow_to | Add self to parent |
| K | link | Link to target |

**Verdict:**
- Short methods are nearly as compact as operators
- No expression-context limitation
- Single letters (T, Y, U, E, X, L, A, F, K) are learnable
- Class constructors look almost identical
- Only cost: variable + extra line for inline creation
