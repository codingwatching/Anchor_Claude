# Fixed Timestep Details

## Point 2: Events Drain on First Step

### The Setup

- Display: 60 Hz (16.67ms per frame)
- Fixed timestep: 144 Hz (6.94ms per step)
- Result: ~2-3 fixed steps per frame

### Example Timeline

```
Frame N begins
├── dt = 16.67ms (time since last frame)
├── lag = 0 + 16.67 = 16.67ms
│
├── Fixed Step 1 (lag = 16.67ms, >= 6.94ms ✓)
│   ├── SDL_PollEvent() → processes ALL queued events
│   │   └── User pressed SPACE → input_state[SPACE] = true
│   ├── (Lua update would run here)
│   ├── lag = 16.67 - 6.94 = 9.73ms
│
├── Fixed Step 2 (lag = 9.73ms, >= 6.94ms ✓)
│   ├── SDL_PollEvent() → queue is EMPTY (already drained)
│   │   └── No events to process
│   ├── (Lua update runs with same input state)
│   ├── lag = 9.73 - 6.94 = 2.79ms
│
├── Fixed Step 3? (lag = 2.79ms, >= 6.94ms ✗)
│   └── Skip, exit loop
│
├── Render
└── SwapWindow (waits for VSync)
```

### What This Means

The SPACE press is only "seen" by SDL_PollEvent on Step 1. Step 2 gets no events.

But this is fine because:
- The raw input state (`input_state[SPACE] = true`) persists
- Lua's input system computes `pressed` by comparing current vs previous state
- `pressed` would be true on Step 1, false on Step 2 (previous now equals current)

This is actually **correct behavior** — a single key press should only register as "pressed" for one simulation step, not multiple.

### What Would Be Wrong

If we polled events OUTSIDE the fixed loop (once per frame):

```
Frame N begins
├── SDL_PollEvent() → SPACE pressed
├── dt = 16.67ms, lag = 16.67ms
│
├── Fixed Step 1
│   └── is_pressed('jump') → true ← correct
├── Fixed Step 2
│   └── is_pressed('jump') → true ← WRONG! double jump!
```

By polling inside the loop, we ensure each input maps to exactly one simulation step.

---

## Point 3: First Frame Edge Case

### The Problem

```c
// Before loop
Uint64 last_time = SDL_GetPerformanceCounter();  // T = 0.000

// Loop iteration 1
Uint64 current_time = SDL_GetPerformanceCounter();  // T = 0.001 (1ms later, maybe less)
double dt = (current_time - last_time) / freq;      // dt = 0.001 seconds
lag += dt;                                          // lag = 0.001 seconds = 1ms

// Is lag >= FIXED_RATE?
// lag = 1ms, FIXED_RATE = 6.94ms
// 1ms >= 6.94ms? NO
// Skip fixed loop entirely!

// Render (with no simulation having run)
// SwapWindow blocks for VSync (~16.67ms on 60Hz)

// Loop iteration 2
// dt = ~16.67ms
// lag = 1ms + 16.67ms = 17.67ms
// Now we run 2 fixed steps
```

### Timeline

```
T = 0.000ms    last_time set
T = 0.050ms    Loop starts, current_time set (50 microseconds later)
               dt = 0.05ms, lag = 0.05ms
               0.05ms >= 6.94ms? NO
               Skip fixed loop
               Render (empty frame)
               SwapWindow blocks...

T = 16.67ms    VSync releases
               dt = 16.62ms, lag = 16.67ms
               16.67ms >= 6.94ms? YES
               Run Step 1, lag = 9.73ms
               Run Step 2, lag = 2.79ms
               Render
               SwapWindow blocks...

T = 33.33ms    Normal operation continues
```

### Why It Doesn't Matter

On that first frame:
- No game state exists yet to simulate
- We just render an empty/initial screen
- By frame 2, we're running normally

The alternative — adding a fake initial dt or running one step unconditionally — adds complexity for no benefit.

### When It Would Matter

If you had a loading screen or intro that needed to start animating immediately on frame 1, you might notice a single skipped frame. But:
- It's one frame out of thousands
- The screen is probably blank anyway (nothing loaded yet)
- VSync means frame 1 still displays for the correct duration
