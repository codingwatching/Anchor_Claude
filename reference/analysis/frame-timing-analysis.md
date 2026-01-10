# Analysis: Anchor's Frame Timing vs. Best Practices

> **Note:** This analysis was performed before implementing fixes. The final implementation uses **120Hz physics with 60Hz render cap and vsync snapping**. See anchor.c and the monitor simulator (tools/monitor_sim.c) for the actual implementation.

Based on Tyler Glaiel's article "How to make your game run at 60fps" and his [frame_timer.cpp](https://github.com/TylerGlaiel/FrameTimingControl/blob/master/frame_timer.cpp) implementation.

---

## Anchor's Current Implementation

From `anchor.c` lines 41-44, 833-839, 1071-1136:

```c
// Configuration
#define PHYSICS_RATE (1.0 / 144.0)  // 144 Hz physics
#define RENDER_RATE  (1.0 / 60.0)   // 60 Hz render
#define MAX_UPDATES 10              // Spiral of death cap

// State (doubles)
static double physics_lag = 0.0;
static double render_lag = 0.0;

// Main loop
Uint64 current_time = SDL_GetPerformanceCounter();
double dt = (double)(current_time - last_time) / (double)perf_freq;
last_time = current_time;

physics_lag += dt;
if (physics_lag > PHYSICS_RATE * MAX_UPDATES) {
    physics_lag = PHYSICS_RATE * MAX_UPDATES;
}
render_lag += dt;

while (physics_lag >= PHYSICS_RATE) {
    // update...
    physics_lag -= PHYSICS_RATE;
}

if (render_lag >= RENDER_RATE) {
    render_lag -= RENDER_RATE;
    // render...
}
```

---

## What Anchor Does Well

1. **Spiral of death protection** ✅
   - Caps `physics_lag` at `MAX_UPDATES * PHYSICS_RATE` (~69ms)
   - Prevents runaway accumulation

2. **High-resolution timer** ✅
   - Uses `SDL_GetPerformanceCounter()` / `SDL_GetPerformanceFrequency()`
   - Best available precision

3. **VSync enabled** ✅
   - `SDL_GL_SetSwapInterval(1)` at line 1286

4. **Fixed timestep for physics** ✅
   - Deterministic 144Hz updates

---

## Critical Issues Found

### 1. No VSync Time Snapping ❌

**The Problem:** Even with vsync on, frame times aren't exactly 1/60th of a second. OS timer precision, scheduler jitter, and other factors introduce noise. If your 60Hz frame takes 16.68ms instead of 16.667ms, the accumulator drifts by 0.013ms. Over 1000 frames, that's 13ms of drift — enough to trigger a double update.

**The 59.94Hz Monitor Problem:** Many monitors (especially TVs and some gaming monitors) run at 59.94Hz, not 60Hz. This means every ~1000 frames, the accumulator will have drifted enough to cause a double update. The article's author discovered this firsthand.

**Glaiel's solution:**
```cpp
int64_t vsync_maxerror = clocks_per_second * .0002;  // 0.2ms tolerance
int64_t snap_frequencies[8];
for(int i = 0; i < 8; i++) {
    snap_frequencies[i] = (clocks_per_second / snap_hz) * (i+1);
}

// Before adding to accumulator:
for(int64_t snap : snap_frequencies) {
    if(std::abs(delta_time - snap) < vsync_maxerror) {
        delta_time = snap;
        break;
    }
}
```

**Anchor does nothing here** — raw delta time goes directly into the accumulator.

### 2. No Delta Time Averaging ❌

**The Problem:** A single slow frame (OS scheduler hiccup, background process, etc.) causes a spike that propagates directly into the accumulator, potentially causing multiple catch-up updates.

**Glaiel's solution:**
```cpp
const int time_history_count = 4;
int64_t time_averager[time_history_count];
int64_t averager_residual = 0;

// Shift and add new value
for(int i = 0; i < time_history_count-1; i++) {
    time_averager[i] = time_averager[i+1];
}
time_averager[time_history_count-1] = delta_time;

// Average with residual tracking for precision
int64_t averager_sum = 0;
for(int i = 0; i < time_history_count; i++) {
    averager_sum += time_averager[i];
}
delta_time = averager_sum / time_history_count;
averager_residual += averager_sum % time_history_count;
delta_time += averager_residual / time_history_count;
averager_residual %= time_history_count;
```

This smooths a single 4x slow frame into four 1.75x frames instead — much less jarring.

### 3. No Timing Anomaly Handling ❌

**The Problem:** Timer wraparound (rare but possible on 32-bit counters) or massive frame spikes (alt-tabbing, debugger pause, laptop sleep resume) can inject garbage values.

**Glaiel's solution:**
```cpp
if(delta_time > desired_frametime * 8) {
    delta_time = desired_frametime;
}
if(delta_time < 0) {
    delta_time = 0;
}
```

**Anchor only caps the accumulator after adding** — a huge delta still gets added, then capped. This is subtly different and worse for consistency.

### 4. Floating Point Accumulator ⚠️

**The Problem:** From the article's addendum:
> "Using doubles or floats introduces floating point error, like adding 1.0/60.0 60 times in a row will not actually end up being exactly 1. In my engine I actually use 64 bit integers for my timer code instead."

**Anchor uses `double`** for both `physics_lag` and `render_lag`. Over a long play session (hours), this could accumulate meaningful drift.

**Glaiel's solution:** Keep everything as `int64_t` (which SDL already provides), only convert to double when passing to game logic.

### 5. No Resync Mechanism ❌

**The Problem:** After loading a level, switching scenes, or resuming from pause, the accumulator contains stale time that shouldn't be "caught up."

**Glaiel's solution:**
```cpp
if(resync) {
    frame_accumulator = 0;
    delta_time = desired_frametime;
    resync = false;
}
```

**Anchor has no way to trigger this** — not exposed to Lua, not called internally after any event.

### 6. The 144Hz/60Hz Split Creates Inherent Unevenness ⚠️

This is architectural and worth thinking about carefully.

On a 60Hz vsync'd monitor, each frame takes ~16.667ms. Physics runs at 144Hz (6.944ms per step). This means:

```
16.667ms / 6.944ms = 2.4 physics updates per frame
```

So you'll get a pattern like: **2, 2, 3, 2, 2, 3, 2, 2, 3...**

This is exactly the stutter pattern the article warns about! Objects move 2 steps some frames, 3 steps others. At 144Hz the individual steps are small enough that this might not be perceptible, but it's technically inconsistent.

**The "correct" solutions would be:**
1. Run physics at 60Hz (or 120Hz — an even multiple of 60)
2. Use interpolation to smooth rendering between physics states

The article's interpolation approach:
```cpp
game.render((double)frame_accumulator / desired_frametime);
// Interpolate between previous_state and current_state
```

### 7. render_lag Can Grow Unbounded ❌

**The Problem:** Look at the render loop:
```c
if (render_lag >= RENDER_RATE) {
    render_lag -= RENDER_RATE;
    // render
}
```

If a frame takes 50ms (3x RENDER_RATE), `render_lag` becomes 50ms. You render once, subtract 16.67ms, now it's 33.33ms. Next frame adds another 16.67ms = 50ms. You're permanently behind.

**Should be:**
```c
while (render_lag >= RENDER_RATE) {
    render_lag -= RENDER_RATE;
}
// Then render once
```

Or simply cap render_lag like physics_lag is capped.

### 8. Emscripten Behaves Differently ⚠️

Desktop: `SDL_GL_SwapWindow()` blocks until vsync
Web: `emscripten_set_main_loop(..., 0, 1)` uses requestAnimationFrame

These have different timing characteristics. requestAnimationFrame can fire at the display's native rate (144Hz on a 144Hz monitor), while the desktop path relies on vsync blocking.

---

## How to Test This

The article's key insight: **build a monitor simulator**. You can't test all hardware configurations manually.

**The output format:**
```
20211012021011202111020211102012012102012
```
Each digit = number of updates since last vsync. Anything other than consistent 1s (or consistent 2s for 30fps) indicates stutter.

**Statistics to track:**
- `TOTAL UPDATES` — should match expected (game_time × update_rate)
- `TOTAL VSYNCS` — should match expected (system_time × refresh_rate)
- `TOTAL DOUBLE UPDATES` — should be 0 for smooth experience
- `TOTAL SKIPPED RENDERS` — should be 0
- `GAME TIME vs SYSTEM TIME` — drift indicates timing bug

**Test cases to simulate:**

| Test | Monitor | VSync | Expected |
|------|---------|-------|----------|
| Baseline | 60Hz | On | Smooth (all 1s or 2-2-3 for 144Hz physics) |
| Common TV | 59.94Hz | On | Check for periodic double-updates |
| Gaming monitor | 144Hz | On | Different update pattern |
| Forced off | 60Hz | Off | Should not drift significantly |
| Slow frames | 60Hz | On | Inject 50ms frames, check recovery |
| Long duration | 60Hz | On | Run 10M frames, measure accumulated drift |

**Instrumentation to add to Anchor:**

1. **Update counter per render frame:**
```c
static int updates_this_frame = 0;
// In physics loop: updates_this_frame++;
// After render: printf("%d", updates_this_frame); updates_this_frame = 0;
```

2. **Delta time logging:**
```c
// Log to file: timestamp, raw_dt, snapped_dt, physics_lag, render_lag
```

3. **Drift measurement:**
```c
static double game_time = 0.0;
static Uint64 start_wall_time;
// game_time += PHYSICS_RATE each update
// Compare to (SDL_GetPerformanceCounter() - start_wall_time) / perf_freq
```

---

## Summary Table

| Feature | Glaiel's Approach | Anchor's Current | Risk Level |
|---------|-------------------|------------------|------------|
| VSync snapping | ✅ Snaps to common rates | ❌ Raw delta | **High** — causes periodic stutter |
| Delta averaging | ✅ 4-frame average | ❌ Raw delta | **Medium** — spikes propagate |
| Anomaly handling | ✅ Clamps before add | ❌ Caps after add | **Medium** — garbage values possible |
| Integer timing | ✅ int64_t internally | ❌ double | **Low** — long-term drift |
| Resync mechanism | ✅ Manual trigger | ❌ None | **Medium** — bad after loads |
| Accumulator cap | ✅ 8× frametime | ✅ 10× frametime | Good |
| Interpolation | ✅ Optional | ❌ None | **Low** — 144Hz physics masks it |
| Render lag cap | ✅ Implicit | ❌ Can grow | **Medium** — permanent lag possible |

---

## Recommendations

### Immediate fixes (low effort, high impact):

1. Add vsync snapping before adding to accumulators
2. Add delta time clamping before adding (not just cap after)
3. Fix render_lag to not grow unbounded
4. Add a resync function exposed to Lua

### Medium-term improvements:

5. Switch to int64_t for timing internals
6. Add delta time averaging (4-frame ring buffer)
7. Consider changing physics to 120Hz (even multiple of 60)

### For testing:

8. Build a monitor simulator mode
9. Add instrumentation for update counts, drift measurement
10. Log timing data to file for analysis

---

## Key Insight

The core insight from the article is that **timing bugs are subtle and cumulative**. A game can feel "mostly smooth" while still having periodic stutters that players notice subconsciously. The monitor simulator approach is the only reliable way to verify correctness across the full range of hardware configurations.
