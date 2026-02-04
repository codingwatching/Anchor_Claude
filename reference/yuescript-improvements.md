# YueScript Improvements for LLM Code Generation

Findings from language design exploration. These are practical improvements to apply when refactoring main.yue.

## 1. Transform Helper (High Priority)

Add a method to layers that handles push/pop automatically to prevent matching errors:

```yuescript
-- Add to layer class in framework
layer.at = (x, y, r, sx, sy, fn) =>
  @\push x, y, r, sx, sy
  fn!
  @\pop!
```

Usage:
```yuescript
-- Instead of manual push/pop:
game\push @x, @y, 0, squash_x, squash_y
game\push 0, 0, @angle, @scale, @scale
game\image @image, 0, 0
game\pop!
game\pop!

-- Use nested callbacks:
game\at @x, @y, 0, squash_x, squash_y, ->
  game\at 0, 0, @angle, @scale, @scale, ->
    game\image @image, 0, 0
```

Nesting enforced by callback structure - can't forget a pop.

## 2. Random Shorthand Functions (Medium Priority)

```yuescript
-- Global helpers
r = (a, b) -> an.random\float a, b
ri = (a, b) -> an.random\int a, b
```

Usage:
```yuescript
pitch = r 0.95, 1.05      -- instead of an.random\float 0.95, 1.05
count = ri 2, 4           -- instead of an.random\int 2, 4
```

## 3. Constructor Section Organization (Medium Priority)

Use comment sections to organize long constructors:

```yuescript
class ball extends object
  new: (@x, @y, @team, @weapon_type='dagger') =>
    super!

    -- PROPS
    @radius = 10
    @hp = 50
    @angle = 0
    @flashing = false

    -- TUNE
    @max_hp = 50
    @base_omega = 1.5 * math.pi
    @max_omega = 3 * math.pi

    -- COMPONENTS
    @\add collider 'ball', 'dynamic', 'circle', @radius
    @\add timer!
    @\add spring!

    -- CHILDREN
    @\add hp_bar!
    @\add hp_ui @team, @hp, @max_hp
```

Sections provide landmarks for navigation and completeness checking.

## Why These Help

1. **Transform helper**: Prevents push/pop matching errors (my most common mistake)
2. **Random shorthand**: Reduces verbosity for extremely common operation
3. **Section organization**: Helps find things in existing code, prompts completeness when generating new code

## What Doesn't Need Changing

- Timer syntax is already clean: `@timer\after 0.1, -> @flash = false`
- Spawning is already clean: `@parent.effects\add hit_particle x, y`
- Collision event loops are verbose but not error-prone
