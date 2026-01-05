# Analysis of the Reference Anchor Engine

This document analyzes the original Anchor engine (LÖVE-based) to inform the C rewrite.

---

## The Fixed Update Loop (init.lua:41-113)

This is the heart of the engine. Here's what each part does:

```lua
local fixed_update = function()
  -- 1. Calculate dt with timescale and slow_amount
  an.dt = love.timer.step()*an.timescale*an.slow_amount

  -- 2. Accumulate lag, capped to prevent "spiral of death"
  an.lag = math.min(an.lag + an.dt, an.rate*an.max_updates)

  -- 3. Fixed timestep loop - catch up if behind
  while an.lag >= an.rate do
    -- Process input events INSIDE the fixed loop
    if love.event then
      love.event.pump()
      for name, a, b, c, d, e, f in love.event.poll() do
        -- Store raw input state into an.input_*_state tables
      end
    end

    local dt = an.rate  -- Fixed delta (1/144)
    an.step = an.step + 1
    an.time = an.time + dt

    -- Collect all objects via DFS traversal
    local all_objects = an:get_all_children()
    table.insert(all_objects, 1, an)

    -- Three-phase update
    for _, object in ipairs(all_objects) do object:_early_update(dt*an.slow_amount) end
    for _, object in ipairs(all_objects) do object:_update(dt*an.slow_amount) end
    for _, object in ipairs(all_objects) do object:_late_update(dt*an.slow_amount) end

    -- Final cleanup (physics post-step, input state reset)
    an:_final_update()

    -- Remove dead actions/objects
    for _, object in ipairs(all_objects) do object:cleanup() end
    for _, object in ipairs(all_objects) do object:remove_dead_branch() end

    an.lag = an.lag - dt
  end

  -- 4. Frame rate limiter (busy-wait to hit target)
  while an.framerate and love.timer.getTime() - an.last_frame < 1/an.framerate do
    love.timer.sleep(.0005)
  end

  -- 5. Render (once per frame, not per fixed step)
  an.last_frame = love.timer.getTime()
  if love.graphics and love.graphics.isActive() then
    an.frame = an.frame + 1
    love.graphics.origin()
    love.graphics.clear()
    an:draw_layers()
    love.graphics.present()
  end

  love.timer.sleep(an.sleep)  -- Reduce CPU usage
end
```

---

## Key Configuration Values

| Variable | Default | Purpose |
|----------|---------|---------|
| `an.rate` | 1/144 | Fixed timestep (144 Hz physics!) |
| `an.framerate` | 60 | Target render rate (from monitor) |
| `an.max_updates` | 10 | Cap on fixed steps per frame |
| `an.sleep` | 0.001 | Sleep per frame (CPU relief) |
| `an.timescale` | 1 | Global time multiplier |
| `an.slow_amount` | 1 | Per-object/global slow-mo |

---

## Interesting Design Choices

### 1. 144 Hz fixed timestep, 60 Hz rendering

Physics runs at 144 Hz regardless of display refresh. This gives:
- Smoother physics (especially for fast-moving objects)
- Determinism (same dt every step)
- Decoupled from rendering

### 2. Input processed inside fixed loop

Unusual! Most engines process input per-frame. Here, input is effectively sampled at 144 Hz. This means:
- More responsive feel
- Input state (`pressed`, `released`) is per-fixed-step, not per-frame

### 3. Three-phase update system

Each object goes through `early_update` → `update` → `late_update`. This allows:
- Physics updates in early phase
- Game logic in main phase
- Camera/drawing setup in late phase

### 4. Slow-mo via `slow_amount`

Every dt gets multiplied by `slow_amount`. This applies to all objects, creating global or per-branch slow motion effects. The timer/tween system respects this too.

### 5. Draw command queuing

Drawing isn't immediate — it queues commands to layers, which are sorted by z-order and executed at render time. This decouples draw order from tree order.

---

## What We Should Implement in C

### Must Have for Phase 1

1. **Fixed timestep accumulator** — The `while an.lag >= an.rate` pattern
2. **Frame rate limiting** — The busy-wait loop to cap rendering
3. **Time tracking** — `step`, `time`, `frame` counters
4. **Max updates cap** — Prevents spiral of death
5. **Sleep per frame** — Reduces CPU usage

### Should Implement (but can simplify)

1. **Timescale** — Nice for slow-mo, but can defer
2. **Slow amount** — More complex, definitely defer to YueScript side

### What Stays in Lua/YueScript

1. **Three-phase updates** — Pure object system, no C involvement
2. **Object tree traversal** — Lua tables, Lua logic
3. **Timer/spring/action systems** — All Lua
4. **Draw command queuing** — Lua collects commands, C executes them

---

## Recommended Changes for C Implementation

### Fixed Timestep Rate

The reference uses 144 Hz, which is high. Options:
- **Keep 144 Hz** — Matches reference behavior exactly
- **Use 60 Hz** — Lower CPU usage, might feel slightly different
- **Make configurable** — Let the game choose

I'd recommend **keeping 144 Hz** for parity, or making it configurable via `anchor_start`.

### Input Processing

The reference processes input inside the fixed loop. For C, I'd suggest:
- **Poll events once per frame** (SDL_PollEvent)
- **Buffer input state** for Lua to read
- Let Lua handle the `pressed`/`released` logic per its own fixed steps

This is simpler and SDL's event queue handles the buffering anyway.

---

## Recommended C Loop Structure

Based on this analysis, here's what the C loop should look like:

```c
// Configuration (set via anchor_start from Lua)
double fixed_rate = 1.0 / 144.0;  // 144 Hz
int max_updates = 10;
double sleep_time = 0.001;

// State
double lag = 0.0;
double last_frame_time = 0.0;
Uint64 step = 0;
double game_time = 0.0;
Uint64 frame = 0;

// Main loop
while (running) {
    double current_time = SDL_GetTicks64() / 1000.0;
    double dt = current_time - last_frame_time;
    last_frame_time = current_time;

    // Accumulate, but cap to prevent spiral of death
    lag += dt;
    if (lag > fixed_rate * max_updates) {
        lag = fixed_rate * max_updates;
    }

    // Fixed timestep loop
    while (lag >= fixed_rate) {
        // Process SDL events (once per fixed step, like reference)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Handle quit, input state updates
        }

        // Call Lua update (Lua does tree traversal, phases)
        // lua_call(L, "an:_fixed_update", fixed_rate);

        step++;
        game_time += fixed_rate;
        lag -= fixed_rate;
    }

    // Render
    frame++;
    glClear(GL_COLOR_BUFFER_BIT);
    // lua_call(L, "an:draw_layers");
    SDL_GL_SwapWindow(window);

    // Small sleep to reduce CPU
    SDL_Delay((Uint32)(sleep_time * 1000));
}
```

---

## Other Notable Systems

### Timer System (timer.lua)

The timer is remarkably full-featured:

- `timer_after(delay, action, tag)` — one-shot
- `timer_every(delay, action, times, immediate, after, tag)` — repeating
- `timer_tween(delay, target, source, method, after, tag)` — interpolation
- `timer_condition(condition, action, times, after, tag)` — trigger on condition
- `timer_change(field, action, times, after, tag)` — trigger on field change
- `timer_cooldown(delay, condition, action, ...)` — conditional with delay
- `timer_for(delay, action, after, tag)` — run every frame for duration
- `timer_every_step(start, end, times, action, ...)` — variable interval
- `timer_for_step(duration, start, end, action, ...)` — fit steps into duration

**Key insight:** Delays can be tables `{min, max}` for random ranges. Tags allow cancellation/replacement.

### Spring System (spring.lua)

Simple damped harmonic oscillator:

```lua
spring_1d = class:class_new()
function spring_1d:spring_1d(x, k, d)
  self.x = x or 0      -- current value
  self.k = k or 100    -- stiffness
  self.d = d or 10     -- damping
  self.target_x = self.x
  self.v = 0           -- velocity
end

function spring_1d:spring_1d_update(dt)
  local a = -self.k*(self.x - self.target_x) - self.d*self.v
  self.v = self.v + a*dt
  self.x = self.x + self.v*dt
end

function spring_1d:spring_1d_pull(f, k, d)
  if k then self.k = k end
  if d then self.d = d end
  self.x = self.x + f
end
```

The `spring` mixin wraps multiple `spring_1d` objects by name.

### Physics World (physics_world.lua)

Uses LÖVE's Box2D bindings. Key patterns:

- **Collision/trigger separation** — contacts vs sensors
- **Tag-based filtering** — `physics_world_enable_collision_between`
- **Event buffering** — `collision_enter`, `collision_exit`, `collision_active` per frame
- **Spatial queries** — `physics_world_get_objects_in_area`

### Input System (input.lua)

Action-based input with multiple bindings:

```lua
an:input_bind('left', {'key:left', 'key:a', 'axis:leftx-', 'button:dpleft'})
```

States: `pressed` (this frame), `down` (held), `released` (this frame).

### Layer System (layer.lua)

Draw commands are queued, not immediate:

```lua
function layer:circle(x, y, rs, color, line_width, z)
  table.insert(self.draw_commands, {type = 'circle', args = {...}, z = z or 0})
end
```

Commands are sorted by z and executed at render time. This decouples draw calls from execution order.

---

## Questions for the Developer

Looking at this codebase, I'm curious:

1. **Why 144 Hz specifically?** Was this tuned for a specific game feel, or just "high enough"?

2. **The mixin architecture** — Everything is mixed into `object`. Was this intentional from the start, or evolved over time? It's very different from typical component-entity patterns.

3. **Draw command sorting by z** — How often do you actually use z-ordering in practice? Is it critical for layering effects, or mostly unused?
