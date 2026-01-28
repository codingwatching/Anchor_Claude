# Engine API Quick Reference

Function signatures for all C-to-Lua bindings. See `ENGINE_API.md` for detailed documentation with examples.

## Layer & Texture

```
layer_create(name) -> layer
layer_rectangle(layer, x, y, w, h, color)
layer_rectangle_line(layer, x, y, w, h, color, line_width?)
layer_rectangle_gradient_h(layer, x, y, w, h, color1, color2)
layer_rectangle_gradient_v(layer, x, y, w, h, color1, color2)
layer_circle(layer, x, y, radius, color)
layer_circle_line(layer, x, y, radius, color, line_width?)
layer_line(layer, x1, y1, x2, y2, width, color)
layer_capsule(layer, x1, y1, x2, y2, radius, color)
layer_capsule_line(layer, x1, y1, x2, y2, radius, color, line_width?)
layer_triangle(layer, x1, y1, x2, y2, x3, y3, color)
layer_triangle_line(layer, x1, y1, x2, y2, x3, y3, color, line_width?)
layer_polygon(layer, vertices, color)
layer_polygon_line(layer, vertices, color, line_width?)
layer_rounded_rectangle(layer, x, y, w, h, radius, color)
layer_rounded_rectangle_line(layer, x, y, w, h, radius, color, line_width?)
layer_push(layer, x?, y?, r?, sx?, sy?)
layer_pop(layer)
layer_draw_texture(layer, texture, x, y, color?, flash?)
layer_set_blend_mode(layer, mode)
layer_draw(layer, x?, y?)
layer_get_texture(layer) -> texture_id
layer_reset_effects(layer)
layer_render(layer)
layer_clear(layer)
layer_draw_from(dst, src, shader?)
shader_set_float_immediate(shader, name, value)
shader_set_vec2_immediate(shader, name, x, y)
shader_set_vec4_immediate(shader, name, x, y, z, w)
shader_set_int_immediate(shader, name, value)
texture_load(path) -> texture
texture_unload(texture)
texture_get_width(texture) -> int
texture_get_height(texture) -> int
```

## Font

```
font_load(name, path, size)
font_unload(name)
font_get_height(name) -> number
font_get_text_width(name, text) -> number
font_get_char_width(name, codepoint) -> number
font_get_glyph_metrics(name, codepoint) -> {width, height, advance, bearingX, bearingY}
layer_draw_text(layer, text, font_name, x, y, color)
layer_draw_glyph(layer, codepoint, font_name, x, y, r?, sx?, sy?, color)
```

## Audio

```
sound_load(path) -> sound
sound_play(sound, volume?, pitch?)
sound_set_volume(volume)
music_load(path) -> music
music_play(music, loop?, channel?)
music_stop(channel?)
music_set_volume(volume, channel?)
music_get_volume(channel) -> number
music_is_playing(channel) -> bool
music_at_end(channel) -> bool
music_get_position(channel) -> number
music_get_duration(channel) -> number
audio_set_master_pitch(pitch)
```

## Utility

```
rgba(r, g, b, a?) -> color
set_filter_mode(mode)
get_filter_mode() -> string
timing_resync()
```

## Effect Shaders

```
shader_load_file(path) -> shader
shader_load_string(source) -> shader
shader_destroy(shader)
layer_shader_set_float(layer, shader, name, value)
layer_shader_set_vec2(layer, shader, name, x, y)
layer_shader_set_vec4(layer, shader, name, x, y, z, w)
layer_shader_set_int(layer, shader, name, value)
layer_apply_shader(layer, shader)
```

## Physics: World & Bodies

```
physics_init()
physics_set_gravity(gx, gy)
physics_set_meter_scale(scale)
physics_set_enabled(enabled)
physics_register_tag(name)
physics_enable_collision(tag_a, tag_b)
physics_disable_collision(tag_a, tag_b)
physics_enable_sensor(tag_a, tag_b)
physics_enable_hit(tag_a, tag_b)
physics_tags_collide(tag_a, tag_b) -> bool
physics_create_body(type, x, y) -> body
physics_destroy_body(body)
physics_get_position(body) -> x, y
physics_get_angle(body) -> angle
physics_get_body_count() -> int
physics_body_is_valid(body) -> bool
physics_add_circle(body, tag, radius, opts?) -> shape
physics_add_box(body, tag, width, height, opts?) -> shape
physics_add_capsule(body, tag, length, radius, opts?) -> shape
physics_add_polygon(body, tag, vertices, opts?) -> shape
```

## Physics: Body Properties

```
physics_set_position(body, x, y)
physics_set_angle(body, angle)
physics_set_transform(body, x, y, angle)
physics_get_velocity(body) -> vx, vy
physics_get_angular_velocity(body) -> av
physics_set_velocity(body, vx, vy)
physics_set_angular_velocity(body, av)
physics_apply_force(body, fx, fy)
physics_apply_force_at(body, fx, fy, px, py)
physics_apply_impulse(body, ix, iy)
physics_apply_impulse_at(body, ix, iy, px, py)
physics_apply_torque(body, torque)
physics_apply_angular_impulse(body, impulse)
physics_set_linear_damping(body, damping)
physics_set_angular_damping(body, damping)
physics_set_gravity_scale(body, scale)
physics_set_fixed_rotation(body, fixed)
physics_set_bullet(body, bullet)
physics_set_user_data(body, id)
physics_get_user_data(body) -> id
```

## Physics: Shape Properties

```
physics_shape_set_friction(shape, friction)
physics_shape_get_friction(shape) -> friction
physics_shape_set_restitution(shape, restitution)
physics_shape_get_restitution(shape) -> restitution
physics_shape_is_valid(shape) -> bool
physics_shape_get_body(shape) -> body
physics_shape_set_density(shape, density)
physics_shape_get_density(shape) -> density
```

## Physics: Queries

```
physics_get_body_type(body) -> string
physics_get_mass(body) -> mass
physics_is_awake(body) -> bool
physics_set_awake(body, awake)
physics_debug_events()
```

## Physics: Events

```
physics_get_collision_begin(tag_a, tag_b) -> [{body_a, body_b, shape_a, shape_b, point_x, point_y, normal_x, normal_y}, ...]
physics_get_collision_end(tag_a, tag_b) -> [{body_a, body_b, shape_a, shape_b}, ...]
physics_get_hit(tag_a, tag_b) -> [{body_a, body_b, shape_a, shape_b, point_x, point_y, normal_x, normal_y, approach_speed}, ...]
physics_get_sensor_begin(tag_a, tag_b) -> [{sensor_body, visitor_body, sensor_shape, visitor_shape}, ...]
physics_get_sensor_end(tag_a, tag_b) -> [{sensor_body, visitor_body, sensor_shape, visitor_shape}, ...]
```

## Physics: Spatial Queries & Raycast

```
physics_query_point(x, y, tags) -> [body, ...]
physics_query_circle(x, y, radius, tags) -> [body, ...]
physics_query_aabb(x, y, w, h, tags) -> [body, ...]
physics_query_box(x, y, w, h, angle, tags) -> [body, ...]
physics_query_capsule(x1, y1, x2, y2, radius, tags) -> [body, ...]
physics_query_polygon(x, y, vertices, tags) -> [body, ...]
physics_raycast(x1, y1, x2, y2, tags) -> {body, shape, point_x, point_y, normal_x, normal_y, fraction} | nil
physics_raycast_all(x1, y1, x2, y2, tags) -> [{body, shape, point_x, point_y, normal_x, normal_y, fraction}, ...]
```

## Random

```
random_create(seed) -> rng
random_seed(seed, rng?)
random_get_seed(rng?) -> seed
random_float_01(rng?) -> number
random_float(min, max, rng?) -> number
random_int(min, max, rng?) -> int
random_angle(rng?) -> number
random_sign(chance?, rng?) -> -1 | 1
random_bool(chance?, rng?) -> bool
random_normal(mean?, stddev?, rng?) -> number
random_choice(array, rng?) -> element
random_choices(array, n, rng?) -> [element, ...]
random_weighted(weights, rng?) -> index
noise(x, y?, z?) -> number
```

## Input: Keyboard

```
key_is_down(key) -> bool
key_is_pressed(key) -> bool
key_is_released(key) -> bool
```

## Input: Mouse

```
mouse_position() -> x, y
mouse_delta() -> dx, dy
mouse_set_visible(visible)
mouse_set_grabbed(grabbed)
mouse_is_down(button) -> bool
mouse_is_pressed(button) -> bool
mouse_is_released(button) -> bool
mouse_wheel() -> wx, wy
```

## Input: Action Binding

```
input_bind(action, control) -> bool
input_bind_chord(name, {action, ...}) -> bool
input_bind_sequence(name, {action, delay, action, delay, action, ...}) -> bool
input_bind_hold(name, duration, source_action) -> bool
input_get_hold_duration(name) -> number
input_get_last_type() -> string
input_start_capture()
input_get_captured() -> string | nil
input_stop_capture()
input_unbind(action, control) -> bool
input_unbind_all(action)
input_bind_all()
input_get_axis(negative, positive) -> number
input_get_vector(left, right, up, down) -> x, y
input_set_deadzone(deadzone)
is_down(action) -> bool
is_pressed(action) -> bool
is_released(action) -> bool
input_any_pressed() -> bool
input_get_pressed_action() -> string | nil
```

## Input: Gamepad

```
gamepad_is_connected() -> bool
gamepad_get_axis(axis) -> number
```

## Engine State

```
engine_get_frame() -> int
engine_get_step() -> int
engine_get_time() -> number
engine_get_dt() -> number
engine_get_width() -> int
engine_get_height() -> int
engine_get_window_size() -> int, int
engine_get_scale() -> int
engine_is_fullscreen() -> bool
engine_get_platform() -> string
engine_get_fps() -> number
engine_get_draw_calls() -> int
```

## Engine Configuration

```
engine_set_game_size(width, height)
engine_set_title(title)
engine_set_scale(scale)
engine_set_vsync(enabled)
engine_set_fullscreen(enabled)
engine_set_resizable(enabled)
engine_init()
```
