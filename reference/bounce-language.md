# Bounce Language Reference

A concise, symbol-prefixed language for game development. Designed for LLM-human collaboration: easy to generate, easy to scan, easy to read.

---

## Philosophy

Every line starts with a character that tells you what kind of line it is:

| Prefix | Meaning |
|--------|---------|
| `:` | Definition (entity, section) |
| `>` | Event handler |
| `?` | Conditional (if) |
| `\|` | Else-if / else / case branch |
| `*` | Loop / repeat |
| `+` | Spawn / add |
| `!` | Method call / effect |
| `=` | Assignment |

You can scan the left edge of code and immediately understand its structure.

---

## Definitions (`:`)

### Entity Definition

```bounce
: ball
  ...

: arena
  ...

: hit-particle
  ...
```

Defines a new entity type. Names are lowercase with kebab-case.

### Sections

Sections group related declarations inside an entity:

```bounce
: ball
  : props
    x, y
    radius
    hp

  : tune
    max-speed = 448
    energy-boost = 1.09

  : derive
    scale << 2 * radius / image.width

  : components
    + collider circle(radius) dynamic
    + timer
    + spring [hit squash-x squash-y]

  : children
    + hp-bar
    + hp-ui team hp max-hp
```

| Section | Purpose |
|---------|---------|
| `props` | Instance properties |
| `tune` | Tunable constants (for tweaking) |
| `derive` | Computed/reactive properties |
| `components` | Components to add |
| `children` | Child entities to add |
| `track` | Mutable state variables |

### Draw Sections

Draw sections specify which layer to render to:

```bounce
: ball
  : draw game
    at x y scale (squash-x squash-y)
      draw image

  : draw weapons
    at wx wy rot weapon-angle
      draw weapon-image
```

This compiles to separate draw methods per layer. The engine calls them in layer order.

---

## Events (`>`)

### Event Handlers

```bounce
> create
  = hp max-hp
  = angle 0~2*pi
  ! tag #ball

> update dt
  = angle collider.angle
  ! process-physics

> destroy
  ! spawn-death-particles
```

Common events: `create`, `update`, `destroy`, `early-update`, `late-update`.

### Collision Events

```bounce
> collision #ball #ball a b point normal
  = intensity remap(length(a.velocity + b.velocity) 0 800 0 1)
  ! a.spring.hit.pull intensity 3 0.7
  ! b.spring.hit.pull intensity 3 0.7
  ! sound ball-ball 0.4

> collision #ball #wall b w point normal
  ! b.squash normal 0.75
  ! sound ball-wall 0.4
```

Format: `> collision #tag-a #tag-b binding-a binding-b point normal`

### Conditional Events (when)

```bounce
? ready?! & aligned?!
  ! fire
```

At the top level of an entity, `?` acts like a "when" — the block runs whenever the condition becomes true.

---

## Conditionals (`?`, `|`)

### If / Else-If / Else

```bounce
? hp <= 0
  ! die
| hp < 10
  ! flash-warning
|
  ! normal-state
```

- `?` — if
- `| condition` — else-if
- `|` (no condition) — else

### Ternary (inline)

```bounce
= image team == #player ? images.cowboy : images.no-mouth
```

### Switch / Match

```bounce
? weapon-type
| #gun
  ! fire-projectile
| #dagger
  ! swing
| #sword
  ! slash
|
  ! nothing
```

With single-expression branches using `->`:

```bounce
? weapon-type
| #gun -> fire-projectile!
| #dagger -> swing!
| _ -> nothing!
```

`_` is the wildcard/default pattern.

### Guards

```bounce
? weapon-type
| #gun & ammo > 0 -> fire!
| #gun -> reload!
| #dagger -> swing!
```

Use `&` after a pattern to add a guard condition.

---

## Loops (`*`)

### Repeat N Times

```bounce
* 10
  ! spawn-enemy
```

### Iterate Collection

```bounce
* enemies enemy
  ! enemy.update dt
```

The name after the collection is the binding for each element.

### Iterate Range

```bounce
* 1..10 i
  ! print i
```

### Random Repeat Count

```bounce
* 2~4
  + effects hit-particle x y
```

### While Loop

```bounce
* ? alive?!
  ! process-frame

* ? hp > 0
  = hp - poison-damage
  ! wait 1
```

`* ?` combines loop (`*`) with condition (`?`).

### Loop Control

```bounce
* enemies enemy
  ? enemy.dead
    >>                    -- continue (skip to next iteration)
  ? found-target
    <<                    -- break (exit loop)
  ! enemy.update dt
```

- `>>` — continue
- `<<` — break

---

## Spawn / Add (`+`)

### Adding Components (in sections)

```bounce
: components
  + collider circle(radius) dynamic restitution=1
  + timer
  + spring [hit weapon squash-x squash-y]
```

### Adding Children (in sections)

```bounce
: children
  + hp-bar
  + hp-ui team hp max-hp
```

### Runtime Spawning

```bounce
> collision #ball #wall b w point normal
  + ^effects hit-circle point.x point.y radius=6
  + ^effects hit-particle point.x point.y vel=100~200
```

`^effects` accesses parent's effects container.

### Spawn and Store Reference

```bounce
> create
  + = player-ball ball x y #player #gun
  + = enemy-ball ball x y #enemy #dagger
```

`+ =` spawns and stores the reference. The `+` leads (consistent prefix), `=` indicates assignment.

---

## Method Calls (`!`)

### Line starts with `!` — the line IS a method call:

```bounce
! sound ball-ball 0.4 pitch=0.95~1.05
! camera.shake 4 0.15
! collider.impulse 10 -20
! spring.hit.pull 0.4 3 0.7
! timer.after 0.5 -> = ready yes
! hp-bar.activate
! kill
```

### Suffix `!` — call with no arguments (when line starts with another symbol):

Like YueScript, `!` at the end calls a function with no arguments:

```bounce
= angle collider.get-angle!           -- line starts with =, calling get-angle()
= vx, vy collider.get-velocity!       -- calling get-velocity()
? player.alive?!                       -- line starts with ?, calling alive?()
? hp-bar.visible?!                     -- calling visible?()
```

### Calls with arguments don't need suffix `!`:

```bounce
= intensity remap(combined 0 800 0 1)  -- has args, no ! needed
= clamped clamp(x 0 1)                 -- has args, no ! needed
```

### Summary:

- Line IS a call → `!` prefix: `! kill`
- Call with no args inside another line → `!` suffix: `= x get-value!`
- Call with args → just the args: `= x compute(a b c)`

---

## Assignment (`=`)

### Basic Assignment

```bounce
= x 10
= name "player"
= angle 0~2*pi
= vx, vy collider.velocity
```

Format: `= target value`

Multiple targets use commas for destructuring.

### Compound Assignment

```bounce
= hp - damage           -- hp = hp - damage
= time + dt             -- time = time + dt
= scale * 2             -- scale = scale * 2
= count / 2             -- count = count / 2
```

Format: `= target op value` where op is `+`, `-`, `*`, `/`

### Negative Values

```bounce
= x (-5)                -- x = -5
= velocity (-1 * speed) -- x = -speed
```

Use parentheses for negative literals.

---

## Random and Ranges

### Random (`a~b`)

```bounce
= angle 0~2*pi              -- random float between 0 and 2*pi
= count 2~4                 -- random int between 2 and 4
= pitch 0.95~1.05           -- random float for slight variation
= direction -1~1 * speed    -- random sign
```

`a~b` produces a random value in the range.

### Range (`a..b`)

```bounce
* 1..10 i                   -- iterate 1, 2, 3, ..., 10
= clamped clamp(x 0..1)     -- range as argument
```

`a..b` is a range value (for iteration, clamping, remapping).

---

## References

### Implicit Self

Bare names refer to self:

```bounce
> update dt
  = angle collider.angle      -- self.angle = self.collider.angle (attribute)
  = vx, vy collider.get-velocity!  -- calling get-velocity() with no args
  ! hp-bar.activate           -- self.hp-bar.activate()
```

### Parent (`^`)

```bounce
+ ^effects hit-particle x y   -- parent.effects
= paused ^paused              -- parent.paused
? ^game-over
  ! show-menu
```

### Return (`^`)

```bounce
> update dt
  ? paused
    ^                         -- early return

  ! do-stuff

: get-damage-multiplier
  ? critical?!
    ^ 2.0                     -- return 2.0
  ^ 1.0                       -- return 1.0
```

Bare `^` returns nothing. `^ value` returns a value.

---

## Derived Values (`<<`)

Reactive bindings that auto-update when dependencies change:

```bounce
: derive
  scale << 2 * radius / image.width
  image << team == #player ? images.cowboy : images.no-mouth
  damage << base-damage * multiplier
```

When `radius` or `image.width` changes, `scale` automatically recomputes. You never manually assign to a derived value.

This is like a spreadsheet formula — declare the relationship once, it stays in sync.

---

## Tags (`#`)

### Tag Literals

```bounce
#ball
#player
#enemy
#gun
#dagger
```

### Tagging Entities

```bounce
> create
  ! tag #ball
  ! tag #player
  ! collider.sensor.tag #hitbox
```

### Checking Tags

```bounce
? b.is? #player
  ! camera.shake 4 0.15
```

### In Collision Handlers

```bounce
> collision #ball #wall b w point normal
  ...
```

---

## Lambdas (`->`)

### Inline Callbacks

```bounce
! timer.after 0.5 -> = ready yes
! timer.after duration -> kill!
! timer.every 1.0 -> spawn-enemy!
```

### Multi-Statement Lambdas

```bounce
! timer.after 0.5 ->
  = ready yes
  ! flash
  ! sound ready-sound 0.5
```

Indent the block under the `->`.

### In Switch Branches

```bounce
? state
| #idle -> wait!
| #moving -> update-position!
| #attacking -> deal-damage!
```

---

## Functions / Methods (`:`)

### Defining Methods

```bounce
: take-damage amount source
  = hp - amount
  ! flash
  ? hp <= 0
    ! die

: squash normal amount=0.3
  ? abs(normal.y) > abs(normal.x)
    ! spring.squash-x.pull amount 3 0.5
  |
    ! spring.squash-y.pull amount 3 0.5
```

Default arguments use `=`.

### Boolean Method Convention

Methods returning bool end with `?`:

```bounce
: alive?
  ^ hp > 0

: ready?
  ^ cooldown <= 0 & ammo > 0
```

### Calling Methods

```bounce
! b.take-damage 10 enemy
! b.squash normal 0.5
? player.alive?!
  ! continue-game
```

Parentheses are optional (like YueScript/Ruby). Use `!` suffix for no-arg calls when the line starts with another symbol.

---

## Transforms

For drawing with push/pop transform stacks:

```bounce
: draw game
  ! at x y scale (squash-x squash-y)
    ! at 0 0 rot angle scale (scale * hit)
      ! draw image flash=flashing
```

`at` is a function that pushes a transform. The indented block is the scope. Pop happens automatically when the block ends.

Transform arguments:
- Position: `at x y`
- Rotation: `rot angle` or `at x y rot angle`
- Scale: `scale s` or `scale (sx sy)` or `at x y scale s`

Combine as needed: `at x y rot angle scale (sx sy)`

---

## Complete Example

```bounce
: ball
  : props
    x, y
    team
    radius = 10
    weapon-type = #dagger
    hp
    angle
    flashing = no

  : tune
    max-hp = 50
    base-omega = 1.5 * pi
    max-omega = 3 * pi
    low-vy-threshold = 0.5~1.5

  : derive
    scale << 2 * radius / image.width
    image << team == #player ? images.cowboy : images.no-mouth
    weapon << weapon.create weapon-type

  : components
    + collider circle(radius) dynamic restitution=1 friction=0
    + timer
    + spring [hit weapon squash-x squash-y]

  : children
    + hp-bar
    + hp-ui team hp max-hp
    + weapon-ui team weapon-type

  > create
    = hp max-hp
    = angle 0~2*pi
    = collider.angle angle
    = collider.gravity-scale 0
    ! tag #ball

  > update dt
    ? ^paused
      ^

    = angle collider.angle
    = vx, vy collider.velocity
    = omega collider.angular-velocity
    = omega-speed abs(omega)

    -- Angular velocity guardrails
    ? omega-speed > base-omega
      = time-above-base + dt
      = time-below-base 0
      ? time-above-base > 0.5
        = new-speed lerp-dt(0.9 1 dt omega-speed base-omega)
        ! collider.set-angular-velocity sign(omega) * new-speed
    | omega-speed < base-omega
      = time-below-base + dt
      = time-above-base 0
      ? time-below-base > 0.25
        = new-speed lerp-dt(0.9 0.5 dt omega-speed base-omega)
        ! collider.set-angular-velocity sign(omega) * new-speed
    |
      = time-above-base 0
      = time-below-base 0

    -- Floor sliding detection
    = near-floor y + radius > ^y + ^h - 20
    ? near-floor
      = low-vy-time + dt
      ? low-vy-time > low-vy-threshold & abs(vy) < 15
        = low-vy-time 0
        = low-vy-threshold 0.5~1.5
        ! collider.impulse sign(vx) * 0~6 -24
        ! sound hop 0.5 pitch=0.95~1.05
        ! spring.squash-y.pull 0.5 3 0.5
        ! spring.squash-x.pull -0.25 3 0.5
        + ^effects dash-particle x y+radius dir=(pi/4)~(3*pi/4)
    |
      = low-vy-time 0

  : draw game
    ! at x y scale (spring.squash-x.x spring.squash-y.x)
      ! at 0 0 rot angle scale (scale * spring.hit.x)
        ! draw image flash=flashing

  : draw weapons
    = offset weapon.visual-offset - weapon-recoil-offset
    = perp angle - pi/2
    = wx x + offset * cos(angle) + 2 * cos(perp)
    = wy y + offset * sin(angle) + 2 * sin(perp)
    ! at wx wy rot (angle + weapon.rotation-offset + weapon-recoil) scale (weapon.scale * spring.weapon.x)
      ! draw weapon.image flash=weapon-flashing

  : take-damage amount source
    = hp - amount
    ! spring.hit.pull 0.3 3 0.7
    ! flash-ball
    ! sound dagger-ball 0.88 pitch=0.95~1.05
    + ^effects damage-number x y-radius-10 amount
    ! hp-bar.activate
    ! hp-ui.refresh hp
    ! weapon-ui.hit

    ? team == #player
      ! camera.shake 4 0.15

    ? hp <= 0
      ! sound player-death 0.44
      ! kill

  : squash normal amount=0.3
    ? abs(normal.y) > abs(normal.x)
      ! spring.squash-x.pull amount 3 0.5
      ! spring.squash-y.pull -amount * 0.5 3 0.5
    |
      ! spring.squash-y.pull amount 3 0.5
      ! spring.squash-x.pull -amount * 0.5 3 0.5

  : start-moving
    = collider.gravity-scale 1
    ! collider.impulse -1~1 * 5~10 -10~10
    = collider.angular-velocity base-omega

  : flash-ball dur=0.15
    = flashing yes
    ! timer.after dur -> = flashing no

  : alive?
    ^ hp > 0


: arena
  : props
    w = 200
    h = 200
    paused = yes

  : derive
    x << (gw - w) / 2
    y << (gh - h) / 2

  : tune
    max-ball-speed = 448
    ball-energy-boost = 1.09
    weapon-energy-boost = 1.14

  : track
    system-energy = 250
    high-speed-hits = 0
    time-since-hit = 1.5

  : children
    + container #effects
    + container #projectiles
    + timer

  > create
    -- Walls
    + wall x+w/2 y-100 w+400 200
    + wall x+w/2 y+h+100 w 200 floor=yes
    + wall x-100 y+h/2+100 200 h+200
    + wall x+w+100 y+h/2+100 200 h+200

    -- Balls
    + = player-ball ball x+w*0.25 y+h/2 #player #gun
    + = enemy-ball ball x+w*0.75 y+h/2 #enemy #dagger

    ! spawn-plants
    ! spawn-clouds

  > early-update dt
    ! draw-gradient-v bg 0 0 gw gh sky-top sky-bottom

    ? key-pressed #m
      ! music-play bgm

    ? paused
      ? key-pressed #space | mouse-pressed 1
        = paused no
        ! player-ball.start-moving
        ! enemy-ball.start-moving
      ^

  > update dt
    = time-since-hit + dt

  > collision #ball #ball a b point normal
    = va a.collider.velocity
    = vb b.collider.velocity
    = sa length(va)
    = sb length(vb)
    = max-speed max(sa sb)
    = combined sa + sb

    -- Update tracking
    = system-energy combined

    -- High speed check
    ? max-speed > 0.78 * max-ball-speed
      = high-speed-hits + 1

    -- Bypass check (S-curve)
    = bypass-chance cubic-in-out(clamp(high-speed-hits 0 9) / 9)
    ? 0~1 < bypass-chance
      = high-speed-hits 0
      = va * 0.7
      = vb * 0.7
    |
      = va * ball-energy-boost
      = vb * ball-energy-boost

    -- Cap speeds
    ? length(va) > max-ball-speed
      = va * max-ball-speed / length(va)
    ? length(vb) > max-ball-speed
      = vb * max-ball-speed / length(vb)

    = a.collider.velocity va
    = b.collider.velocity vb

    ! sound ball-ball 0.4 pitch=0.95~1.05

    = intensity remap(combined 0 800 0 1)
    ! a.spring.hit.pull intensity * 0.4 3 0.7
    ! b.spring.hit.pull intensity * 0.4 3 0.7

  > collision #ball #wall b w point normal
    = v b.collider.velocity
    = intensity clamp(remap(length(v) 0 800 0 1) 0 1)

    ? intensity > 0.45
      = pitch 1 + remap(intensity 0.45 0.7 0 1)
    |
      = pitch 1

    ! sound ball-wall 0.4 pitch=pitch * 0.95~1.05
    ! b.squash normal 0.75 * intensity

  : hit-effect x y
    = prob get-hit-stop-probability!
    = chance 35 + 65 * prob
    ? 0~100 < chance
      + effects hit-effect-anim x y scale=1.35
    |
      + effects hit-circle x y radius=9
      * 2~4
        + effects hit-particle x y vel=100~250 dur=0.3~0.5

  : get-hit-stop-probability
    = t clamp(time-since-hit / 1.5 0 1)
    ? t < 0.5
      ^ 0
    ^ quint-out(remap(t 0.5 1 0 1))


: hit-particle
  : props
    x, y
    vel = 150
    dir = 0~2*pi
    dur = 0.4
    col = white
    gravity = 0

  : derive
    vx << vel * cos(dir)
    vy << vel * sin(dir)

  > create
    = flash yes
    ! timer.after 0.1 -> = flash no
    ! timer.after dur -> kill!

  > update dt
    = x + vx * dt
    = y + vy * dt
    = vy + gravity * dt

  : draw effects
    ! draw-circle x y 2 flash ? white : col


: plant
  : props
    x, y
    image-name
    w, h
    layer = #front
    offset = 0
    min-rotation = -0.3

  : tune
    force-threshold = 3
    max-rotation = 0.5
    spring-freq = 2
    spring-bounce = 0.3

  : track
    force-count = 0

  : components
    + collider box(w h) sensor
    + spring rotation

  : derive
    scale << w / images[image-name].width
    draw-y << y + offset - h/2

  > create
    ! collider.set-position x draw-y
    ! tag #plant-ghost
    = rotation min-rotation~max-rotation
    ! spring.rotation.set-target rotation

  : apply-force dir intensity can-kill=no
    = force-count + 1
    ? can-kill & force-count > force-threshold
      + ^effects cut-plant x draw-y image-name scale dir intensity
      ! kill
      ^

    = force clamp(intensity / 200 0.1 1)
    ! spring.rotation.pull dir * force * 0.5 spring-freq spring-bounce

  : draw layer
    = rot clamp(spring.rotation.x min-rotation max-rotation)
    ! at x draw-y rot rot scale scale
      ! draw images[image-name]
```

---

## Quick Reference

### Line Prefixes

| Prefix | Meaning | Example |
|--------|---------|---------|
| `:` | Definition | `: ball`, `: props`, `: draw game` |
| `>` | Event | `> create`, `> update dt`, `> collision ...` |
| `?` | If / when | `? hp <= 0` |
| `\|` | Else-if / else | `\| hp < 10`, `\|` |
| `*` | Loop | `* 10`, `* enemies enemy`, `* ? alive?!` |
| `+` | Spawn | `+ timer`, `+ = name entity x y` |
| `!` | Method call | `! sound bang 0.5`, `! b.update dt` |
| `=` | Assignment | `= x 10`, `= x + 5` |

### Operators

| Syntax | Meaning |
|--------|---------|
| `a~b` | Random in range |
| `a..b` | Range |
| `#name` | Tag literal |
| `^` | Parent access / return |
| `<<` | Derived binding / break |
| `->` | Lambda |
| `>>` | Continue |
| `func!` | Call with no args |
| `name?` | Boolean method convention |

### Control Flow

| Syntax | Meaning |
|--------|---------|
| `? cond` | If |
| `\| cond` | Else-if |
| `\|` | Else |
| `? val` + `\| pattern` | Switch/match |
| `* n` | Repeat n times |
| `* coll name` | Iterate |
| `* a..b name` | Iterate range |
| `* ? cond` | While |
| `>>` | Continue |
| `<<` | Break |
| `^` | Return |
| `^ val` | Return value |
