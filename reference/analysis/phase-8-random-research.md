# Phase 8: Random System Research

Comprehensive research into random/RNG APIs across 17+ game engines, plus analysis of the existing Anchor implementation, to inform the design of Anchor's random system.

---

## Table of Contents

1. [Phase 8 Requirements](#phase-8-requirements)
2. [Existing Anchor Implementation](#existing-anchor-implementation)
3. [Cross-Engine API Research](#cross-engine-api-research)
4. [Design Decisions](#design-decisions)
5. [Recommended API](#recommended-api)

---

## Phase 8 Requirements

From `ANCHOR_IMPLEMENTATION_PLAN.md`:

```lua
-- Goal: Seedable PRNG for deterministic gameplay and replay support

an:random_seed(12345)
local x = an:random_float(0, 100)      -- [0, 100)
local i = an:random_int(1, 10)         -- [1, 10] inclusive
local r = an:random_angle()            -- [0, 2π)
local s = an:random_sign()             -- -1 or 1
local b = an:random_bool()             -- true or false
local item = an:random_choice(array)   -- random element

-- Additional requirements mentioned:
-- - Save/restore RNG state (required for replays)
-- - Consider separate streams for gameplay vs cosmetic RNG
```

---

## Existing Anchor Implementation

### random.lua (both projects)

```lua
-- Core structure
random = class:class_new()
function random:random(seed)
  self.tags.random = true
  self.seed = seed or os.time()
  self.generator = love.math.newRandomGenerator(self.seed)
  return self
end

-- Functions provided:
random_angle()                    -- Float [0, 2π)
random_bool(chance)               -- Bool with % chance (0-100, default 50)
random_float(min, max)            -- Float [min, max) (defaults 0, 1)
random_int(min, max)              -- Int [min, max] inclusive (defaults 0, 1)
random_sign(chance)               -- -1 or 1 with % chance (0-100, default 50)
random_weighted(...)              -- Returns index based on weights (varargs)
random_subset_sum(total, n, var)  -- Partition total into n parts with variation %
```

### math.lua (noise functions)

```lua
-- love-compare version
math.perlin_noise(x, y, z, w)     -- Perlin noise [0, 1]
math.simplex_noise(x, y, z, w)    -- Simplex noise [0, 1]

-- super emoji box version (unified)
math.noise(x, y, z, w)            -- Uses love.math.noise (simplex)
```

### array.lua (random operations)

```lua
array.random(t, n, rng)           -- Pick n random elements (unique indexes)
array.remove_random(t, n, rng)    -- Pick and remove n random elements
array.shuffle(t, rng)             -- In-place Fisher-Yates shuffle
```

---

## Cross-Engine API Research

### Algorithm Choices

| Engine | Algorithm | Notes |
|--------|-----------|-------|
| LÖVE | MT19937 variant | Mersenne Twister, cross-platform consistent |
| Unity | Xorshift128 | Fast, decent quality |
| Godot | PCG32 | Excellent quality, fast, small state |
| GameMaker | Proprietary | Unknown, but deterministic |
| Defold-splitmix64 | SplitMix64 | Same output on ALL platforms |
| MonoGame | PCG | Modern, excellent stats |
| Phaser | Unknown | High quality |

**Recommendation:** PCG (Permuted Congruential Generator) — fast, excellent statistical quality, small state (64-bit), easy to save/restore.

---

### Core Functions (Universal)

Every engine provides:
- Float in [0, 1)
- Float in range [a, b)
- Integer in range [a, b]

| Engine | Float [0,1) | Float Range | Int Range |
|--------|-------------|-------------|-----------|
| LÖVE | `love.math.random()` | — | `random(min, max)` |
| Unity | `Random.value` | `Random.Range(a, b)` | `Random.Range(a, b)` |
| Godot | `randf()` | `randf_range(a, b)` | `randi_range(a, b)` |
| HaxeFlixel | `float()` | `float(min, max)` | `int(min, max)` |
| Phaser | `frac()` | `realInRange(a, b)` | `between(a, b)` |
| KaboomJS | `rand()` | `rand(a, b)` | `randi(a, b)` |
| GameMaker | — | `random_range(a, b)` | `irandom_range(a, b)` |
| p5.js | `random()` | `random(a, b)` | — |

---

### Seed/State Management

| Engine | Set Seed | Get Seed | Save/Restore State |
|--------|----------|----------|-------------------|
| LÖVE | `setRandomSeed(seed)` | — | `newRandomGenerator(seed)` |
| Unity | `InitState(seed)` | — | `Random.state` (get/set) |
| Godot | `rng.seed = value` | `rng.seed` | `rng.state` (get/set) |
| HaxeFlixel | `new FlxRandom(seed)` | `currentSeed` | — |
| Phaser | `sow(seeds)` | — | `rnd.state` (get/set) |
| GameMaker | `random_set_seed(seed)` | `random_get_seed()` | — |
| p5.js | `randomSeed(seed)` | — | — |
| KaboomJS | `randSeed(seed)` | `randSeed()` | — |

**Key insight:** Unity, Godot, and Phaser support full state save/restore for replays. This is critical for deterministic replays.

---

### Convenience Functions

#### Boolean/Chance

| Engine | Function | Parameters |
|--------|----------|------------|
| HaxeFlixel | `bool(chance)` | Chance 0-100 (default 50) |
| KaboomJS | `chance(p)` | Probability 0-1 |
| Old Anchor | `random_bool(chance)` | Chance 0-100 (default 50) |

**Design choice:** HaxeFlixel's 0-100 is more intuitive for game designers ("30% chance"). KaboomJS's 0-1 is more mathematical.

#### Sign (±1)

| Engine | Function | Parameters |
|--------|----------|------------|
| HaxeFlixel | `sign(chance)` | Chance 0-100 (default 50) |
| Phaser | — | Not built-in |
| Old Anchor | `random_sign(chance)` | Chance 0-100 (default 50) |

#### Angle/Rotation

| Engine | Function | Returns |
|--------|----------|---------|
| Phaser | `angle()` | Degrees [-180, 180] |
| Phaser | `rotation()` | Radians [-π, π] |
| Old Anchor | `random_angle()` | Radians [0, 2π) |

**Note:** Phaser uses signed angles. Old Anchor uses unsigned. Game preference.

---

### Array Operations

#### Pick from Array

| Engine | Function | Notes |
|--------|----------|-------|
| HaxeFlixel | `getObject(array, weights, start, end)` | Optional weights |
| Phaser | `pick(array)` | Simple pick |
| p5.js | `random(array)` | Overloaded |
| KaboomJS | `choose(array)` | Simple pick |
| Old Anchor | `array.random(t, n, rng)` | Pick n unique |

#### Weighted Pick

| Engine | Function | Weight Format |
|--------|----------|---------------|
| HaxeFlixel | `getObject(array, weights)` | Parallel array |
| HaxeFlixel | `weightedPick(weights)` | Returns index |
| Phaser | `weightedPick(array)` | Favors earlier entries |
| Old Anchor | `random_weighted(...)` | Varargs, returns index |

#### Shuffle

| Engine | Function | In-place? |
|--------|----------|-----------|
| HaxeFlixel | `shuffle(array)` | Yes |
| Phaser | `shuffle(array)` | Yes |
| Old Anchor | `array.shuffle(t, rng)` | Yes |

---

### Distributions

#### Normal/Gaussian

| Engine | Function | Parameters |
|--------|----------|------------|
| LÖVE | `randomNormal(stddev, mean)` | stddev=1, mean=0 |
| Godot | `randfn(mean, deviation)` | mean, deviation |
| HaxeFlixel | `floatNormal(mean, stdDev)` | Box-Muller |
| p5.js | `randomGaussian(mean, sd)` | mean=0, sd=1 |

**Use case:** Natural variation, enemy spawn patterns, particle velocities.

---

### Noise Functions

Only a few engines provide noise built-in:

| Engine | Function | Notes |
|--------|----------|-------|
| LÖVE | `love.math.noise(x,y,z,w)` | Simplex noise |
| Construct 3 | Advanced Random plugin | Perlin/Simplex |

**Implementation note:** Since we're in C, we'll need to implement noise ourselves. Options:
1. stb_perlin.h (single header, public domain)
2. FastNoiseLite (C version available)
3. Custom simplex implementation

---

### Unique/Interesting Features

#### HaxeFlixel: Excludes Array
```haxe
// Pick random int but exclude certain values
FlxG.random.int(1, 10, [3, 5, 7])  // Never returns 3, 5, or 7
```

#### Phaser: UUID Generation
```javascript
rnd.uuid()  // RFC4122 v4 UUID
```

#### GameMaker: Seed Retrieval
```gml
// Log seed for procedural generation debugging
var seed = random_get_seed()
show_debug_message("Level seed: " + string(seed))
```

#### Unity: Geometric Random
```csharp
Random.insideUnitCircle    // Random point in circle
Random.insideUnitSphere    // Random point in sphere
Random.onUnitSphere        // Random point on sphere surface
Random.rotation            // Random quaternion
```

#### p5.js: Vector Random
```javascript
p5.Vector.random2D()  // Random 2D unit vector
p5.Vector.random3D()  // Random 3D unit vector
```

---

### Multiple RNG Instances

**Engines that support independent instances:**
- LÖVE: `love.math.newRandomGenerator(seed)`
- Godot: `var rng = RandomNumberGenerator.new()`
- HaxeFlixel: `new FlxRandom(seed)`
- Phaser: `new Phaser.Math.RandomDataGenerator(seed)`

**Use cases:**
1. **Gameplay vs Cosmetic:** Particles use separate RNG so they don't affect determinism
2. **Procedural Generation:** Each chunk/level has its own seeded RNG
3. **Replay Systems:** Main RNG state is saved/restored, cosmetic RNG is not

---

## Design Decisions

### Decision 1: Algorithm Choice

**Options:**
1. **PCG32** — Fast, excellent quality, 64-bit state, easy save/restore
2. **Xorshift128** — Fast, good quality, 128-bit state
3. **SplitMix64** — Very fast, good quality, cross-platform identical
4. **Mersenne Twister** — Classic, large state (624 words)

**Recommendation:** PCG32
- Excellent statistical quality (passes BigCrush)
- Small state (64-bit) — easy to save/restore for replays
- Fast — ~1 billion numbers per second
- Simple C implementation

### Decision 2: API Style

**Option A: Method calls on `an` object (current plan)**
```lua
an:random_float(0, 100)
an:random_int(1, 10)
```

**Option B: Global functions**
```lua
random_float(0, 100)
random_int(1, 10)
```

**Option C: Shorter names (like KaboomJS)**
```lua
rand(0, 100)
randi(1, 10)
```

**Recommendation:** Keep Option A (method calls on `an`)
- Consistent with existing Anchor design
- Allows multiple RNG instances: `my_rng:random_float()`
- Clear namespace

### Decision 3: State Save/Restore

**For replay support, need:**
1. `random_get_state()` — returns state that can be serialized
2. `random_set_state(state)` — restores state

**Implementation:** PCG32 state is just two 64-bit integers — easy to serialize.

### Decision 4: Multiple RNG Streams

**Options:**
1. **Single global RNG** — Simple, most games don't need more
2. **Global + ability to create instances** — Flexible
3. **Separate gameplay/cosmetic by default** — Enforces best practice

**Recommendation:** Option 2 — Global default + ability to create instances
```lua
-- Default global
an:random_float(0, 1)

-- Create instance for procedural generation
local level_rng = random_create(12345)
level_rng:random_float(0, 1)
```

### Decision 5: Noise Functions

**Options:**
1. **Include in C (stb_perlin.h)** — Simple integration
2. **Implement custom** — More control
3. **Lua implementation** — Portable but slower

**Recommendation:** stb_perlin.h
- Single header, public domain
- Already using stb pattern (stb_image, stb_truetype)
- Perlin noise 2D/3D/4D
- Turbulence and fractal brownian motion helpers

---

## Recommended API

### Core Functions (C-side, exposed to Lua)

```lua
-- Seeding
random_seed(seed)                  -- Seed the global RNG
random_get_state()                 -- Get state for replay saving
random_set_state(state)            -- Restore state for replay

-- Basic generation
random()                           -- Float [0, 1)
random_float(min, max)             -- Float [min, max)
random_int(min, max)               -- Int [min, max] inclusive

-- Convenience
random_angle()                     -- Float [0, 2π)
random_sign(chance?)               -- -1 or 1 (chance 0-100, default 50)
random_bool(chance?)               -- Bool (chance 0-100, default 50)

-- Advanced
random_normal(mean?, stddev?)      -- Gaussian (mean=0, stddev=1)

-- Noise (via stb_perlin)
noise(x)                           -- 1D Perlin noise [-1, 1]
noise(x, y)                        -- 2D Perlin noise [-1, 1]
noise(x, y, z)                     -- 3D Perlin noise [-1, 1]

-- Multiple instances
random_create(seed)                -- Create new RNG instance
```

### Array Functions (Lua-side, using RNG)

```lua
-- These use an:random_* internally
random_choice(array)               -- Pick one random element
random_choices(array, n)           -- Pick n unique elements
random_weighted(array, weights)    -- Pick based on weights
random_weighted_index(...)         -- Legacy: varargs weights, returns index
array_shuffle(array)               -- In-place Fisher-Yates shuffle
array_remove_random(array, n?)     -- Remove and return n random elements
```

### Usage Examples

```lua
-- Basic usage
random_seed(12345)
local x = random_float(0, 100)     -- 47.832...
local i = random_int(1, 10)        -- 7
local r = random_angle()           -- 4.21...
local s = random_sign()            -- -1 or 1
local b = random_bool(30)          -- 30% chance true

-- Array operations
local enemy = random_choice(enemy_types)
local cards = random_choices(deck, 5)
local rarity = random_weighted({'common', 'rare', 'epic'}, {70, 25, 5})

-- Procedural generation with separate RNG
local level_seed = 42
local level_rng = random_create(level_seed)
for i = 1, 10 do
    local x = level_rng:random_int(0, 100)
    -- Generate level features...
end

-- Replay support
function save_replay()
    replay.rng_state = random_get_state()
    replay.inputs = recorded_inputs
end

function load_replay()
    random_set_state(replay.rng_state)
    play_inputs(replay.inputs)
end

-- Noise for terrain/effects
for x = 0, 100 do
    for y = 0, 100 do
        local height = noise(x * 0.1, y * 0.1)
        -- height is smooth [-1, 1]
    end
end
```

---

## Implementation Order

1. **PCG32 core** — Seed, next, float, int
2. **Lua bindings** — random_*, global `an` integration
3. **State save/restore** — get_state, set_state
4. **Convenience functions** — angle, sign, bool
5. **Gaussian distribution** — Box-Muller transform
6. **Array functions** — choice, shuffle (Lua-side)
7. **stb_perlin integration** — noise functions
8. **Multiple instances** — random_create

---

## References

- [LÖVE math.random](https://love2d.org/wiki/love.math.random)
- [Unity Random](https://docs.unity3d.com/ScriptReference/Random.html)
- [Godot RandomNumberGenerator](https://docs.godotengine.org/en/stable/classes/class_randomnumbergenerator.html)
- [HaxeFlixel FlxRandom](https://api.haxeflixel.com/flixel/math/FlxRandom.html)
- [Phaser RandomDataGenerator](https://docs.phaser.io/api-documentation/class/math-randomdatagenerator)
- [GameMaker Random Functions](https://manual.gamemaker.io/monthly/en/GameMaker_Language/GML_Reference/Random_Functions/)
- [KaboomJS rand](https://kaboomjs.com/)
- [PCG Paper](https://www.pcg-random.org/paper.html)
- [stb_perlin.h](https://github.com/nothings/stb/blob/master/stb_perlin.h)
