# Framework API

Complete documentation for YueScript framework classes. For quick reference signatures, see `FRAMEWORK_API_QUICK.md`.

---

## Initialization

The Anchor framework is initialized by requiring it with a configuration table:

```yuescript
require('anchor')
  width: 640
  height: 360
  title: "My Game"
  scale: 2
  vsync: true
  fullscreen: false
  resizable: true
  filter: "rough"
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `width` | int | 480 | Game resolution width |
| `height` | int | 270 | Game resolution height |
| `title` | string | "Anchor" | Window title |
| `scale` | int | 3 | Initial window scale multiplier |
| `vsync` | bool | true | Enable vertical sync |
| `fullscreen` | bool | false | Start in fullscreen mode |
| `resizable` | bool | true | Window can be resized |
| `filter` | string | "rough" | Texture filter mode: "rough" (pixel-perfect) or "smooth" (anti-aliased) |

All options are optional. Omitted options use their defaults:

```yuescript
-- Minimal initialization with defaults (480x270)
require('anchor') {}

-- Just set resolution
require('anchor')
  width: 640
  height: 360
```

---

## Root Object (an)

The global `an` object is the entry point for all framework functionality. It manages resources, physics, input, and the object tree.

### Engine State Properties

These properties provide access to engine state. Some are static (set once at init), others update every frame.

#### Static Properties (set at initialization)

| Property | Type | Description |
|----------|------|-------------|
| `an.width` | int | Game resolution width |
| `an.height` | int | Game resolution height |
| `an.dt` | number | Fixed delta time (1/120 for 120Hz physics) |
| `an.platform` | string | Platform: "web" or "windows" |

#### Dynamic Properties (updated every frame)

| Property | Type | Description |
|----------|------|-------------|
| `an.frame` | int | Current render frame number |
| `an.step` | int | Current physics step count |
| `an.time` | number | Elapsed game time in seconds |
| `an.window_width` | int | Actual window width in pixels |
| `an.window_height` | int | Actual window height in pixels |
| `an.scale` | int | Current integer render scale |
| `an.fullscreen` | bool | Whether window is fullscreen |
| `an.fps` | number | Current frames per second |
| `an.draw_calls` | int | Draw calls in previous frame |

```yuescript
-- Access engine state
print "Game size: #{an.width}x#{an.height}"
print "Time: #{an.time}, FPS: #{an.fps}"
print "Window: #{an.window_width}x#{an.window_height} @ #{an.scale}x"
```

### Resource Registration

#### an\layer(name)

Creates and registers a layer for rendering.

```yuescript
game = an\layer 'game'
ui = an\layer 'ui'
```

Layers are FBO-backed render targets. Drawing commands are queued during update and rendered later. Layers are stored in `an.layers.name`.

---

#### an\image(name, path)

Loads and registers an image (texture).

```yuescript
an\image 'player', 'assets/player.png'
an\image 'bullet', 'assets/bullet.png'

-- Access later
layer\image an.images.player, x, y
```

Images are stored in `an.images.name` with properties:
- `handle` - internal texture handle
- `width` - texture width in pixels
- `height` - texture height in pixels

---

#### an\font(name, path, size)

Loads and registers a font.

```yuescript
an\font 'main', 'assets/font.ttf', 16
an\font 'title', 'assets/title.ttf', 32

-- Access later
layer\text "Score: 100", 'main', 10, 10, white!
```

Fonts are stored in `an.fonts.name`.

---

#### an\shader(name, path)

Loads and registers a fragment shader.

```yuescript
an\shader 'blur', 'shaders/blur.frag'
an\shader 'outline', 'shaders/outline.frag'

-- Access later
layer\apply_shader an.shaders.blur
```

Shaders are stored in `an.shaders.name`.

---

#### an\sound(name, path)

Loads and registers a sound effect.

```yuescript
an\sound 'jump', 'assets/jump.ogg'
an\sound 'hit', 'assets/hit.ogg'
```

Sounds are stored in `an.sounds.name`.

---

#### an\music(name, path)

Loads and registers a music track.

```yuescript
an\music 'bgm', 'assets/music.ogg'
an\music 'boss', 'assets/boss_theme.ogg'
```

Music tracks are stored in `an.tracks.name`.

---

### Audio Playback

#### an\sound_play(name, volume?, pitch?)

Plays a sound effect.

```yuescript
an\sound_play 'jump'
an\sound_play 'hit', 0.5        -- half volume
an\sound_play 'hit', 1, 1.5     -- normal volume, higher pitch
```

**Parameters:**
- `name` - registered sound name
- `volume` - 0.0 to 1.0 (default 1.0)
- `pitch` - pitch multiplier (default 1.0)

---

#### an\music_play(name, loop?, channel?)

Plays a music track.

```yuescript
an\music_play 'bgm'              -- loops by default
an\music_play 'victory', false   -- play once
an\music_play 'bgm', true, 2     -- on channel 2
```

**Parameters:**
- `name` - registered music name
- `loop` - whether to loop (default true)
- `channel` - audio channel 1-4 (default 1)

---

#### an\music_stop(channel?)

Stops music playback.

```yuescript
an\music_stop!       -- stop channel 1
an\music_stop 2      -- stop channel 2
```

---

#### an\music_crossfade(name, duration, channel?)

Crossfades from current music to a new track.

```yuescript
an\music_crossfade 'boss_theme', 2    -- 2 second crossfade
```

**Parameters:**
- `name` - target track name
- `duration` - crossfade time in seconds
- `channel` - audio channel (default 1)

---

#### Playlist System

```yuescript
-- Setup
an\playlist_set {'track1', 'track2', 'track3'}
an\playlist_shuffle true
an\playlist_set_crossfade 2

-- Playback
an\playlist_play!
an\playlist_stop!
an\playlist_next!
an\playlist_prev!

-- Query
current = an\playlist_current_track!
```

---

### Physics Setup

#### an\physics_init()

Initializes the physics world. Must be called before any physics operations.

```yuescript
an\physics_init!
an\physics_set_gravity 0, 500
an\physics_set_meter_scale 64
```

---

#### an\physics_set_gravity(gx, gy)

Sets world gravity in pixels per second squared.

```yuescript
an\physics_set_gravity 0, 500    -- normal downward gravity
an\physics_set_gravity 0, 0      -- space
```

---

#### an\physics_set_meter_scale(scale)

Sets pixels per meter for physics simulation. Box2D works in meters internally.

```yuescript
an\physics_set_meter_scale 64    -- 64 pixels = 1 meter
```

---

#### an\physics_tag(name)

Registers a collision tag. Tags must be registered before use.

```yuescript
an\physics_tag 'player'
an\physics_tag 'enemy'
an\physics_tag 'wall'
an\physics_tag 'bullet'
```

---

#### an\physics_collision(tag_a, tag_b)

Enables physical collision between two tags. Bodies will collide and generate collision events.

```yuescript
an\physics_collision 'player', 'wall'
an\physics_collision 'player', 'enemy'
an\physics_collision 'bullet', 'enemy'
```

---

#### an\physics_sensor(tag_a, tag_b)

Enables sensor overlap detection. Bodies pass through each other but generate sensor events.

```yuescript
an\physics_sensor 'player', 'coin'
an\physics_sensor 'player', 'danger_zone'
```

---

#### an\physics_hit(tag_a, tag_b)

Enables hit events for physical collisions. Hit events include approach speed for impact-based effects.

```yuescript
an\physics_hit 'player', 'wall'
an\physics_hit 'enemy', 'bullet'
```

---

### Physics Events

Events are queried each frame to handle physics interactions. Each event contains the parent objects (not colliders).

#### an\collision_begin_events(tag_a, tag_b)

Returns collisions that started this frame.

```yuescript
for event in *an\collision_begin_events 'player', 'enemy'
  event.a\take_damage!  -- player
  event.b\knockback!    -- enemy
  -- Also available: point_x, point_y, normal_x, normal_y
```

---

#### an\collision_end_events(tag_a, tag_b)

Returns collisions that ended this frame.

```yuescript
for event in *an\collision_end_events 'player', 'platform'
  event.a.on_ground = false
```

---

#### an\sensor_begin_events(tag_a, tag_b)

Returns sensor overlaps that started this frame.

```yuescript
for event in *an\sensor_begin_events 'player', 'coin'
  event.b\collect!
  event.a.score += 10
```

---

#### an\sensor_end_events(tag_a, tag_b)

Returns sensor overlaps that ended this frame.

```yuescript
for event in *an\sensor_end_events 'player', 'water'
  event.a\exit_water!
```

---

#### an\hit_events(tag_a, tag_b)

Returns hit events with approach speed (requires physics_hit enabled).

```yuescript
for event in *an\hit_events 'player', 'wall'
  if event.approach_speed > 300
    event.a\take_fall_damage!
    spawn_impact event.point_x, event.point_y
```

---

### Spatial Queries

Query the physics world for objects at positions or shapes. Returns arrays of parent objects.

#### an\query_point(x, y, tags)

```yuescript
for obj in *an\query_point mouse_x, mouse_y, 'button'
  obj\hover!
```

---

#### an\query_circle(x, y, radius, tags)

```yuescript
for enemy in *an\query_circle explosion_x, explosion_y, 100, 'enemy'
  enemy\take_damage 50
```

---

#### an\query_aabb(x, y, w, h, tags)

Axis-aligned bounding box query.

```yuescript
for obj in *an\query_aabb camera_x, camera_y, camera_w, camera_h, 'drawable'
  obj.visible = true
```

---

#### an\query_box(x, y, w, h, angle, tags)

Rotated box query.

```yuescript
for enemy in *an\query_box x, y, 50, 100, player_angle, 'enemy'
  enemy\hit_by_sword!
```

---

#### an\query_capsule(x1, y1, x2, y2, radius, tags)

Capsule shape query (two endpoints + radius).

```yuescript
for obj in *an\query_capsule start_x, start_y, end_x, end_y, 10, 'enemy'
  obj\in_laser_path!
```

---

#### an\query_polygon(x, y, vertices, tags)

Polygon shape query. Vertices are a flat array: `{x1, y1, x2, y2, ...}`

```yuescript
verts = {0, 0, 100, 0, 50, 100}
for enemy in *an\query_polygon x, y, verts, 'enemy'
  enemy\take_damage!
```

---

#### an\raycast(x1, y1, x2, y2, tags)

Casts a ray and returns the first hit, or nil.

```yuescript
hit = an\raycast player_x, player_y, target_x, target_y, 'wall'
if hit
  draw_laser player_x, player_y, hit.point_x, hit.point_y
  hit.object\take_damage!
```

**Returns:** `{object, shape, point_x, point_y, normal_x, normal_y, fraction}` or nil

---

#### an\raycast_all(x1, y1, x2, y2, tags)

Casts a ray and returns all hits.

```yuescript
for hit in *an\raycast_all x1, y1, x2, y2, 'enemy'
  hit.object\pierce_damage 10
```

---

### Input Binding

The action system maps physical inputs to named actions.

#### an\bind(action, control)

Binds a control to an action.

```yuescript
an\bind 'jump', 'space'
an\bind 'jump', 'gamepad_a'
an\bind 'left', 'a'
an\bind 'left', 'left'
an\bind 'fire', 'mouse_1'
```

**Control names:**
- Keyboard: 'a'-'z', '0'-'9', 'space', 'escape', 'return', 'left', 'right', 'up', 'down', etc.
- Mouse: 'mouse_1', 'mouse_2', 'mouse_3', 'mouse_wheel_up', 'mouse_wheel_down'
- Gamepad: 'gamepad_a', 'gamepad_b', 'gamepad_x', 'gamepad_y', 'gamepad_lb', 'gamepad_rb', etc.

---

#### an\unbind(action, control?)

Removes a binding.

```yuescript
an\unbind 'jump', 'space'   -- remove specific binding
an\unbind 'jump'            -- remove all bindings for action
```

---

#### an\bind_chord(name, actions)

Creates a chord (simultaneous key press).

```yuescript
an\bind_chord 'dash', {'shift', 'space'}
```

---

#### an\bind_sequence(name, sequence)

Creates a sequence (timed key presses).

```yuescript
an\bind_sequence 'hadouken', {'down', 0.1, 'down_forward', 0.1, 'forward', 0.1, 'punch'}
```

---

#### an\bind_hold(name, duration, source_action)

Creates a hold action (press and hold).

```yuescript
an\bind_hold 'charge_attack', 0.5, 'attack'
```

---

#### Action Queries

```yuescript
if an\is_pressed 'jump'    -- true on the frame pressed
  player\jump!

if an\is_down 'fire'       -- true while held
  player\shoot!

if an\is_released 'crouch' -- true on the frame released
  player\stand_up!

if an\any_pressed!         -- any action pressed
  start_game!

action = an\get_pressed_action!  -- which action was pressed
```

---

#### an\get_axis(negative, positive)

Returns -1 to 1 based on two opposing actions.

```yuescript
horizontal = an\get_axis 'left', 'right'
player.vx = horizontal * speed
```

---

#### an\get_vector(left, right, up, down)

Returns normalized x, y from four directional actions.

```yuescript
dx, dy = an\get_vector 'left', 'right', 'up', 'down'
player.vx = dx * speed
player.vy = dy * speed
```

---

### Raw Input State

Direct input queries without the action system.

```yuescript
-- Keyboard
if an\key_is_pressed 'escape'
  pause_game!

-- Mouse
if an\mouse_is_down 1
  shoot!

mx, my = an\mouse_position!
dx, dy = an\mouse_delta!
wx, wy = an\mouse_wheel!
```

---

### Global Actions

Register callbacks that run every frame at different phases.

```yuescript
-- Runs before object updates (good for physics events)
an\early_action 'handle_collisions', =>
  for event in *an\collision_begin_events 'player', 'enemy'
    event.a\hit!

-- Runs during main update phase
an\action 'spawn', =>
  if an\key_is_pressed 'k'
    an\add enemy math.random(0, 480), 0

-- Runs after object updates (good for rendering)
an\late_action 'draw', =>
  for obj in *an\all 'drawable'
    obj\draw game
```

---

### Object Management

#### an\add(object)

Adds an object to the tree as a child of `an`.

```yuescript
player = an\add player 100, 100
enemy = an\add enemy 300, 200
```

---

#### an\all(tag)

Returns all objects with a tag.

```yuescript
for enemy in *an\all 'enemy'
  enemy\update dt

for drawable in *an\all 'drawable'
  drawable\draw layer
```

---

### Engine State

The `an` object exposes engine state:

```yuescript
an.width          -- game width in pixels
an.height         -- game height in pixels
an.dt             -- delta time
an.time           -- total elapsed time
an.frame          -- frame counter
an.step           -- physics step counter
an.fps            -- current FPS
an.platform       -- 'desktop' or 'web'
an.window_width   -- actual window width
an.window_height  -- actual window height
an.scale          -- render scale
an.fullscreen     -- fullscreen state
an.draw_calls     -- draw calls last frame
```

---

## Object

Base class for all game objects. Objects form a tree structure with automatic lifecycle management.

### Creating Objects

```yuescript
class enemy extends object
  new: (@x, @y, @type) =>
    super!                      -- call object constructor
    @\tag 'enemy', 'drawable'   -- add tags
    @hp = 100
    @\add collider 'enemy', 'dynamic', 'circle', 16
    @\add timer!

  action: (dt) =>
    -- per-frame logic

  draw: (layer) =>
    layer\circle @x, @y, 16, red!
```

---

### Lifecycle

#### object\add(child)

Adds a child object. The child's `parent` is set automatically.

```yuescript
@\add timer!                    -- named 'timer', access as @timer
@\add collider 'player', 'dynamic', 'circle', 16
@\add spring!

-- Custom name
hitbox = collider 'hitbox', 'dynamic', 'box', 32, 16
hitbox.name = 'hitbox'
@\add hitbox                    -- access as @hitbox
```

---

#### object\all(tag)

Returns children with a tag.

```yuescript
for timer in *@\all 'timer'
  timer\cancel_all!
```

---

#### object\kill()

Marks the object for removal. Children are killed recursively.

```yuescript
if @hp <= 0
  @\kill!
```

---

### Tags & Types

#### object\tag(tags...)

Adds tags to the object.

```yuescript
@\tag 'enemy'
@\tag 'enemy', 'drawable', 'boss'
```

---

#### object\is(tag)

Checks if the object has a tag.

```yuescript
if obj\is 'enemy'
  obj\take_damage damage
```

---

### Linking

#### object\link(name, target)

Creates a reference that auto-clears when target dies.

```yuescript
@\link 'target', enemy

-- In update
if @target
  move_toward @target.x, @target.y
else
  find_new_target!
```

---

### Property Setting

#### object\set(properties)

Batch-sets properties.

```yuescript
@\set {
  x: 100
  y: 200
  speed: 50
  hp: 100
}
```

---

### Build Pattern

#### object\build()

Returns self for chained construction.

```yuescript
enemy = an\add (enemy 100, 200)\build!
```

Override `build` for post-construction setup that needs the parent:

```yuescript
class bullet extends object
  build: =>
    @collider\set_position @parent.x, @parent.y
    @
```

---

### State Transitions

#### object\flow_to(state_name)

Transitions to a named state. Calls `enter_<state>` and sets up `<state>_action`.

```yuescript
class player extends object
  new: =>
    super!
    @\flow_to 'idle'

  enter_idle: =>
    @animation = 'idle'

  idle_action: (dt) =>
    if an\is_pressed 'jump'
      @\flow_to 'jumping'

  enter_jumping: =>
    @vy = -200

  jumping_action: (dt) =>
    if @on_ground
      @\flow_to 'idle'
```

---

### Action Phases

Objects can register callbacks at different update phases:

```yuescript
-- Before main update (physics events, early logic)
@\early_action =>
  @on_ground = false

-- Main update (game logic)
@\action (dt) =>
  @x += @vx * dt

-- After main update (rendering, late effects)
@\late_action =>
  layer\draw @x, @y
```

Named actions can be overwritten:

```yuescript
@\action 'movement', (dt) =>
  @x += @vx * dt

-- Later, replace it
@\action 'movement', (dt) =>
  @x += @vx * dt * 2
```

---

### Aliases

Short aliases for common methods (useful in compact code):

```yuescript
@\T 'enemy', 'boss'     -- tag
@\Y 'enemy'             -- is (tYpe check)
@\U (dt) =>             -- early_action (Update early)
@\E (dt) =>             -- action (Execute)
@\X (dt) =>             -- late_action (eXit/late)
@\L 'target', enemy     -- link
@\A timer!              -- add
@\F 'jumping'           -- flow_to
@\K!                    -- kill
```

---

## Layer

Layers are FBO-backed render targets for queuing and compositing draw calls.

### Creating Layers

```yuescript
game = an\layer 'game'
ui = an\layer 'ui'
ui.camera = nil    -- disable camera for UI layer
```

---

### Drawing Primitives

#### layer\rectangle(x, y, w, h, color)

```yuescript
layer\rectangle 0, 0, 100, 50, red!
```

---

#### layer\circle(x, y, radius, color)

```yuescript
layer\circle 50, 50, 25, blue!
```

---

#### layer\rectangle_gradient_h(x, y, w, h, color1, color2)

Draws a filled rectangle with horizontal gradient (left to right).

```yuescript
layer\rectangle_gradient_h 0, 0, 100, 50, red!, blue!
```

---

#### layer\rectangle_gradient_v(x, y, w, h, color1, color2)

Draws a filled rectangle with vertical gradient (top to bottom).

```yuescript
-- Sky gradient: light blue at top to darker blue at bottom
bg\rectangle_gradient_v 0, 0, gw, gh, color(135, 206, 235)!, color(25, 25, 112)!
```

---

#### layer\image(image, x, y, color?, flash?)

```yuescript
layer\image an.images.player, @x, @y
layer\image an.images.player, @x, @y, white!            -- with tint
layer\image an.images.player, @x, @y, nil, white!       -- flash white
```

**Parameters:**
- `image` - image object from `an.images`
- `x, y` - position (center of image)
- `color` - optional tint color
- `flash` - optional flash color (replaces all pixels)

---

#### layer\spritesheet(spritesheet, frame, x, y, color?, flash?)

Draws a specific frame from a spritesheet.

```yuescript
layer\spritesheet an.spritesheets.hit, 1, @x, @y
layer\spritesheet an.spritesheets.hit, 3, @x, @y, white!, red!  -- tint and flash
```

**Parameters:**
- `spritesheet` - spritesheet object from `an.spritesheets`
- `frame` - frame number (1-indexed)
- `x, y` - position (center of frame)
- `color` - optional tint color
- `flash` - optional flash color

---

#### layer\animation(animation, x, y, color?, flash?)

Draws an animation's current frame.

```yuescript
layer\animation @hit1, @x, @y
layer\animation @walk, @x, @y, nil, @flashing and white!
```

**Parameters:**
- `animation` - animation object
- `x, y` - position (center of frame)
- `color` - optional tint color
- `flash` - optional flash color

---

#### layer\text(text, font_name, x, y, color)

```yuescript
layer\text "Score: #{score}", 'main', 10, 10, white!
```

---

### Transform Stack

Push/pop transforms for hierarchical drawing.

```yuescript
layer\push @x, @y, @rotation, @scale_x, @scale_y
layer\image an.images.player, 0, 0    -- draws at pushed transform
layer\pop!
```

**Parameters:**
- `x, y` - translation (default 0, 0)
- `r` - rotation in radians (default 0)
- `sx, sy` - scale (default 1, 1)

---

### Blend Modes

```yuescript
layer\set_blend_mode 'alpha'       -- default
layer\set_blend_mode 'add'         -- additive blending
layer\set_blend_mode 'multiply'    -- multiply blending
```

---

### Stencil Masking

Use stencil buffer to mask drawing to specific shapes.

```yuescript
-- Draw only inside a circular mask
layer\stencil_mask!
layer\circle 150, 150, 50, white!   -- define mask shape (not visible)
layer\stencil_test!
layer\image an.images.background, 150, 150  -- only visible inside circle
layer\stencil_off!
```

**stencil_mask!** — Subsequent draws write to stencil only (not visible).

**stencil_test!** — Subsequent draws only appear where stencil was set.

**stencil_off!** — Return to normal drawing.

---

### Shader Effects

Apply shaders as post-processing to the layer.

```yuescript
-- Apply shader with uniforms
layer\shader_set_float an.shaders.blur, 'u_radius', 5
layer\shader_set_vec2 an.shaders.blur, 'u_direction', 1, 0
layer\apply_shader an.shaders.blur

-- Apply another shader
layer\shader_set_vec4 an.shaders.color_shift, 'u_color', 1, 0.5, 0, 1
layer\apply_shader an.shaders.color_shift

-- Reset all effects
layer\reset_effects!
```

---

### Rendering Pipeline

```yuescript
-- 1. Queue drawing commands during update
game\rectangle 0, 0, 100, 100, red!
game\image an.images.player, player.x, player.y

-- 2. Render queued commands to FBO
game\render!

-- 3. Optionally create derived layers
shadow\clear!
shadow\draw_from game, an.shaders.shadow

-- 4. Composite to screen
game\draw!
shadow\draw 4, 4    -- with offset
ui\draw!
```

---

## Spritesheet

Spritesheets are textures divided into a grid of frames for animations.

### Registration

```yuescript
an\spritesheet 'hit', 'assets/hit.png', 96, 48   -- name, path, frame_width, frame_height
```

Spritesheets are stored in `an.spritesheets.name`.

### Properties

```yuescript
sheet = an.spritesheets.hit
sheet.handle        -- internal spritesheet handle
sheet.frame_width   -- width of each frame in pixels
sheet.frame_height  -- height of each frame in pixels
sheet.frames        -- total number of frames
```

Frames are indexed 1-based, read left-to-right, top-to-bottom.

---

## Animation

Frame-based animations from spritesheets with configurable timing and callbacks.

### Creating Animations

Add an animation as a child object:

```yuescript
-- Basic: spritesheet name, delay per frame, loop mode
@\add animation 'hit1', 0.05, 'once'

-- With per-frame callbacks (frame 0 = completion)
@\add animation 'hit1', 0.05, 'once',
  [3]: => @\flash!        -- called on frame 3
  [0]: => @\kill!         -- called when animation completes
```

The animation is accessible as `@hit1` (using the spritesheet name).

**Loop modes:**
- `'once'` — Plays once, then kills itself
- `'loop'` — Repeats indefinitely (default)
- `'bounce'` — Ping-pongs back and forth

### Updating & Drawing

```yuescript
update: (dt) =>
  @hit1\update dt

  layer\push @x, @y, @rotation, @scale, @scale
  layer\animation @hit1, 0, 0
  layer\pop!
```

### Control Methods

```yuescript
@hit1\play!              -- resume playback
@hit1\stop!              -- pause (keeps position)
@hit1\reset!             -- restart from frame 1
@hit1\set_frame 3        -- jump to specific frame
```

### Properties

```yuescript
@hit1.frame       -- current frame number (1-indexed)
@hit1.playing     -- whether animation is playing
@hit1.dead        -- true when 'once' animation completes
@hit1.direction   -- play direction (1 or -1 for bounce)
```

---

## Collider

Physics body wrapper for Box2D integration.

### Creating Colliders

```yuescript
-- As child object
@\add collider 'player', 'dynamic', 'circle', 16

-- With options
@\add collider 'sensor', 'static', 'box', 100, 100, {sensor: true}

-- Multiple shapes
@\add collider 'player', 'dynamic', 'circle', 16
@collider\add_box 'feet', 10, 5, {offset_y: 16}
```

**Body types:**
- `'static'` - doesn't move (walls, platforms)
- `'dynamic'` - full physics simulation
- `'kinematic'` - moves but not affected by forces

**Shape types:**
- `'circle'` - args: `radius`
- `'box'` - args: `width, height`
- `'capsule'` - args: `length, radius`
- `'polygon'` - args: `vertices` (flat array: `{x1, y1, x2, y2, ...}`)

---

### Position & Velocity

```yuescript
-- Position
x, y = @collider\get_position!
@collider\set_position 100, 200
angle = @collider\get_angle!
@collider\set_angle math.pi / 4
@collider\set_transform 100, 200, 0

-- Velocity
vx, vy = @collider\get_velocity!
@collider\set_velocity 100, 0
av = @collider\get_angular_velocity!
@collider\set_angular_velocity 2
```

---

### Forces & Impulses

```yuescript
-- Forces (continuous, affected by mass)
@collider\apply_force 100, 0           -- rightward force
@collider\apply_force_at 0, -100, @x + 10, @y   -- at point

-- Impulses (instant velocity change)
@collider\apply_impulse 50, -100       -- jump
@collider\apply_impulse_at 10, 0, @x, @y + 5

-- Torque
@collider\apply_torque 10
@collider\apply_angular_impulse 5
```

---

### Body Properties

```yuescript
@collider\set_linear_damping 0.5       -- air resistance
@collider\set_angular_damping 0.2      -- rotation damping
@collider\set_gravity_scale 0.5        -- half gravity
@collider\set_gravity_scale 0          -- no gravity
@collider\set_fixed_rotation true      -- no rotation
@collider\set_bullet true              -- continuous collision detection

mass = @collider\get_mass!
@collider\set_center_of_mass 0, 0      -- override computed center of mass (in pixels)
type = @collider\get_body_type!        -- 'static', 'dynamic', 'kinematic'
awake = @collider\is_awake!
@collider\set_awake true
```

---

### Shape Properties

```yuescript
@collider\set_friction 0.5
@collider\set_restitution 0.8    -- bounciness
@collider\set_density 1.0        -- affects mass
```

---

### Adding Extra Shapes

```yuescript
@collider\add_circle 'head', 8, {offset_y: -16}
@collider\add_box 'body', 16, 24
@collider\add_capsule 'arm', 20, 4, {offset_x: 12}

verts = {-16, -16, 16, -16, 16, 16, -16, 16}
@collider\add_polygon 'hitbox', verts
```

---

## Timer

Scheduling system for delayed and repeating callbacks.

### Creating Timers

```yuescript
@\add timer!

-- Access as @timer
@timer\after 1, -> print "1 second later"
```

---

### One-shot Timers

#### timer\after(delay, name_or_callback, callback?)

```yuescript
@timer\after 2, -> @\explode!

-- Named (can be cancelled/replaced)
@timer\after 0.5, 'invincibility', -> @invincible = false
```

---

### Repeating Timers

#### timer\every(interval, name_or_callback, callback?, times?)

```yuescript
-- Forever
@timer\every 1, -> spawn_enemy!

-- Limited times
@timer\every 0.5, -> @\shoot!, 3    -- shoot 3 times

-- Named
@timer\every 0.1, 'spawn', -> spawn_particle!, 10
```

---

#### timer\every_step(name_or_callback, callback?)

Runs every frame.

```yuescript
@timer\every_step -> @trail[] = {@x, @y}
```

---

### Duration-based

#### timer\during(duration, name_or_callback, callback?, after?)

Called every frame for a duration.

```yuescript
-- Callback receives (elapsed_time, progress 0-1)
@timer\during 2, (t, p) =>
  @alpha = 1 - p
, -> @\kill!
```

---

#### timer\during_step(name_or_callback, callback?, after?)

Runs every frame until cancelled.

```yuescript
@timer\during_step 'flicker', =>
  @visible = not @visible
```

---

### Tweening

#### timer\tween(duration, target, properties, easing?, name?, after?)

Animates properties over time.

```yuescript
@timer\tween 0.5, @, {x: 100, y: 200}                    -- linear
@timer\tween 1, @, {scale: 2}, math.quad_out            -- with easing
@timer\tween 1, @, {alpha: 0}, math.sine_in_out, 'fade', -> @\kill!
```

---

### Conditional

#### timer\watch(condition, name_or_callback, callback?)

Runs callback once when condition becomes true.

```yuescript
@timer\watch (-> @hp <= 0), -> @\die!
```

---

#### timer\when(condition, name_or_callback, callback?)

Runs callback every frame while condition is true.

```yuescript
@timer\when (-> @on_fire), =>
  @hp -= 1
  spawn_flame @x, @y
```

---

#### timer\cooldown(duration, name)

Returns true once, then false until duration passes.

```yuescript
if @timer\cooldown 0.5, 'shoot'
  @\shoot!
```

---

### Timer Control

```yuescript
@timer\cancel 'spawn'           -- cancel specific timer
@timer\trigger 'spawn'          -- trigger immediately
@timer\set_multiplier 0.5       -- slow down all timers
time = @timer\get_time_left 'spawn'
```

---

## Camera

Manages view transformation and coordinate conversion.

### Creating Camera

```yuescript
an\add camera!
an.camera\add shake!    -- optional shake effects
```

---

### Properties

```yuescript
an.camera.x = 100           -- camera center X
an.camera.y = 200           -- camera center Y
an.camera.rotation = 0.1    -- rotation in radians
an.camera.zoom = 1.5        -- zoom factor

-- Read-only
an.camera.w                 -- view width
an.camera.h                 -- view height
an.camera.mouse.x           -- mouse X in world coords
an.camera.mouse.y           -- mouse Y in world coords
```

---

### Coordinate Conversion

```yuescript
-- Screen position -> world position
world_x, world_y = an.camera\to_world screen_x, screen_y

-- World position -> screen position
screen_x, screen_y = an.camera\to_screen world_x, world_y
```

---

### Following

```yuescript
-- Follow a target with smoothing
an.camera\follow player, 0.9, 0.1    -- 90% of distance in 0.1 seconds

-- Set bounds to prevent camera from leaving area
an.camera\set_bounds 0, 0, level_width, level_height
```

---

### Attaching to Layers

```yuescript
an.camera\attach game_layer    -- apply camera transform
-- draw world objects
an.camera\detach game_layer

-- UI layer typically has no camera
ui_layer.camera = nil
```

---

## Spring

Damped spring animation system.

### Creating Springs

```yuescript
@\add spring!

-- Access the default 'main' spring
scale = @spring.main.x

-- Add more springs
@spring\add 'rotation', 0, 3, 0.5    -- value, frequency, bounce
@spring\add 'x_offset', 0, 8, 0.3
```

**Parameters:**
- `name` - identifier
- `x` - initial value (default 0)
- `frequency` - oscillations per second (default 5)
- `bounce` - 0 to 1, where 0=no overshoot, 1=infinite oscillation (default 0.5)

---

### Pulling Springs

```yuescript
-- Add impulse (spring will oscillate back to target)
@spring\pull 'main', 0.3

-- With new frequency/bounce
@spring\pull 'main', 0.5, 10, 0.7
```

---

### Changing Target

```yuescript
-- Spring will animate toward new value
@spring\set_target 'scale', 2
```

---

### Checking State

```yuescript
if @spring\at_rest 'main'
  print "animation complete"
```

---

### Using in Draw

```yuescript
draw: (layer) =>
  scale = @spring.main.x
  layer\push @x, @y, @spring.rotation.x, scale, scale
  layer\image an.images.player, 0, 0
  layer\pop!
```

---

## Shake

Camera shake effects for juice and impact feedback.

### Creating Shake

```yuescript
an.camera\add shake!
```

---

### Trauma-based Shake

Perlin noise shake that accumulates and decays.

```yuescript
an.camera.shake\trauma 0.5, 0.3    -- amount, duration

-- Configure amplitude
an.camera.shake\set_trauma_parameters {
  x: 20
  y: 20
  rotation: 0.1
  zoom: 0.05
}
```

---

### Directional Shakes

```yuescript
-- Spring push (oscillates and settles)
an.camera.shake\push angle, 20, 5, 0.5    -- angle, amount, frequency, bounce

-- Random jitter
an.camera.shake\shake 15, 0.5, 60    -- amplitude, duration, frequency

-- Sine wave
an.camera.shake\sine angle, 15, 8, 0.5    -- angle, amplitude, frequency, duration

-- Square wave
an.camera.shake\square angle, 15, 8, 0.5
```

---

### Handcam

Continuous subtle motion for immersion.

```yuescript
an.camera.shake\handcam true
an.camera.shake\handcam true, {x: 3, y: 3, rotation: 0.02}, 0.5
an.camera.shake\handcam false
```

---

## Random

Seeded random number generator for deterministic randomness.

### Creating Random

```yuescript
@\add random!               -- uses os.time
@\add random 12345          -- deterministic seed

-- Access as @random
value = @random\float 0, 100
```

---

### Number Generation

```yuescript
@random\float!              -- 0 to 1
@random\float 10            -- 0 to 10
@random\float 5, 10         -- 5 to 10

@random\int 10              -- 1 to 10
@random\int 5, 10           -- 5 to 10

@random\angle!              -- 0 to 2*pi
@random\sign!               -- -1 or 1 (50% each)
@random\sign 75             -- -1 or 1 (75% for 1)
@random\bool!               -- true/false (50% each)
@random\bool 10             -- true/false (10% for true)

@random\normal!             -- gaussian, mean=0, stddev=1
@random\normal 100, 15      -- mean=100, stddev=15
```

---

### Selection

```yuescript
enemy = @random\choice enemies
items = @random\choices loot_table, 3

-- Weighted selection (returns index)
weights = {1, 2, 7}  -- 10%, 20%, 70%
index = @random\weighted weights
```

---

### Seed Control

```yuescript
seed = @random\get_seed!
@random\set_seed 12345
```

---

## Color

Mutable color with RGB and HSL access.

### Creating Colors

```yuescript
white = color!                    -- 255, 255, 255, 255
red = color 255, 0, 0             -- opaque red
transparent = color 0, 0, 255, 128
```

---

### Properties

All properties are read/write. RGB and HSL stay synchronized.

```yuescript
c = color 255, 0, 0

-- RGB (0-255)
c.r, c.g, c.b, c.a

-- HSL
c.h    -- hue 0-360
c.s    -- saturation 0-1
c.l    -- lightness 0-1

-- Modify
c.r = 200
c.l = 0.8    -- makes it lighter, RGB updates automatically
```

---

### Using for Drawing

Call the color to get a packed RGBA value:

```yuescript
layer\circle x, y, r, red!
layer\rectangle 0, 0, w, h, my_color!
```

---

### Operators

All operators mutate in place and return self for chaining:

```yuescript
red = red * 0.5           -- darken
red = red / 2             -- darken
red = red + 50            -- brighten all channels
red = red - 30            -- darken all channels

result = color1 * color2  -- component-wise multiply
result = color1 + color2  -- component-wise add
```

---

### Methods

```yuescript
copy = red\clone!         -- independent copy

red\invert!               -- 255-r, 255-g, 255-b

red\mix blue, 0.5         -- 50% toward blue
red\mix blue, 0           -- stays red
red\mix blue, 1           -- becomes blue
```

---

## Math Extensions

### Interpolation

```yuescript
-- Linear interpolation
math.lerp 0.5, 0, 100       -- 50

-- Frame-rate independent lerp
-- Covers p% of distance in t seconds
x = math.lerp_dt 0.9, 1, dt, x, target   -- 90% in 1 second

-- Angle interpolation (shortest path)
math.lerp_angle 0.5, 0, math.pi          -- pi/2
math.lerp_angle_dt 0.9, 1, dt, angle, target_angle
```

---

### Utility

```yuescript
math.clamp value, 0, 100     -- keep in range
math.remap 10, 0, 20, 0, 1   -- 0.5 (10 is 50% of [0,20], maps to 50% of [0,1])
math.remap speed, 0, 512, 0, 100  -- convert speed to percentage
math.loop angle, 2 * math.pi -- wrap to range
math.length 3, 4             -- 5 (vector magnitude: sqrt(x*x + y*y))
math.sign -5                 -- -1 (returns -1, 0, or 1)
math.sign 0                  -- 0
math.sign 42                 -- 1
```

---

### Easing Functions

All take t (0-1) and return transformed t:

```yuescript
math.linear(t)
math.sine_in(t), math.sine_out(t), math.sine_in_out(t), math.sine_out_in(t)
math.quad_in(t), math.quad_out(t), math.quad_in_out(t), math.quad_out_in(t)
math.cubic_in(t), math.cubic_out(t), math.cubic_in_out(t), math.cubic_out_in(t)
math.quart_in(t), math.quart_out(t), math.quart_in_out(t), math.quart_out_in(t)
math.quint_in(t), math.quint_out(t), math.quint_in_out(t), math.quint_out_in(t)
math.expo_in(t), math.expo_out(t), math.expo_in_out(t), math.expo_out_in(t)
math.circ_in(t), math.circ_out(t), math.circ_in_out(t), math.circ_out_in(t)
math.bounce_in(t), math.bounce_out(t), math.bounce_in_out(t), math.bounce_out_in(t)
math.back_in(t), math.back_out(t), math.back_in_out(t), math.back_out_in(t)
math.elastic_in(t), math.elastic_out(t), math.elastic_in_out(t), math.elastic_out_in(t)
```

Usage with tween:

```yuescript
@timer\tween 1, @, {x: 100}, math.quad_out
@timer\tween 0.5, @, {scale: 2}, math.bounce_out
```

---

## Array Utilities

All functions operate on Lua array tables. Modifying functions change the array in place.

### Predicates

```yuescript
array.all {1, 2, 3}, (v) -> v > 0         -- true
array.any {1, 2, 3}, (v) -> v > 2         -- true
array.has {1, 2, 3}, 2                    -- true
array.has {1, 2, 3}, (v) -> v > 2         -- true
```

---

### Aggregation

```yuescript
array.count {1, 1, 2}                     -- 3
array.count {1, 1, 2}, 1                  -- 2
array.count {1, 2, 3, 4}, (v) -> v > 2    -- 2

array.sum {1, 2, 3}                       -- 6
array.sum objects, (v) -> v.score         -- sum of scores

array.average {1, 3}                      -- 2

array.max {1, 5, 3}                       -- 5
array.max objects, (v) -> v.hp            -- object with max hp
```

---

### Search

```yuescript
array.index {2, 1, 2}, 2                  -- 1 (first occurrence)
array.index {1, 2, 3}, (v) -> v > 2       -- 3

array.get {4, 3, 2, 1}, 1                 -- 4
array.get {4, 3, 2, 1}, -1                -- 1 (last)
array.get {4, 3, 2, 1}, 1, 3              -- {4, 3, 2} (range)
array.get {4, 3, 2, 1}, -2, -1            -- {2, 1}

array.get_circular_buffer_index {a, b, c}, 4   -- 1 (wraps)
```

---

### Modification

```yuescript
array.delete t, value                     -- remove all instances
array.remove t, index                     -- remove at index
array.reverse t                           -- in place
array.reverse t, 1, 2                     -- partial
array.rotate t, 1                         -- shift right
array.rotate t, -1                        -- shift left
array.shuffle t                           -- randomize
```

---

### Selection

```yuescript
array.random t                            -- random element
array.random t, 3                         -- 3 unique elements

array.remove_random t                     -- remove and return random
array.remove_random t, 3                  -- remove and return 3
```

---

### Transformation

```yuescript
array.flatten {1, {2, {3}}}               -- {1, 2, 3}
array.flatten {1, {2, {3}}}, 1            -- {1, 2, {3}} (1 level)

array.join {1, 2, 3}                      -- '123'
array.join {1, 2, 3}, ', '                -- '1, 2, 3'
```

---

### Table Utilities

```yuescript
copy = table.copy original                -- deep copy
str = table.tostring t                    -- debug string
```

---

### YueScript Idioms

These common operations use native YueScript syntax instead of array functions:

```yuescript
-- Map
doubled = [v * 2 for v in *numbers]

-- Filter
evens = [v for v in *numbers when v % 2 == 0]

-- Copy
copy = [v for v in *original]

-- Indexes matching condition
indexes = [i for i, v in ipairs t when v > 10]
```
