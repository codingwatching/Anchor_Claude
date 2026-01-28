# Framework API Quick Reference

Function signatures for YueScript framework classes. See `FRAMEWORK_API.md` for detailed documentation with examples.

## Initialization

```
require('anchor')
  width: 640        -- game resolution width (default: 480)
  height: 360       -- game resolution height (default: 270)
  title: "My Game"  -- window title (default: "Anchor")
  scale: 2          -- initial window scale (default: 3)
  vsync: true       -- vertical sync (default: true)
  fullscreen: false -- start fullscreen (default: false)
  resizable: true   -- window resizable (default: true)
  filter: "rough"   -- "rough" or "smooth" (default: "rough")
```

## Root Object (an)

### Engine State (Static)
```
an.width -> int           -- game resolution width
an.height -> int          -- game resolution height
an.dt -> number           -- fixed delta time (1/120)
an.platform -> string     -- "web" or "windows"
```

### Engine State (Dynamic - updated every frame)
```
an.frame -> int           -- current render frame
an.step -> int            -- current physics step
an.time -> number         -- elapsed game time
an.window_width -> int    -- actual window width
an.window_height -> int   -- actual window height
an.scale -> int           -- current render scale
an.fullscreen -> bool     -- fullscreen state
an.fps -> number          -- current FPS
an.draw_calls -> int      -- draw calls last frame
```

### Resource Registration
```
an\layer(name) -> layer
an\image(name, path) -> image
an\font(name, path, size)
an\shader(name, path) -> shader
an\sound(name, path) -> sound_handle
an\music(name, path) -> music_handle
```

### Audio
```
an\sound_play(name, volume?, pitch?)
an\music_play(name, loop?, channel?)
an\music_stop(channel?)
an\music_crossfade(name, duration, channel?)
an\playlist_set(tracks)
an\playlist_play()
an\playlist_stop()
an\playlist_next()
an\playlist_prev()
an\playlist_shuffle(enabled)
an\playlist_set_crossfade(duration)
an\playlist_current_track() -> string
```

### Physics Setup
```
an\physics_init()
an\physics_set_gravity(gx, gy)
an\physics_set_meter_scale(scale)
an\physics_tag(name)
an\physics_collision(tag_a, tag_b)
an\physics_sensor(tag_a, tag_b)
an\physics_hit(tag_a, tag_b)
```

### Physics Events
```
an\collision_begin_events(tag_a, tag_b) -> [{a, b, point_x, point_y, normal_x, normal_y}, ...]
an\collision_end_events(tag_a, tag_b) -> [{a, b}, ...]
an\sensor_begin_events(tag_a, tag_b) -> [{a, b}, ...]
an\sensor_end_events(tag_a, tag_b) -> [{a, b}, ...]
an\hit_events(tag_a, tag_b) -> [{a, b, point_x, point_y, normal_x, normal_y, approach_speed}, ...]
```

### Spatial Queries
```
an\query_point(x, y, tags) -> [object, ...]
an\query_circle(x, y, radius, tags) -> [object, ...]
an\query_aabb(x, y, w, h, tags) -> [object, ...]
an\query_box(x, y, w, h, angle, tags) -> [object, ...]
an\query_capsule(x1, y1, x2, y2, radius, tags) -> [object, ...]
an\query_polygon(x, y, vertices, tags) -> [object, ...]
an\raycast(x1, y1, x2, y2, tags) -> {object, shape, point_x, point_y, normal_x, normal_y, fraction} | nil
an\raycast_all(x1, y1, x2, y2, tags) -> [{object, ...}, ...]
```

### Input Binding
```
an\bind(action, control)
an\unbind(action, control?)
an\bind_chord(name, actions)
an\bind_sequence(name, sequence)
an\bind_hold(name, duration, source_action)
an\is_pressed(action) -> bool
an\is_down(action) -> bool
an\is_released(action) -> bool
an\any_pressed() -> bool
an\get_pressed_action() -> string | nil
an\get_axis(negative, positive) -> number
an\get_vector(left, right, up, down) -> x, y
```

### Input State
```
an\key_is_down(key) -> bool
an\key_is_pressed(key) -> bool
an\key_is_released(key) -> bool
an\mouse_is_down(button) -> bool
an\mouse_is_pressed(button) -> bool
an\mouse_is_released(button) -> bool
an\mouse_position() -> x, y
an\mouse_delta() -> dx, dy
an\mouse_wheel() -> wx, wy
```

### Actions
```
an\early_action(name?, callback)
an\action(name?, callback)
an\late_action(name?, callback)
```

### Object Management
```
an\add(object) -> object
an\all(tag) -> [object, ...]
```

## Object

### Lifecycle
```
object\add(child) -> child
object\all(tag) -> [child, ...]
object\kill()
```

### Tags & Types
```
object\tag(tags...)
object\is(tag) -> bool
```

### Linking
```
object\link(name, target)
```

### Property Setting
```
object\set(properties)
```

### Build Pattern
```
object\build() -> self
```

### State Transitions
```
object\flow_to(state_name)
```

### Action Phases
```
object\early_action(name?, callback)
object\action(name?, callback)
object\late_action(name?, callback)
```

### Aliases
```
object\T(tags...)           -- tag
object\Y(tag) -> bool       -- is (tYpe check)
object\U(callback)          -- early_action (Update early)
object\E(callback)          -- action (Execute)
object\X(callback)          -- late_action (eXit/late)
object\L(name, target)      -- link
object\A(child) -> child    -- add
object\F(state) -> self     -- flow_to
object\K()                  -- kill
```

## Layer

### Drawing
```
layer\rectangle(x, y, w, h, color)
layer\rectangle_gradient_h(x, y, w, h, color1, color2)
layer\rectangle_gradient_v(x, y, w, h, color1, color2)
layer\circle(x, y, radius, color)
layer\image(image, x, y, color?, flash?)
layer\text(text, font_name, x, y, color)
```

### Transform Stack
```
layer\push(x?, y?, r?, sx?, sy?)
layer\pop()
```

### Blend & Effects
```
layer\set_blend_mode(mode)
layer\apply_shader(shader)
layer\shader_set_float(shader, name, value)
layer\shader_set_vec2(shader, name, x, y)
layer\shader_set_vec4(shader, name, x, y, z, w)
layer\shader_set_int(shader, name, value)
layer\reset_effects()
```

### Rendering
```
layer\render()
layer\clear()
layer\draw(x?, y?)
layer\draw_from(source, shader?)
layer\get_texture() -> texture_id
```

## Image

```
image.width -> number
image.height -> number
image.handle -> texture_handle
```

## Font

```
font.name -> string
font.size -> number
font.height -> number
font\text_width(text) -> number
font\char_width(codepoint) -> number
font\glyph_metrics(codepoint) -> {width, height, advance, bearingX, bearingY}
```

## Collider

### Creation
```
collider(tag, body_type, shape_type, ...) -> collider
-- body_type: 'static', 'dynamic', 'kinematic'
-- shape_type: 'circle', 'box', 'capsule', 'polygon'
```

### Position & Transform
```
collider\get_position() -> x, y
collider\set_position(x, y)
collider\get_angle() -> angle
collider\set_angle(angle)
collider\set_transform(x, y, angle)
```

### Velocity
```
collider\get_velocity() -> vx, vy
collider\set_velocity(vx, vy)
collider\get_angular_velocity() -> av
collider\set_angular_velocity(av)
```

### Forces
```
collider\apply_force(fx, fy)
collider\apply_force_at(fx, fy, px, py)
collider\apply_impulse(ix, iy)
collider\apply_impulse_at(ix, iy, px, py)
collider\apply_torque(torque)
collider\apply_angular_impulse(impulse)
```

### Body Properties
```
collider\set_linear_damping(damping)
collider\set_angular_damping(damping)
collider\set_gravity_scale(scale)
collider\set_fixed_rotation(fixed)
collider\set_bullet(bullet)
collider\get_mass() -> mass
collider\set_center_of_mass(x, y)
collider\get_body_type() -> string
collider\is_awake() -> bool
collider\set_awake(awake)
```

### Shape Properties
```
collider\set_friction(friction)
collider\get_friction() -> friction
collider\set_restitution(restitution)
collider\get_restitution() -> restitution
collider\set_density(density)
collider\get_density() -> density
```

### Adding Shapes
```
collider\add_circle(tag, radius, opts?) -> shape
collider\add_box(tag, width, height, opts?) -> shape
collider\add_capsule(tag, length, radius, opts?) -> shape
collider\add_polygon(tag, vertices, opts?) -> shape
```

## Timer

### One-shot
```
timer\after(delay, name_or_callback, callback?) -> timer
```

### Repeating
```
timer\every(interval, name_or_callback, callback?, times?) -> timer
timer\every_step(name_or_callback, callback?) -> timer
```

### Duration-based
```
timer\during(duration, name_or_callback, callback?, after?) -> timer
timer\during_step(name_or_callback, callback?, after?) -> timer
```

### Tweening
```
timer\tween(duration, target, properties, easing?, name?, after?) -> timer
```

### Conditional
```
timer\watch(condition, name_or_callback, callback?) -> timer
timer\when(condition, name_or_callback, callback?) -> timer
timer\cooldown(duration, name) -> timer
```

### Control
```
timer\cancel(name)
timer\trigger(name)
timer\set_multiplier(multiplier)
timer\get_time_left(name) -> number | nil
```

## Camera

### Properties
```
camera.x -> number
camera.y -> number
camera.w -> number (read-only)
camera.h -> number (read-only)
camera.rotation -> number
camera.zoom -> number
camera.mouse.x -> number (world coordinates)
camera.mouse.y -> number (world coordinates)
```

### Coordinate Conversion
```
camera\to_world(screen_x, screen_y) -> world_x, world_y
camera\to_screen(world_x, world_y) -> screen_x, screen_y
```

### Transform
```
camera\attach(layer)
camera\detach(layer)
camera\get_effects() -> {x, y, rotation, zoom}
```

### Following
```
camera\follow(target, lerp_speed?, lerp_time?)
camera\set_bounds(x, y, w, h)
```

## Spring

### Creation
```
spring\add(name, x?, frequency?, bounce?)
```

### Control
```
spring\pull(name, force, frequency?, bounce?)
spring\set_target(name, value)
spring\at_rest(name, threshold?) -> bool
```

### Access
```
spring.name.x -> number (current value)
spring.name.target_x -> number
spring.name.v -> number (velocity)
```

## Shake

### Trauma
```
shake\trauma(amount, duration?)
shake\set_trauma_parameters(amplitude)
```

### Directional
```
shake\push(angle, amount, frequency?, bounce?)
shake\shake(amplitude, duration, frequency?)
shake\sine(angle, amplitude, frequency, duration)
shake\square(angle, amplitude, frequency, duration)
```

### Continuous
```
shake\handcam(enabled, amplitude?, frequency?)
```

### Effects
```
shake\get_transform() -> {x, y, rotation, zoom}
```

## Random

### Float
```
random\float(max?) -> number           -- 0 to max (or 0 to 1)
random\float(min, max) -> number       -- min to max
```

### Integer
```
random\int(max) -> int                 -- 1 to max
random\int(min, max) -> int            -- min to max
```

### Special
```
random\angle() -> number               -- 0 to 2*pi
random\sign(chance?) -> -1 | 1         -- chance% for 1
random\bool(chance?) -> bool           -- chance% for true
random\normal(mean?, stddev?) -> number
```

### Selection
```
random\choice(array) -> element
random\choices(array, n) -> [element, ...]
random\weighted(weights) -> index
```

### Seed
```
random\get_seed() -> seed
random\set_seed(seed)
```

## Color

### Creation
```
color(r?, g?, b?, a?) -> color
```

### Properties (read/write)
```
color.r, color.g, color.b, color.a -> 0-255
color.h -> 0-360 (hue)
color.s -> 0-1 (saturation)
color.l -> 0-1 (lightness)
```

### Operators (mutate in place)
```
color * number -> self
color * color -> self
color / number -> self
color + number -> self
color + color -> self
color - number -> self
color - color -> self
```

### Methods
```
color() -> packed_rgba        -- for drawing functions
color\clone() -> color
color\invert() -> self
color\mix(other, t?) -> self
```

## Math Extensions

### Interpolation
```
math.lerp(t, source, destination) -> number
math.lerp_dt(p, t, dt, source, destination) -> number
math.lerp_angle(t, source, destination) -> number
math.lerp_angle_dt(p, t, dt, source, destination) -> number
```

### Utility
```
math.clamp(value, min, max) -> number
math.remap(value, old_min, old_max, new_min, new_max) -> number
math.loop(t, length) -> number
math.length(x, y) -> number
math.sign(value) -> -1 | 0 | 1
math.snap(value, grid) -> number
```

### Vector & Angle
```
math.angle(x, y) -> number
math.angle_to_point(x1, y1, x2, y2) -> number
math.distance(x1, y1, x2, y2) -> number
math.normalize(x, y) -> x, y
math.direction(angle) -> x, y
math.rotate(x, y, angle) -> x, y
math.reflect(angle, normal_x, normal_y) -> number
math.limit(x, y, max) -> x, y
```

### Easing Functions
```
math.linear(t) -> number
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

## Array Utilities

### Predicates
```
array.all(t, f) -> bool
array.any(t, f) -> bool
array.has(t, v) -> bool
```

### Aggregation
```
array.count(t, v?) -> int
array.sum(t, f?) -> number
array.average(t) -> number
array.max(t, f?) -> element
```

### Search
```
array.index(t, v) -> int | nil
array.get(t, i, j?) -> element | [element, ...]
array.get_circular_buffer_index(t, i) -> int
```

### Modification (in-place)
```
array.delete(t, v) -> count
array.remove(t, i) -> element
array.reverse(t, i?, j?) -> t
array.rotate(t, n) -> t
array.shuffle(t, rng?) -> t
```

### Selection
```
array.random(t, n?, rng?) -> element | [element, ...]
array.remove_random(t, n?, rng?) -> element | [element, ...]
```

### Transformation
```
array.flatten(t, level?) -> [element, ...]
array.join(t, separator?) -> string
```

### Table Utilities
```
table.copy(t) -> table
table.tostring(t) -> string
```
