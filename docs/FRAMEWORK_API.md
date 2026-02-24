# Framework API

Complete documentation for Lua framework classes. For quick reference signatures, see `FRAMEWORK_API_QUICK.md`.

---

## Initialization

The Anchor framework is initialized by requiring it with a configuration table:

```lua
require('anchor') {
  width = 640,
  height = 360,
  title = "My Game",
  scale = 2,
  vsync = true,
  fullscreen = false,
  resizable = true,
  filter = "rough",
}
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

```lua
-- Minimal initialization with defaults (480x270)
require('anchor') {}

-- Just set resolution
require('anchor') {
  width = 640,
  height = 360,
}
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

```lua
-- Access engine state
print("Game size: " .. an.width .. "x" .. an.height)
print("Time: " .. an.time .. ", FPS: " .. an.fps)
print("Window: " .. an.window_width .. "x" .. an.window_height .. " @ " .. an.scale .. "x")
```

### Resource Registration

#### an:layer(name)

Creates and registers a layer for rendering.

```lua
game = an:layer('game')
ui = an:layer('ui')
```

Layers are FBO-backed render targets. Drawing commands are queued during update and rendered later. Layers are stored in `an.layers.name`.

---

#### an:image(name, path)

Loads and registers an image (texture).

```lua
an:image('player', 'assets/player.png')
an:image('bullet', 'assets/bullet.png')

-- Access later
layer:image(an.images.player, x, y)
```

Images are stored in `an.images.name` with properties:
- `handle` - internal texture handle
- `width` - texture width in pixels
- `height` - texture height in pixels

---

#### an:font(name, path, size)

Loads and registers a font.

```lua
an:font('main', 'assets/font.ttf', 16)
an:font('title', 'assets/title.ttf', 32)

-- Access later
layer:text("Score: 100", 'main', 10, 10, white())
```

Fonts are stored in `an.fonts.name`.

---

#### an:shader(name, path)

Loads and registers a fragment shader.

```lua
an:shader('blur', 'shaders/blur.frag')
an:shader('outline', 'shaders/outline.frag')

-- Access later
layer:apply_shader(an.shaders.blur)
```

Shaders are stored in `an.shaders.name`.

---

#### an:sound(name, path)

Loads and registers a sound effect.

```lua
an:sound('jump', 'assets/jump.ogg')
an:sound('hit', 'assets/hit.ogg')
```

Sounds are stored in `an.sounds.name`.

---

#### an:music(name, path)

Loads and registers a music track.

```lua
an:music('bgm', 'assets/music.ogg')
an:music('boss', 'assets/boss_theme.ogg')
```

Music tracks are stored in `an.tracks.name`.

---

### Audio Playback

#### an:sound_play(name, volume?, pitch?)

Plays a sound effect.

```lua
an:sound_play('jump')
an:sound_play('hit', 0.5)        -- half volume
an:sound_play('hit', 1, 1.5)     -- normal volume, higher pitch
```

**Parameters:**
- `name` - registered sound name
- `volume` - 0.0 to 1.0 (default 1.0)
- `pitch` - pitch multiplier (default 1.0)

---

#### an:music_play(name, loop?, channel?)

Plays a music track.

```lua
an:music_play('bgm')              -- loops by default
an:music_play('victory', false)   -- play once
an:music_play('bgm', true, 2)     -- on channel 2
```

**Parameters:**
- `name` - registered music name
- `loop` - whether to loop (default true)
- `channel` - audio channel 1-4 (default 1)

---

#### an:music_stop(channel?)

Stops music playback.

```lua
an:music_stop()       -- stop channel 1
an:music_stop(2)      -- stop channel 2
```

---

#### an:music_crossfade(name, duration, channel?)

Crossfades from current music to a new track.

```lua
an:music_crossfade('boss_theme', 2)    -- 2 second crossfade
```

**Parameters:**
- `name` - target track name
- `duration` - crossfade time in seconds
- `channel` - audio channel (default 1)

---

#### Playlist System

```lua
-- Setup
an:playlist_set({'track1', 'track2', 'track3'})
an:playlist_shuffle(true)
an:playlist_set_crossfade(2)

-- Playback
an:playlist_play()
an:playlist_stop()
an:playlist_next()
an:playlist_prev()

-- Query
current = an:playlist_current_track()
```

---

### Physics Setup

#### an:physics_init()

Initializes the physics world. Must be called before any physics operations.

```lua
an:physics_init()
an:physics_set_gravity(0, 500)
an:physics_set_meter_scale(64)
```

---

#### an:physics_set_gravity(gx, gy)

Sets world gravity in pixels per second squared.

```lua
an:physics_set_gravity(0, 500)    -- normal downward gravity
an:physics_set_gravity(0, 0)      -- space
```

---

#### an:physics_set_meter_scale(scale)

Sets pixels per meter for physics simulation. Box2D works in meters internally.

```lua
an:physics_set_meter_scale(64)    -- 64 pixels = 1 meter
```

---

#### an:physics_tag(name)

Registers a collision tag. Tags must be registered before use.

```lua
an:physics_tag('player')
an:physics_tag('enemy')
an:physics_tag('wall')
an:physics_tag('bullet')
```

---

#### an:physics_collision(tag_a, tag_b)

Enables physical collision between two tags. Bodies will collide and generate collision events.

```lua
an:physics_collision('player', 'wall')
an:physics_collision('player', 'enemy')
an:physics_collision('bullet', 'enemy')
```

---

#### an:physics_sensor(tag_a, tag_b)

Enables sensor overlap detection. Bodies pass through each other but generate sensor events.

```lua
an:physics_sensor('player', 'coin')
an:physics_sensor('player', 'danger_zone')
```

---

#### an:physics_hit(tag_a, tag_b)

Enables hit events for physical collisions. Hit events include approach speed for impact-based effects.

```lua
an:physics_hit('player', 'wall')
an:physics_hit('enemy', 'bullet')
```

---

### Physics Events

Events are queried each frame to handle physics interactions. Each event contains the parent objects (not colliders).

#### an:collision_begin_events(tag_a, tag_b)

Returns collisions that started this frame.

```lua
for _, event in ipairs(an:collision_begin_events('player', 'enemy')) do
  event.a:take_damage()  -- player
  event.b:knockback()    -- enemy
  -- Also available: point_x, point_y, normal_x, normal_y
end
```

---

#### an:collision_end_events(tag_a, tag_b)

Returns collisions that ended this frame.

```lua
for _, event in ipairs(an:collision_end_events('player', 'platform')) do
  event.a.on_ground = false
end
```

---

#### an:sensor_begin_events(tag_a, tag_b)

Returns sensor overlaps that started this frame.

```lua
for _, event in ipairs(an:sensor_begin_events('player', 'coin')) do
  event.b:collect()
  event.a.score = event.a.score + 10
end
```

---

#### an:sensor_end_events(tag_a, tag_b)

Returns sensor overlaps that ended this frame.

```lua
for _, event in ipairs(an:sensor_end_events('player', 'water')) do
  event.a:exit_water()
end
```

---

#### an:hit_events(tag_a, tag_b)

Returns hit events with approach speed (requires physics_hit enabled).

```lua
for _, event in ipairs(an:hit_events('player', 'wall')) do
  if event.approach_speed > 300 then
    event.a:take_fall_damage()
    spawn_impact(event.point_x, event.point_y)
  end
end
```

---

### Spatial Queries

Query the physics world for objects at positions or shapes. Returns arrays of parent objects.

#### an:query_point(x, y, tags)

```lua
for _, obj in ipairs(an:query_point(mouse_x, mouse_y, 'button')) do
  obj:hover()
end
```

---

#### an:query_circle(x, y, radius, tags)

```lua
for _, enemy in ipairs(an:query_circle(explosion_x, explosion_y, 100, 'enemy')) do
  enemy:take_damage(50)
end
```

---

#### an:query_aabb(x, y, w, h, tags)

Axis-aligned bounding box query.

```lua
for _, obj in ipairs(an:query_aabb(camera_x, camera_y, camera_w, camera_h, 'drawable')) do
  obj.visible = true
end
```

---

#### an:query_box(x, y, w, h, angle, tags)

Rotated box query.

```lua
for _, enemy in ipairs(an:query_box(x, y, 50, 100, player_angle, 'enemy')) do
  enemy:hit_by_sword()
end
```

---

#### an:query_capsule(x1, y1, x2, y2, radius, tags)

Capsule shape query (two endpoints + radius).

```lua
for _, obj in ipairs(an:query_capsule(start_x, start_y, end_x, end_y, 10, 'enemy')) do
  obj:in_laser_path()
end
```

---

#### an:query_polygon(x, y, vertices, tags)

Polygon shape query. Vertices are a flat array: `{x1, y1, x2, y2, ...}`

```lua
verts = {0, 0, 100, 0, 50, 100}
for _, enemy in ipairs(an:query_polygon(x, y, verts, 'enemy')) do
  enemy:take_damage()
end
```

---

#### an:raycast(x1, y1, x2, y2, tags)

Casts a ray and returns the first hit, or nil.

```lua
hit = an:raycast(player_x, player_y, target_x, target_y, 'wall')
if hit then
  draw_laser(player_x, player_y, hit.point_x, hit.point_y)
  hit.object:take_damage()
end
```

**Returns:** `{object, shape, point_x, point_y, normal_x, normal_y, fraction}` or nil

---

#### an:raycast_all(x1, y1, x2, y2, tags)

Casts a ray and returns all hits.

```lua
for _, hit in ipairs(an:raycast_all(x1, y1, x2, y2, 'enemy')) do
  hit.object:pierce_damage(10)
end
```

---

### Input Binding

The action system maps physical inputs to named actions.

#### an:bind(action, control)

Binds a control to an action.

```lua
an:bind('jump', 'space')
an:bind('jump', 'gamepad_a')
an:bind('left', 'a')
an:bind('left', 'left')
an:bind('fire', 'mouse_1')
```

**Control names:**
- Keyboard: 'a'-'z', '0'-'9', 'space', 'escape', 'return', 'left', 'right', 'up', 'down', etc.
- Mouse: 'mouse_1', 'mouse_2', 'mouse_3', 'mouse_wheel_up', 'mouse_wheel_down'
- Gamepad: 'gamepad_a', 'gamepad_b', 'gamepad_x', 'gamepad_y', 'gamepad_lb', 'gamepad_rb', etc.

---

#### an:unbind(action, control?)

Removes a binding.

```lua
an:unbind('jump', 'space')   -- remove specific binding
an:unbind('jump')            -- remove all bindings for action
```

---

#### an:bind_chord(name, actions)

Creates a chord (simultaneous key press).

```lua
an:bind_chord('dash', {'shift', 'space'})
```

---

#### an:bind_sequence(name, sequence)

Creates a sequence (timed key presses).

```lua
an:bind_sequence('hadouken', {'down', 0.1, 'down_forward', 0.1, 'forward', 0.1, 'punch'})
```

---

#### an:bind_hold(name, duration, source_action)

Creates a hold action (press and hold).

```lua
an:bind_hold('charge_attack', 0.5, 'attack')
```

---

#### Action Queries

```lua
if an:is_pressed('jump') then    -- true on the frame pressed
  player:jump()
end

if an:is_down('fire') then       -- true while held
  player:shoot()
end

if an:is_released('crouch') then -- true on the frame released
  player:stand_up()
end

if an:any_pressed() then         -- any action pressed
  start_game()
end

action = an:get_pressed_action()  -- which action was pressed
```

---

#### an:get_axis(negative, positive)

Returns -1 to 1 based on two opposing actions.

```lua
horizontal = an:get_axis('left', 'right')
player.vx = horizontal*speed
```

---

#### an:get_vector(left, right, up, down)

Returns normalized x, y from four directional actions.

```lua
dx, dy = an:get_vector('left', 'right', 'up', 'down')
player.vx = dx*speed
player.vy = dy*speed
```

---

### Raw Input State

Direct input queries without the action system.

```lua
-- Keyboard
if an:key_is_pressed('escape') then
  pause_game()
end

-- Mouse
if an:mouse_is_down(1) then
  shoot()
end

mx, my = an:mouse_position()
dx, dy = an:mouse_delta()
wx, wy = an:mouse_wheel()
```

---

### Global Actions

Register callbacks that run every frame at different phases.

```lua
-- Runs before object updates (good for physics events)
an:early_action('handle_collisions', function(self)
  for _, event in ipairs(an:collision_begin_events('player', 'enemy')) do
    event.a:hit()
  end
end)

-- Runs during main update phase
an:action('spawn', function(self)
  if an:key_is_pressed('k') then
    an:add(enemy(math.random(0, 480), 0))
  end
end)

-- Runs after object updates (good for rendering)
an:late_action('draw', function(self)
  for _, obj in ipairs(an:all('drawable')) do
    obj:draw(game)
  end
end)
```

---

### Object Management

#### an:add(object)

Adds an object to the tree as a child of `an`.

```lua
player = an:add(player(100, 100))
enemy = an:add(enemy(300, 200))
```

---

#### an:all(tag)

Returns all objects with a tag.

```lua
for _, enemy in ipairs(an:all('enemy')) do
  enemy:update(dt)
end

for _, drawable in ipairs(an:all('drawable')) do
  drawable:draw(layer)
end
```

---

### Engine State

The `an` object exposes engine state:

```lua
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

```lua
enemy = object:extend()

function enemy:new(x, y, type)
  self.x, self.y, self.type = x, y, type
  object.new(self)                      -- call object constructor
  self:tag('enemy', 'drawable')         -- add tags
  self.hp = 100
  self:add(collider('enemy', 'dynamic', 'circle', 16))
  self:add(timer())
end

function enemy:action(dt)
  -- per-frame logic
end

function enemy:draw(layer)
  layer:circle(self.x, self.y, 16, red())
end
```

---

### Lifecycle

#### object:add(child)

Adds a child object. The child's `parent` is set automatically.

```lua
self:add(timer())                    -- named 'timer', access as self.timer
self:add(collider('player', 'dynamic', 'circle', 16))
self:add(spring())

-- Custom name
hitbox = collider('hitbox', 'dynamic', 'box', 32, 16)
hitbox.name = 'hitbox'
self:add(hitbox)                    -- access as self.hitbox
```

---

#### object:all(tag)

Returns children with a tag.

```lua
for _, timer in ipairs(self:all('timer')) do
  timer:cancel_all()
end
```

---

#### object:kill()

Marks the object for removal. Children are killed recursively.

```lua
if self.hp <= 0 then
  self:kill()
end
```

---

### Tags & Types

#### object:tag(tags...)

Adds tags to the object.

```lua
self:tag('enemy')
self:tag('enemy', 'drawable', 'boss')
```

---

#### object:is(tag)

Checks if the object has a tag.

```lua
if obj:is('enemy') then
  obj:take_damage(damage)
end
```

---

### Linking

#### object:link(name, target)

Creates a reference that auto-clears when target dies.

```lua
self:link('target', enemy)

-- In update
if self.target then
  move_toward(self.target.x, self.target.y)
else
  find_new_target()
end
```

---

### Property Setting

#### object:set(properties)

Batch-sets properties.

```lua
self:set({
  x = 100,
  y = 200,
  speed = 50,
  hp = 100,
})
```

---

### Build Pattern

#### object:build()

Returns self for chained construction.

```lua
enemy = an:add(enemy(100, 200):build())
```

Override `build` for post-construction setup that needs the parent:

```lua
bullet = object:extend()

function bullet:build()
  self.collider:set_position(self.parent.x, self.parent.y)
  return self
end
```

---

### State Transitions

#### object:flow_to(state_name)

Transitions to a named state. Calls `enter_<state>` and sets up `<state>_action`.

```lua
player = object:extend()

function player:new()
  object.new(self)
  self:flow_to('idle')
end

function player:enter_idle()
  self.animation = 'idle'
end

function player:idle_action(dt)
  if an:is_pressed('jump') then
    self:flow_to('jumping')
  end
end

function player:enter_jumping()
  self.vy = -200
end

function player:jumping_action(dt)
  if self.on_ground then
    self:flow_to('idle')
  end
end
```

---

### Action Phases

Objects can register callbacks at different update phases:

```lua
-- Before main update (physics events, early logic)
self:early_action(function(self)
  self.on_ground = false
end)

-- Main update (game logic)
self:action(function(self, dt)
  self.x = self.x + self.vx*dt
end)

-- After main update (rendering, late effects)
self:late_action(function(self)
  layer:draw(self.x, self.y)
end)
```

Named actions can be overwritten:

```lua
self:action('movement', function(self, dt)
  self.x = self.x + self.vx*dt
end)

-- Later, replace it
self:action('movement', function(self, dt)
  self.x = self.x + self.vx*dt*2
end)
```

---

## Layer

Layers are FBO-backed render targets for queuing and compositing draw calls.

### Creating Layers

```lua
game = an:layer('game')
ui = an:layer('ui')
ui.camera = nil    -- disable camera for UI layer
```

---

### Drawing Primitives

#### layer:rectangle(x, y, w, h, color)

```lua
layer:rectangle(0, 0, 100, 50, red())
```

---

#### layer:circle(x, y, radius, color)

```lua
layer:circle(50, 50, 25, blue())
```

---

#### layer:rectangle_gradient_h(x, y, w, h, color1, color2)

Draws a filled rectangle with horizontal gradient (left to right).

```lua
layer:rectangle_gradient_h(0, 0, 100, 50, red(), blue())
```

---

#### layer:rectangle_gradient_v(x, y, w, h, color1, color2)

Draws a filled rectangle with vertical gradient (top to bottom).

```lua
-- Sky gradient: light blue at top to darker blue at bottom
bg:rectangle_gradient_v(0, 0, gw, gh, color(135, 206, 235)(), color(25, 25, 112)())
```

---

#### layer:image(image, x, y, color?, flash?)

```lua
layer:image(an.images.player, self.x, self.y)
layer:image(an.images.player, self.x, self.y, white())            -- with tint
layer:image(an.images.player, self.x, self.y, nil, white())       -- flash white
```

**Parameters:**
- `image` - image object from `an.images`
- `x, y` - position (center of image)
- `color` - optional tint color
- `flash` - optional flash color (replaces all pixels)

---

#### layer:spritesheet(spritesheet, frame, x, y, color?, flash?)

Draws a specific frame from a spritesheet.

```lua
layer:spritesheet(an.spritesheets.hit, 1, self.x, self.y)
layer:spritesheet(an.spritesheets.hit, 3, self.x, self.y, white(), red())  -- tint and flash
```

**Parameters:**
- `spritesheet` - spritesheet object from `an.spritesheets`
- `frame` - frame number (1-indexed)
- `x, y` - position (center of frame)
- `color` - optional tint color
- `flash` - optional flash color

---

#### layer:animation(animation, x, y, color?, flash?)

Draws an animation's current frame.

```lua
layer:animation(self.hit1, self.x, self.y)
layer:animation(self.walk, self.x, self.y, nil, self.flashing and white())
```

**Parameters:**
- `animation` - animation object
- `x, y` - position (center of frame)
- `color` - optional tint color
- `flash` - optional flash color

---

#### layer:text(text, font_name, x, y, color)

```lua
layer:text("Score: " .. score, 'main', 10, 10, white())
```

---

### Transform Stack

Push/pop transforms for hierarchical drawing.

```lua
layer:push(self.x, self.y, self.rotation, self.scale_x, self.scale_y)
layer:image(an.images.player, 0, 0)    -- draws at pushed transform
layer:pop()
```

**Parameters:**
- `x, y` - translation (default 0, 0)
- `r` - rotation in radians (default 0)
- `sx, sy` - scale (default 1, 1)

---

### Blend Modes

```lua
layer:set_blend_mode('alpha')       -- default
layer:set_blend_mode('add')         -- additive blending
layer:set_blend_mode('multiply')    -- multiply blending
```

---

### Stencil Masking

Use stencil buffer to mask drawing to specific shapes.

```lua
-- Draw only inside a circular mask
layer:stencil_mask()
layer:circle(150, 150, 50, white())   -- define mask shape (not visible)
layer:stencil_test()
layer:image(an.images.background, 150, 150)  -- only visible inside circle
layer:stencil_off()
```

**stencil_mask()** — Subsequent draws write to stencil only (not visible).

**stencil_test()** — Subsequent draws only appear where stencil was set.

**stencil_off()** — Return to normal drawing.

---

### Shader Effects

Apply shaders as post-processing to the layer.

```lua
-- Apply shader with uniforms
layer:shader_set_float(an.shaders.blur, 'u_radius', 5)
layer:shader_set_vec2(an.shaders.blur, 'u_direction', 1, 0)
layer:apply_shader(an.shaders.blur)

-- Apply another shader
layer:shader_set_vec4(an.shaders.color_shift, 'u_color', 1, 0.5, 0, 1)
layer:apply_shader(an.shaders.color_shift)

-- Reset all effects
layer:reset_effects()
```

---

### Rendering Pipeline

```lua
-- 1. Queue drawing commands during update
game:rectangle(0, 0, 100, 100, red())
game:image(an.images.player, player.x, player.y)

-- 2. Render queued commands to FBO
game:render()

-- 3. Optionally create derived layers
shadow:clear()
shadow:draw_from(game, an.shaders.shadow)

-- 4. Composite to screen
game:draw()
shadow:draw(4, 4)    -- with offset
ui:draw()
```

---

## Spritesheet

Spritesheets are textures divided into a grid of frames for animations.

### Registration

```lua
an:spritesheet('hit', 'assets/hit.png', 96, 48)   -- name, path, frame_width, frame_height
```

Spritesheets are stored in `an.spritesheets.name`.

### Properties

```lua
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

```lua
-- Basic: spritesheet name, delay per frame, loop mode
self:add(animation('hit1', 0.05, 'once'))

-- With per-frame callbacks (frame 0 = completion)
self:add(animation('hit1', 0.05, 'once', {
  [3] = function(self) self:flash() end,        -- called on frame 3
  [0] = function(self) self:kill() end,         -- called when animation completes
}))
```

The animation is accessible as `self.hit1` (using the spritesheet name).

**Loop modes:**
- `'once'` — Plays once, then kills itself
- `'loop'` — Repeats indefinitely (default)
- `'bounce'` — Ping-pongs back and forth

### Updating & Drawing

```lua
function enemy:update(dt)
  self.hit1:update(dt)

  layer:push(self.x, self.y, self.rotation, self.scale, self.scale)
  layer:animation(self.hit1, 0, 0)
  layer:pop()
end
```

### Control Methods

```lua
self.hit1:play()              -- resume playback
self.hit1:stop()              -- pause (keeps position)
self.hit1:reset()             -- restart from frame 1
self.hit1:set_frame(3)        -- jump to specific frame
```

### Properties

```lua
self.hit1.frame       -- current frame number (1-indexed)
self.hit1.playing     -- whether animation is playing
self.hit1.dead        -- true when 'once' animation completes
self.hit1.direction   -- play direction (1 or -1 for bounce)
```

---

## Collider

Physics body wrapper for Box2D integration.

### Creating Colliders

```lua
-- As child object
self:add(collider('player', 'dynamic', 'circle', 16))

-- With options
self:add(collider('sensor', 'static', 'box', 100, 100, {sensor = true}))

-- Multiple shapes
self:add(collider('player', 'dynamic', 'circle', 16))
self.collider:add_box('feet', 10, 5, {offset_y = 16})
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

```lua
-- Position
x, y = self.collider:get_position()
self.collider:set_position(100, 200)
angle = self.collider:get_angle()
self.collider:set_angle(math.pi/4)
self.collider:set_transform(100, 200, 0)

-- Velocity
vx, vy = self.collider:get_velocity()
self.collider:set_velocity(100, 0)
av = self.collider:get_angular_velocity()
self.collider:set_angular_velocity(2)
```

---

### Forces & Impulses

```lua
-- Forces (continuous, affected by mass)
self.collider:apply_force(100, 0)           -- rightward force
self.collider:apply_force_at(0, -100, self.x + 10, self.y)   -- at point

-- Impulses (instant velocity change)
self.collider:apply_impulse(50, -100)       -- jump
self.collider:apply_impulse_at(10, 0, self.x, self.y + 5)

-- Torque
self.collider:apply_torque(10)
self.collider:apply_angular_impulse(5)
```

---

### Body Properties

```lua
self.collider:set_linear_damping(0.5)       -- air resistance
self.collider:set_angular_damping(0.2)      -- rotation damping
self.collider:set_gravity_scale(0.5)        -- half gravity
self.collider:set_gravity_scale(0)          -- no gravity
self.collider:set_fixed_rotation(true)      -- no rotation
self.collider:set_bullet(true)              -- continuous collision detection

mass = self.collider:get_mass()
self.collider:set_center_of_mass(0, 0)      -- override computed center of mass (in pixels)
type = self.collider:get_body_type()        -- 'static', 'dynamic', 'kinematic'
awake = self.collider:is_awake()
self.collider:set_awake(true)
```

---

### Shape Properties

```lua
self.collider:set_friction(0.5)
self.collider:set_restitution(0.8)    -- bounciness
self.collider:set_density(1.0)        -- affects mass
```

---

### Adding Extra Shapes

```lua
self.collider:add_circle('head', 8, {offset_y = -16})
self.collider:add_box('body', 16, 24)
self.collider:add_capsule('arm', 20, 4, {offset_x = 12})

verts = {-16, -16, 16, -16, 16, 16, -16, 16}
self.collider:add_polygon('hitbox', verts)
```

---

## Timer

Scheduling system for delayed and repeating callbacks.

### Creating Timers

```lua
self:add(timer())

-- Access as self.timer
self.timer:after(1, function() print("1 second later") end)
```

---

### One-shot Timers

#### timer:after(delay, name_or_callback, callback?)

```lua
self.timer:after(2, function() self:explode() end)

-- Named (can be cancelled/replaced)
self.timer:after(0.5, 'invincibility', function() self.invincible = false end)
```

---

### Repeating Timers

#### timer:every(interval, name_or_callback, callback?, times?)

```lua
-- Forever
self.timer:every(1, function() spawn_enemy() end)

-- Limited times
self.timer:every(0.5, function() self:shoot() end, 3)    -- shoot 3 times

-- Named
self.timer:every(0.1, 'spawn', function() spawn_particle() end, 10)
```

---

#### timer:every_step(name_or_callback, callback?)

Runs every frame.

```lua
self.timer:every_step(function() table.insert(self.trail, {self.x, self.y}) end)
```

---

### Duration-based

#### timer:during(duration, name_or_callback, callback?, after?)

Called every frame for a duration.

```lua
-- Callback receives (elapsed_time, progress 0-1)
self.timer:during(2, function(self, t, p)
  self.alpha = 1 - p
end, function() self:kill() end)
```

---

#### timer:during_step(name_or_callback, callback?, after?)

Runs every frame until cancelled.

```lua
self.timer:during_step('flicker', function(self)
  self.visible = not self.visible
end)
```

---

### Tweening

#### timer:tween(duration, target, properties, easing?, name?, after?)

Animates properties over time.

```lua
self.timer:tween(0.5, self, {x = 100, y = 200})                    -- linear
self.timer:tween(1, self, {scale = 2}, math.quad_out)            -- with easing
self.timer:tween(1, self, {alpha = 0}, math.sine_in_out, 'fade', function() self:kill() end)
```

---

### Conditional

#### timer:watch(condition, name_or_callback, callback?)

Runs callback once when condition becomes true.

```lua
self.timer:watch(function() return self.hp <= 0 end, function() self:die() end)
```

---

#### timer:when(condition, name_or_callback, callback?)

Runs callback every frame while condition is true.

```lua
self.timer:when(function() return self.on_fire end, function(self)
  self.hp = self.hp - 1
  spawn_flame(self.x, self.y)
end)
```

---

#### timer:cooldown(duration, name)

Returns true once, then false until duration passes.

```lua
if self.timer:cooldown(0.5, 'shoot') then
  self:shoot()
end
```

---

### Timer Control

```lua
self.timer:cancel('spawn')           -- cancel specific timer
self.timer:trigger('spawn')          -- trigger immediately
self.timer:set_multiplier(0.5)       -- slow down all timers
time = self.timer:get_time_left('spawn')
```

---

## Camera

Manages view transformation and coordinate conversion.

### Creating Camera

```lua
an:add(camera())
an.camera:add(shake())    -- optional shake effects
```

---

### Properties

```lua
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

```lua
-- Screen position -> world position
world_x, world_y = an.camera:to_world(screen_x, screen_y)

-- World position -> screen position
screen_x, screen_y = an.camera:to_screen(world_x, world_y)
```

---

### Following

```lua
-- Follow a target with smoothing
an.camera:follow(player, 0.9, 0.1)    -- 90% of distance in 0.1 seconds

-- Set bounds to prevent camera from leaving area
an.camera:set_bounds(0, 0, level_width, level_height)
```

---

### Attaching to Layers

```lua
an.camera:attach(game_layer)    -- apply camera transform
-- draw world objects
an.camera:detach(game_layer)

-- UI layer typically has no camera
ui_layer.camera = nil
```

---

## Spring

Damped spring animation system.

### Creating Springs

```lua
self:add(spring())

-- Access the default 'main' spring
scale = self.spring.main.x

-- Add more springs
self.spring:add('rotation', 0, 3, 0.5)    -- value, frequency, bounce
self.spring:add('x_offset', 0, 8, 0.3)
```

**Parameters:**
- `name` - identifier
- `x` - initial value (default 0)
- `frequency` - oscillations per second (default 5)
- `bounce` - 0 to 1, where 0=no overshoot, 1=infinite oscillation (default 0.5)

---

### Pulling Springs

```lua
-- Add impulse (spring will oscillate back to target)
self.spring:pull('main', 0.3)

-- With new frequency/bounce
self.spring:pull('main', 0.5, 10, 0.7)
```

---

### Changing Target

```lua
-- Spring will animate toward new value
self.spring:set_target('scale', 2)
```

---

### Checking State

```lua
if self.spring:at_rest('main') then
  print("animation complete")
end
```

---

### Using in Draw

```lua
function enemy:draw(layer)
  scale = self.spring.main.x
  layer:push(self.x, self.y, self.spring.rotation.x, scale, scale)
  layer:image(an.images.player, 0, 0)
  layer:pop()
end
```

---

## Shake

Camera shake effects for juice and impact feedback.

### Creating Shake

```lua
an.camera:add(shake())
```

---

### Trauma-based Shake

Perlin noise shake that accumulates and decays.

```lua
an.camera.shake:trauma(0.5, 0.3)    -- amount, duration

-- Configure amplitude
an.camera.shake:set_trauma_parameters({
  x = 20,
  y = 20,
  rotation = 0.1,
  zoom = 0.05,
})
```

---

### Directional Shakes

```lua
-- Spring push (oscillates and settles)
an.camera.shake:push(angle, 20, 5, 0.5)    -- angle, amount, frequency, bounce

-- Random jitter
an.camera.shake:shake(15, 0.5, 60)    -- amplitude, duration, frequency

-- Sine wave
an.camera.shake:sine(angle, 15, 8, 0.5)    -- angle, amplitude, frequency, duration

-- Square wave
an.camera.shake:square(angle, 15, 8, 0.5)
```

---

### Handcam

Continuous subtle motion for immersion.

```lua
an.camera.shake:handcam(true)
an.camera.shake:handcam(true, {x = 3, y = 3, rotation = 0.02}, 0.5)
an.camera.shake:handcam(false)
```

---

## Random

Seeded random number generator for deterministic randomness.

### Creating Random

```lua
self:add(random())               -- uses os.time
self:add(random(12345))          -- deterministic seed

-- Access as self.random
value = self.random:float(0, 100)
```

---

### Number Generation

```lua
self.random:float()              -- 0 to 1
self.random:float(10)            -- 0 to 10
self.random:float(5, 10)         -- 5 to 10

self.random:int(10)              -- 1 to 10
self.random:int(5, 10)           -- 5 to 10

self.random:angle()              -- 0 to 2*pi
self.random:sign()               -- -1 or 1 (50% each)
self.random:sign(75)             -- -1 or 1 (75% for 1)
self.random:bool()               -- true/false (50% each)
self.random:bool(10)             -- true/false (10% for true)

self.random:normal()             -- gaussian, mean=0, stddev=1
self.random:normal(100, 15)      -- mean=100, stddev=15
```

---

### Selection

```lua
enemy = self.random:choice(enemies)
items = self.random:choices(loot_table, 3)

-- Weighted selection (returns index)
weights = {1, 2, 7}  -- 10%, 20%, 70%
index = self.random:weighted(weights)
```

---

### Seed Control

```lua
seed = self.random:get_seed()
self.random:set_seed(12345)
```

---

## Color

Mutable color with RGB and HSL access.

### Creating Colors

```lua
white = color()                    -- 255, 255, 255, 255
red = color(255, 0, 0)             -- opaque red
transparent = color(0, 0, 255, 128)
```

---

### Properties

All properties are read/write. RGB and HSL stay synchronized.

```lua
c = color(255, 0, 0)

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

```lua
layer:circle(x, y, r, red())
layer:rectangle(0, 0, w, h, my_color())
```

---

### Operators

All operators mutate in place and return self for chaining:

```lua
red = red * 0.5           -- darken
red = red / 2             -- darken
red = red + 50            -- brighten all channels
red = red - 30            -- darken all channels

result = color1 * color2  -- component-wise multiply
result = color1 + color2  -- component-wise add
```

---

### Methods

```lua
copy = red:clone()         -- independent copy

red:invert()               -- 255-r, 255-g, 255-b

red:mix(blue, 0.5)         -- 50% toward blue
red:mix(blue, 0)           -- stays red
red:mix(blue, 1)           -- becomes blue
```

---

## Math Extensions

### Interpolation

```lua
-- Linear interpolation
math.lerp(0.5, 0, 100)       -- 50

-- Frame-rate independent lerp
-- Covers p% of distance in t seconds
x = math.lerp_dt(0.9, 1, dt, x, target)   -- 90% in 1 second

-- Angle interpolation (shortest path)
math.lerp_angle(0.5, 0, math.pi)          -- pi/2
math.lerp_angle_dt(0.9, 1, dt, angle, target_angle)
```

---

### Utility

```lua
math.clamp(value, 0, 100)     -- keep in range
math.remap(10, 0, 20, 0, 1)   -- 0.5 (10 is 50% of [0,20], maps to 50% of [0,1])
math.remap(speed, 0, 512, 0, 100)  -- convert speed to percentage
math.loop(angle, 2*math.pi) -- wrap to range
math.length(3, 4)             -- 5 (vector magnitude: sqrt(x*x + y*y))
math.sign(-5)                 -- -1 (returns -1, 0, or 1)
math.sign(0)                  -- 0
math.sign(42)                 -- 1
```

---

### Easing Functions

All take t (0-1) and return transformed t:

```lua
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

```lua
self.timer:tween(1, self, {x = 100}, math.quad_out)
self.timer:tween(0.5, self, {scale = 2}, math.bounce_out)
```

---

## Array Utilities

All functions operate on Lua array tables. Modifying functions change the array in place.

### Predicates

```lua
array.all({1, 2, 3}, function(v) return v > 0 end)         -- true
array.any({1, 2, 3}, function(v) return v > 2 end)         -- true
array.has({1, 2, 3}, 2)                    -- true
array.has({1, 2, 3}, function(v) return v > 2 end)         -- true
```

---

### Aggregation

```lua
array.count({1, 1, 2})                     -- 3
array.count({1, 1, 2}, 1)                  -- 2
array.count({1, 2, 3, 4}, function(v) return v > 2 end)    -- 2

array.sum({1, 2, 3})                       -- 6
array.sum(objects, function(v) return v.score end)         -- sum of scores

array.average({1, 3})                      -- 2

array.max({1, 5, 3})                       -- 5
array.max(objects, function(v) return v.hp end)            -- object with max hp
```

---

### Search

```lua
array.index({2, 1, 2}, 2)                  -- 1 (first occurrence)
array.index({1, 2, 3}, function(v) return v > 2 end)       -- 3

array.get({4, 3, 2, 1}, 1)                 -- 4
array.get({4, 3, 2, 1}, -1)                -- 1 (last)
array.get({4, 3, 2, 1}, 1, 3)              -- {4, 3, 2} (range)
array.get({4, 3, 2, 1}, -2, -1)            -- {2, 1}

array.get_circular_buffer_index({a, b, c}, 4)   -- 1 (wraps)
```

---

### Modification

```lua
array.delete(t, value)                     -- remove all instances
array.remove(t, index)                     -- remove at index
array.reverse(t)                           -- in place
array.reverse(t, 1, 2)                     -- partial
array.rotate(t, 1)                         -- shift right
array.rotate(t, -1)                        -- shift left
array.shuffle(t)                           -- randomize
```

---

### Selection

```lua
array.random(t)                            -- random element
array.random(t, 3)                         -- 3 unique elements

array.remove_random(t)                     -- remove and return random
array.remove_random(t, 3)                  -- remove and return 3
```

---

### Transformation

```lua
array.flatten({1, {2, {3}}})               -- {1, 2, 3}
array.flatten({1, {2, {3}}}, 1)            -- {1, 2, {3}} (1 level)

array.join({1, 2, 3})                      -- '123'
array.join({1, 2, 3}, ', ')                -- '1, 2, 3'
```

---

### Table Utilities

```lua
copy = table.copy(original)                -- deep copy
str = table.tostring(t)                    -- debug string
```
