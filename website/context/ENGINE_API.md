# Engine API Reference

Detailed documentation for all C-to-Lua bindings in the Anchor engine. For a compact signature list, see `ENGINE_API_QUICK.md`.

---

## Layer & Texture

### layer_create

`layer_create(name) -> layer`

Gets or creates a named layer. Layers are FBOs that can be drawn to and composited.

```lua
local game_layer = layer_create("game")
local ui_layer = layer_create("ui")
```

### layer_rectangle

`layer_rectangle(layer, x, y, w, h, color)`

Draws a filled rectangle with top-left corner at (x, y).

```lua
layer_rectangle(layer, 100, 100, 50, 30, rgba(255, 0, 0))  -- top-left at (100, 100)
```

### layer_circle

`layer_circle(layer, x, y, radius, color)`

Draws a filled circle centered at (x, y).

```lua
layer_circle(layer, 200, 150, 25, rgba(0, 255, 0))
```

### layer_push

`layer_push(layer, x?, y?, r?, sx?, sy?)`

Pushes a transform onto the layer's transform stack. All subsequent draws are transformed.

```lua
layer_push(layer, 100, 100, math.pi/4, 2, 2)  -- translate, rotate 45°, scale 2x
layer_rectangle(layer, 0, 0, 20, 20, rgba(255, 255, 255))
layer_pop(layer)
```

### layer_pop

`layer_pop(layer)`

Pops the top transform from the layer's transform stack.

```lua
layer_push(layer, 50, 50)
-- draw stuff transformed...
layer_pop(layer)
```

### layer_draw_texture

`layer_draw_texture(layer, texture, x, y, color?, flash?)`

Draws a texture at the given position. Color tints the texture (default white). Flash overlays a solid color.

```lua
local tex = texture_load("player.png")
layer_draw_texture(layer, tex, player.x, player.y)
layer_draw_texture(layer, tex, x, y, rgba(255, 100, 100), rgba(255, 255, 255, 128))  -- red tint + white flash
```

### layer_set_blend_mode

`layer_set_blend_mode(layer, mode)`

Sets the blend mode for subsequent draws. Modes: "alpha" (default), "additive".

```lua
layer_set_blend_mode(layer, "additive")
layer_circle(layer, x, y, 50, rgba(255, 200, 100))  -- glowing effect
layer_set_blend_mode(layer, "alpha")
```

### layer_draw

`layer_draw(layer, x?, y?)`

Queues the layer to be drawn to screen at the given offset.

```lua
layer_draw(game_layer)
layer_draw(ui_layer, 0, 0)
```

### layer_get_texture

`layer_get_texture(layer) -> texture_id`

Returns the layer's current texture handle (for shader uniforms).

```lua
local tex = layer_get_texture(layer)
layer_shader_set_int(layer, shader, "u_texture", tex)
```

### layer_reset_effects

`layer_reset_effects(layer)`

Clears the layer's contents and resets effect state.

```lua
layer_reset_effects(layer)
```

### layer_render

`layer_render(layer)`

Processes all queued draw commands to the layer's FBO. Clears the FBO first, then renders all commands, then clears the command queue.

```lua
layer_render(game_layer)
```

### layer_clear

`layer_clear(layer)`

Clears the layer's FBO contents to transparent black without processing commands.

```lua
layer_clear(shadow_layer)
```

### layer_draw_from

`layer_draw_from(dst, src, shader?)`

Draws the source layer's texture to the destination layer's FBO. Optionally applies a shader during the draw. Uses alpha blending, so multiple sources accumulate.

```lua
-- Copy game layer to shadow layer through shadow shader
layer_draw_from(shadow_layer, game_layer, shadow_shader)

-- Copy without shader (passthrough)
layer_draw_from(composite_layer, game_layer)
```

### shader_set_float

`shader_set_float(shader, name, value)`

Sets a float uniform on a shader. Use before `layer_draw_from` for immediate effect.

```lua
shader_set_float(blur_shader, "u_radius", 5.0)
```

### shader_set_vec2

`shader_set_vec2(shader, name, x, y)`

Sets a vec2 uniform on a shader.

```lua
shader_set_vec2(outline_shader, "u_pixel_size", 1/480, 1/270)
```

### shader_set_vec4

`shader_set_vec4(shader, name, x, y, z, w)`

Sets a vec4 uniform on a shader.

```lua
shader_set_vec4(tint_shader, "u_color", 1.0, 0.5, 0.0, 1.0)
```

### shader_set_int

`shader_set_int(shader, name, value)`

Sets an int uniform on a shader.

```lua
shader_set_int(effect_shader, "u_mode", 2)
```

### texture_load

`texture_load(path) -> texture`

Loads a texture from file. Supports PNG, JPG, etc.

```lua
local player_tex = texture_load("assets/player.png")
```

### texture_unload

`texture_unload(texture)`

Frees a texture's GPU memory.

```lua
texture_unload(player_tex)
```

### texture_get_width

`texture_get_width(texture) -> int`

Returns the texture's width in pixels.

```lua
local w = texture_get_width(tex)
```

### texture_get_height

`texture_get_height(texture) -> int`

Returns the texture's height in pixels.

```lua
local h = texture_get_height(tex)
```

---

## Font

### font_load

`font_load(name, path, size)`

Loads a TTF font and registers it with a name.

```lua
font_load("main", "assets/font.ttf", 16)
font_load("title", "assets/font.ttf", 32)
```

### font_unload

`font_unload(name)`

Unloads a registered font.

```lua
font_unload("title")
```

### font_get_height

`font_get_height(name) -> number`

Returns the font's line height.

```lua
local line_height = font_get_height("main")
```

### font_get_text_width

`font_get_text_width(name, text) -> number`

Returns the width of a text string in pixels.

```lua
local width = font_get_text_width("main", "Hello World")
```

### font_get_char_width

`font_get_char_width(name, codepoint) -> number`

Returns the advance width of a single character.

```lua
local w = font_get_char_width("main", string.byte("W"))
```

### font_get_glyph_metrics

`font_get_glyph_metrics(name, codepoint) -> {width, height, advance, bearingX, bearingY}`

Returns detailed metrics for a glyph.

```lua
local metrics = font_get_glyph_metrics("main", string.byte("A"))
print(metrics.width, metrics.height, metrics.advance)
```

### layer_draw_text

`layer_draw_text(layer, text, font_name, x, y, color)`

Draws text at the given position.

```lua
layer_draw_text(layer, "Score: 100", "main", 10, 10, rgba(255, 255, 255))
```

### layer_draw_glyph

`layer_draw_glyph(layer, codepoint, font_name, x, y, r?, sx?, sy?, color)`

Draws a single glyph with transform.

```lua
layer_draw_glyph(layer, string.byte("A"), "main", 100, 100, 0, 2, 2, rgba(255, 255, 255))
```

---

## Audio

### sound_load

`sound_load(path) -> sound`

Loads a sound effect (WAV, OGG, etc.).

```lua
local hit_sound = sound_load("assets/hit.wav")
```

### sound_play

`sound_play(sound, volume?, pitch?)`

Plays a sound effect. Volume 0-1, pitch 1.0 = normal.

```lua
sound_play(hit_sound)
sound_play(hit_sound, 0.5, 1.2)  -- half volume, higher pitch
```

### sound_set_volume

`sound_set_volume(volume)`

Sets the master sound effect volume.

```lua
sound_set_volume(0.8)
```

### music_load

`music_load(path) -> music`

Loads a music track.

```lua
local bgm = music_load("assets/music.ogg")
```

### music_play

`music_play(music, loop?)`

Plays a music track. Stops any currently playing music.

```lua
music_play(bgm, true)  -- loop
```

### music_stop

`music_stop()`

Stops the currently playing music.

```lua
music_stop()
```

### music_set_volume

`music_set_volume(volume)`

Sets the music volume (0-1).

```lua
music_set_volume(0.5)
```

### audio_set_master_pitch

`audio_set_master_pitch(pitch)`

Sets the master pitch for all audio. Useful for slow-motion effects.

```lua
audio_set_master_pitch(0.5)  -- slow motion
```

---

## Utility

### rgba

`rgba(r, g, b, a?) -> color`

Packs RGBA values (0-255) into a color integer.

```lua
local red = rgba(255, 0, 0)
local semi_transparent_blue = rgba(0, 0, 255, 128)
```

### set_filter_mode

`set_filter_mode(mode)`

Sets texture filtering mode. "smooth" (bilinear) or "rough" (nearest neighbor).

```lua
set_filter_mode("rough")  -- pixel art
```

### get_filter_mode

`get_filter_mode() -> string`

Returns the current filter mode.

```lua
local mode = get_filter_mode()
```

### timing_resync

`timing_resync()`

Resets timing accumulators. Call after pausing or scene transitions to prevent catch-up updates.

```lua
timing_resync()
```

---

## Effect Shaders

### shader_load_file

`shader_load_file(path) -> shader`

Loads a fragment shader from file.

```lua
local blur_shader = shader_load_file("shaders/blur.glsl")
```

### shader_load_string

`shader_load_string(source) -> shader`

Compiles a fragment shader from a string.

```lua
local invert = shader_load_string([[
    uniform sampler2D u_texture;
    in vec2 v_texcoord;
    out vec4 fragColor;
    void main() {
        vec4 c = texture(u_texture, v_texcoord);
        fragColor = vec4(1.0 - c.rgb, c.a);
    }
]])
```

### shader_destroy

`shader_destroy(shader)`

Frees a shader.

```lua
shader_destroy(blur_shader)
```

### layer_shader_set_float

`layer_shader_set_float(layer, shader, name, value)`

Sets a float uniform (queued, applied when shader runs).

```lua
layer_shader_set_float(layer, blur_shader, "u_radius", 5.0)
```

### layer_shader_set_vec2

`layer_shader_set_vec2(layer, shader, name, x, y)`

Sets a vec2 uniform.

```lua
layer_shader_set_vec2(layer, shader, "u_resolution", 480, 270)
```

### layer_shader_set_vec4

`layer_shader_set_vec4(layer, shader, name, x, y, z, w)`

Sets a vec4 uniform.

```lua
layer_shader_set_vec4(layer, shader, "u_color", 1.0, 0.5, 0.0, 1.0)
```

### layer_shader_set_int

`layer_shader_set_int(layer, shader, name, value)`

Sets an int uniform.

```lua
layer_shader_set_int(layer, shader, "u_iterations", 3)
```

### layer_apply_shader

`layer_apply_shader(layer, shader)`

Applies a shader to the layer's current contents (ping-pong rendering).

```lua
layer_shader_set_float(layer, blur_shader, "u_radius", 4.0)
layer_apply_shader(layer, blur_shader)
```

---

## Physics: World & Bodies

### physics_init

`physics_init()`

Initializes the physics world. Call once at startup.

```lua
physics_init()
```

### physics_set_gravity

`physics_set_gravity(gx, gy)`

Sets world gravity in pixels/sec².

```lua
physics_set_gravity(0, 500)  -- down
```

### physics_set_meter_scale

`physics_set_meter_scale(scale)`

Sets the pixels-per-meter scale for physics (default 64).

```lua
physics_set_meter_scale(32)
```

### physics_set_enabled

`physics_set_enabled(enabled)`

Enables or disables physics simulation.

```lua
physics_set_enabled(false)  -- pause physics
```

### physics_register_tag

`physics_register_tag(name)`

Registers a collision tag. Call before creating bodies.

```lua
physics_register_tag("player")
physics_register_tag("enemy")
physics_register_tag("wall")
```

### physics_enable_collision

`physics_enable_collision(tag_a, tag_b)`

Enables physical collision between two tags.

```lua
physics_enable_collision("player", "wall")
physics_enable_collision("enemy", "wall")
```

### physics_disable_collision

`physics_disable_collision(tag_a, tag_b)`

Disables physical collision between two tags.

```lua
physics_disable_collision("player", "enemy")  -- pass through each other
```

### physics_enable_sensor

`physics_enable_sensor(tag_a, tag_b)`

Enables sensor events (overlap without collision) between two tags.

```lua
physics_enable_sensor("player", "pickup")
```

### physics_enable_hit

`physics_enable_hit(tag_a, tag_b)`

Enables hit events (impact data) between two tags.

```lua
physics_enable_hit("projectile", "enemy")
```

### physics_tags_collide

`physics_tags_collide(tag_a, tag_b) -> bool`

Checks if two tags have collision enabled.

```lua
if physics_tags_collide("player", "wall") then
    -- they physically collide
end
```

### physics_create_body

`physics_create_body(type, x, y) -> body`

Creates a physics body. Types: "static", "dynamic", "kinematic".

```lua
local player_body = physics_create_body("dynamic", 100, 100)
local ground_body = physics_create_body("static", 240, 250)
```

### physics_destroy_body

`physics_destroy_body(body)`

Destroys a physics body and all its shapes.

```lua
physics_destroy_body(enemy_body)
```

### physics_get_position

`physics_get_position(body) -> x, y`

Returns the body's position in pixels.

```lua
local x, y = physics_get_position(player_body)
```

### physics_get_angle

`physics_get_angle(body) -> angle`

Returns the body's rotation in radians.

```lua
local angle = physics_get_angle(player_body)
```

### physics_get_body_count

`physics_get_body_count() -> int`

Returns total number of bodies in the world.

```lua
local count = physics_get_body_count()
```

### physics_body_is_valid

`physics_body_is_valid(body) -> bool`

Checks if a body handle is still valid.

```lua
if physics_body_is_valid(enemy_body) then
    -- still exists
end
```

### physics_add_circle

`physics_add_circle(body, tag, radius, opts?) -> shape`

Adds a circle shape to a body. Opts: {sensor, offset_x, offset_y}.

```lua
local shape = physics_add_circle(player_body, "player", 16)
local sensor = physics_add_circle(player_body, "player_sensor", 32, {sensor = true})
```

### physics_add_box

`physics_add_box(body, tag, width, height, opts?) -> shape`

Adds a box shape. Opts: {sensor, offset_x, offset_y, angle}.

```lua
local shape = physics_add_box(ground_body, "wall", 200, 20)
local rotated = physics_add_box(body, "wall", 50, 10, {angle = math.pi/4})
```

### physics_add_capsule

`physics_add_capsule(body, tag, length, radius, opts?) -> shape`

Adds a capsule shape (vertical).

```lua
local shape = physics_add_capsule(player_body, "player", 24, 8)
```

### physics_add_polygon

`physics_add_polygon(body, tag, vertices, opts?) -> shape`

Adds a convex polygon shape. Vertices: {x1, y1, x2, y2, ...}.

```lua
local shape = physics_add_polygon(body, "wall", {0, 0, 50, 0, 25, 40})
```

---

## Physics: Body Properties

### physics_set_position

`physics_set_position(body, x, y)`

Teleports a body to a position.

```lua
physics_set_position(player_body, spawn_x, spawn_y)
```

### physics_set_angle

`physics_set_angle(body, angle)`

Sets the body's rotation.

```lua
physics_set_angle(player_body, 0)
```

### physics_set_transform

`physics_set_transform(body, x, y, angle)`

Sets position and angle at once.

```lua
physics_set_transform(player_body, 100, 100, 0)
```

### physics_get_velocity

`physics_get_velocity(body) -> vx, vy`

Returns the body's velocity in pixels/sec.

```lua
local vx, vy = physics_get_velocity(player_body)
```

### physics_get_angular_velocity

`physics_get_angular_velocity(body) -> av`

Returns the angular velocity in radians/sec.

```lua
local spin = physics_get_angular_velocity(body)
```

### physics_set_velocity

`physics_set_velocity(body, vx, vy)`

Sets the body's velocity.

```lua
physics_set_velocity(player_body, 0, -300)  -- jump
```

### physics_set_angular_velocity

`physics_set_angular_velocity(body, av)`

Sets angular velocity.

```lua
physics_set_angular_velocity(body, math.pi)
```

### physics_apply_force

`physics_apply_force(body, fx, fy)`

Applies a force at the center of mass (continuous, call every frame).

```lua
physics_apply_force(player_body, move_x * 1000, 0)
```

### physics_apply_force_at

`physics_apply_force_at(body, fx, fy, px, py)`

Applies a force at a world point.

```lua
physics_apply_force_at(body, 0, -500, body_x + 10, body_y)
```

### physics_apply_impulse

`physics_apply_impulse(body, ix, iy)`

Applies an instant impulse at the center of mass.

```lua
physics_apply_impulse(player_body, 0, -200)  -- instant jump
```

### physics_apply_impulse_at

`physics_apply_impulse_at(body, ix, iy, px, py)`

Applies an impulse at a world point.

```lua
physics_apply_impulse_at(body, 100, -50, hit_x, hit_y)
```

### physics_apply_torque

`physics_apply_torque(body, torque)`

Applies rotational force (continuous).

```lua
physics_apply_torque(body, 50)
```

### physics_apply_angular_impulse

`physics_apply_angular_impulse(body, impulse)`

Applies instant rotational impulse.

```lua
physics_apply_angular_impulse(body, 10)
```

### physics_set_linear_damping

`physics_set_linear_damping(body, damping)`

Sets velocity damping (0 = no damping).

```lua
physics_set_linear_damping(player_body, 5)  -- slows down quickly
```

### physics_set_angular_damping

`physics_set_angular_damping(body, damping)`

Sets angular velocity damping.

```lua
physics_set_angular_damping(body, 2)
```

### physics_set_gravity_scale

`physics_set_gravity_scale(body, scale)`

Scales gravity for this body (1 = normal, 0 = no gravity).

```lua
physics_set_gravity_scale(floating_body, 0.1)
```

### physics_set_fixed_rotation

`physics_set_fixed_rotation(body, fixed)`

Prevents the body from rotating.

```lua
physics_set_fixed_rotation(player_body, true)
```

### physics_set_bullet

`physics_set_bullet(body, bullet)`

Enables continuous collision detection for fast-moving bodies.

```lua
physics_set_bullet(projectile_body, true)
```

### physics_set_user_data

`physics_set_user_data(body, id)`

Associates an integer ID with the body (for game object lookup).

```lua
physics_set_user_data(enemy_body, enemy.id)
```

### physics_get_user_data

`physics_get_user_data(body) -> id`

Retrieves the body's user data.

```lua
local enemy_id = physics_get_user_data(body)
local enemy = objects[enemy_id]
```

---

## Physics: Shape Properties

### physics_shape_set_friction

`physics_shape_set_friction(shape, friction)`

Sets the friction coefficient (0 = ice, 1 = rubber).

```lua
physics_shape_set_friction(ground_shape, 0.8)
```

### physics_shape_get_friction

`physics_shape_get_friction(shape) -> friction`

Returns the shape's friction.

```lua
local f = physics_shape_get_friction(shape)
```

### physics_shape_set_restitution

`physics_shape_set_restitution(shape, restitution)`

Sets bounciness (0 = no bounce, 1 = full bounce).

```lua
physics_shape_set_restitution(ball_shape, 0.9)
```

### physics_shape_get_restitution

`physics_shape_get_restitution(shape) -> restitution`

Returns the shape's restitution.

```lua
local r = physics_shape_get_restitution(shape)
```

### physics_shape_is_valid

`physics_shape_is_valid(shape) -> bool`

Checks if a shape handle is still valid.

```lua
if physics_shape_is_valid(shape) then
    -- still exists
end
```

### physics_shape_get_body

`physics_shape_get_body(shape) -> body`

Returns the body that owns this shape.

```lua
local body = physics_shape_get_body(shape)
```

### physics_shape_set_density

`physics_shape_set_density(shape, density)`

Sets the shape's density (affects body mass).

```lua
physics_shape_set_density(shape, 2.0)
```

### physics_shape_get_density

`physics_shape_get_density(shape) -> density`

Returns the shape's density.

```lua
local d = physics_shape_get_density(shape)
```

---

## Physics: Queries

### physics_get_body_type

`physics_get_body_type(body) -> string`

Returns "static", "dynamic", or "kinematic".

```lua
if physics_get_body_type(body) == "dynamic" then
    -- can be moved by forces
end
```

### physics_get_mass

`physics_get_mass(body) -> mass`

Returns the body's total mass.

```lua
local mass = physics_get_mass(player_body)
```

### physics_is_awake

`physics_is_awake(body) -> bool`

Checks if a body is active (not sleeping).

```lua
if physics_is_awake(body) then
    -- body is moving
end
```

### physics_set_awake

`physics_set_awake(body, awake)`

Wakes up or puts a body to sleep.

```lua
physics_set_awake(body, true)  -- wake it up
```

### physics_debug_events

`physics_debug_events()`

Prints current physics event counts to console (for debugging).

```lua
physics_debug_events()
```

---

## Physics: Events

### physics_get_collision_begin

`physics_get_collision_begin(tag_a, tag_b) -> [{body_a, body_b, shape_a, shape_b, point_x, point_y, normal_x, normal_y}, ...]`

Returns all new collision contacts this frame between the given tags. Includes contact point and normal.

```lua
for _, event in ipairs(physics_get_collision_begin("player", "enemy")) do
    local player_id = physics_get_user_data(event.body_a)
    local enemy_id = physics_get_user_data(event.body_b)
    -- event.point_x, event.point_y: contact point in pixels
    -- event.normal_x, event.normal_y: normal from A to B
end
```

### physics_get_collision_end

`physics_get_collision_end(tag_a, tag_b) -> [{body_a, body_b, shape_a, shape_b}, ...]`

Returns all collision contacts that ended this frame.

```lua
for _, event in ipairs(physics_get_collision_end("player", "ground")) do
    -- player left ground
end
```

### physics_get_hit

`physics_get_hit(tag_a, tag_b) -> [{body_a, body_b, shape_a, shape_b, point_x, point_y, normal_x, normal_y, approach_speed}, ...]`

Returns hit events with impact data. Useful for damage based on impact speed.

```lua
for _, hit in ipairs(physics_get_hit("projectile", "enemy")) do
    if hit.approach_speed > 100 then
        -- deal damage
    end
end
```

### physics_get_sensor_begin

`physics_get_sensor_begin(tag_a, tag_b) -> [{sensor_body, visitor_body, sensor_shape, visitor_shape}, ...]`

Returns all new sensor overlaps this frame.

```lua
for _, event in ipairs(physics_get_sensor_begin("pickup_zone", "player")) do
    -- player entered pickup zone
end
```

### physics_get_sensor_end

`physics_get_sensor_end(tag_a, tag_b) -> [{sensor_body, visitor_body, sensor_shape, visitor_shape}, ...]`

Returns all sensor overlaps that ended this frame.

```lua
for _, event in ipairs(physics_get_sensor_end("danger_zone", "player")) do
    -- player left danger zone
end
```

---

## Physics: Spatial Queries & Raycast

### physics_query_point

`physics_query_point(x, y, tags) -> [body, ...]`

Finds all bodies overlapping a point.

```lua
local bodies = physics_query_point(mouse_x, mouse_y, {"enemy", "item"})
```

### physics_query_circle

`physics_query_circle(x, y, radius, tags) -> [body, ...]`

Finds all bodies overlapping a circle.

```lua
local nearby = physics_query_circle(player.x, player.y, 100, {"enemy"})
for _, body in ipairs(nearby) do
    -- enemies within 100 pixels
end
```

### physics_query_aabb

`physics_query_aabb(x, y, w, h, tags) -> [body, ...]`

Finds all bodies overlapping an axis-aligned box centered at (x, y).

```lua
local in_area = physics_query_aabb(200, 150, 100, 80, {"enemy"})
```

### physics_query_box

`physics_query_box(x, y, w, h, angle, tags) -> [body, ...]`

Finds all bodies overlapping a rotated box.

```lua
local in_cone = physics_query_box(x, y, 50, 200, player.angle, {"enemy"})
```

### physics_query_capsule

`physics_query_capsule(x1, y1, x2, y2, radius, tags) -> [body, ...]`

Finds all bodies overlapping a capsule.

```lua
local in_path = physics_query_capsule(start_x, start_y, end_x, end_y, 10, {"wall"})
```

### physics_query_polygon

`physics_query_polygon(x, y, vertices, tags) -> [body, ...]`

Finds all bodies overlapping a polygon. Vertices are {{x,y}, {x,y}, ...}.

```lua
local verts = {{-20, -20}, {20, -20}, {0, 30}}
local in_triangle = physics_query_polygon(100, 100, verts, {"enemy"})
```

### physics_raycast

`physics_raycast(x1, y1, x2, y2, tags) -> {body, shape, point_x, point_y, normal_x, normal_y, fraction} | nil`

Casts a ray and returns the closest hit matching the tag filter.

```lua
local hit = physics_raycast(player.x, player.y, mouse_x, mouse_y, {"wall", "enemy"})
if hit then
    -- draw line to hit.point_x, hit.point_y
    local enemy_id = physics_get_user_data(hit.body)
end
```

### physics_raycast_all

`physics_raycast_all(x1, y1, x2, y2, tags) -> [{body, shape, point_x, point_y, normal_x, normal_y, fraction}, ...]`

Casts a ray and returns all hits (unsorted).

```lua
local hits = physics_raycast_all(x1, y1, x2, y2, {"enemy"})
for _, hit in ipairs(hits) do
    -- pierce through all enemies
end
```

---

## Random

### random_create

`random_create(seed) -> rng`

Creates a new RNG instance with the given seed.

```lua
local level_rng = random_create(level_seed)
```

### random_seed

`random_seed(seed, rng?)`

Seeds an RNG (or the global RNG if not specified).

```lua
random_seed(12345)
random_seed(67890, level_rng)
```

### random_get_seed

`random_get_seed(rng?) -> seed`

Returns the current seed.

```lua
local seed = random_get_seed()
```

### random_float_01

`random_float_01(rng?) -> number`

Returns a random float in [0, 1].

```lua
local t = random_float_01()
```

### random_float

`random_float(min, max, rng?) -> number`

Returns a random float in [min, max].

```lua
local speed = random_float(100, 200)
```

### random_int

`random_int(min, max, rng?) -> int`

Returns a random integer in [min, max] inclusive.

```lua
local damage = random_int(5, 10)
```

### random_angle

`random_angle(rng?) -> number`

Returns a random angle in [0, 2π].

```lua
local dir = random_angle()
local vx, vy = math.cos(dir) * speed, math.sin(dir) * speed
```

### random_sign

`random_sign(chance?, rng?) -> -1 | 1`

Returns -1 or 1. Chance (0-100) is probability of returning 1 (default 50).

```lua
local dir = random_sign()  -- 50/50
local mostly_positive = random_sign(75)  -- 75% chance of 1
```

### random_bool

`random_bool(chance?, rng?) -> bool`

Returns true or false. Chance (0-100) is probability of true (default 50).

```lua
if random_bool(10) then
    -- 10% chance
end
```

### random_normal

`random_normal(mean?, stddev?, rng?) -> number`

Returns a normally distributed random number.

```lua
local height = random_normal(170, 10)  -- mean 170, stddev 10
```

### random_choice

`random_choice(array, rng?) -> element`

Returns a random element from an array.

```lua
local color = random_choice({"red", "green", "blue"})
```

### random_choices

`random_choices(array, n, rng?) -> [element, ...]`

Returns n unique random elements from an array.

```lua
local items = random_choices(loot_table, 3)
```

### random_weighted

`random_weighted(weights, rng?) -> index`

Returns a 1-based index selected by weights.

```lua
local weights = {70, 20, 10}  -- common, rare, legendary
local rarity = random_weighted(weights)
```

### noise

`noise(x, y?, z?) -> number`

Returns Perlin noise in [-1, 1].

```lua
local n = noise(x * 0.1, y * 0.1)
local height = (n + 1) / 2 * max_height
```

---

## Input: Keyboard

### key_is_down

`key_is_down(key) -> bool`

Returns true if the key is currently held.

```lua
if key_is_down("space") then
    -- charging
end
```

### key_is_pressed

`key_is_pressed(key) -> bool`

Returns true on the frame the key was pressed.

```lua
if key_is_pressed("escape") then
    toggle_pause()
end
```

### key_is_released

`key_is_released(key) -> bool`

Returns true on the frame the key was released.

```lua
if key_is_released("space") then
    -- release charged attack
end
```

---

## Input: Mouse

### mouse_position

`mouse_position() -> x, y`

Returns the mouse position in game coordinates.

```lua
local mx, my = mouse_position()
```

### mouse_delta

`mouse_delta() -> dx, dy`

Returns the mouse movement this frame.

```lua
local dx, dy = mouse_delta()
camera.x = camera.x + dx
```

### mouse_set_visible

`mouse_set_visible(visible)`

Shows or hides the system cursor.

```lua
mouse_set_visible(false)
```

### mouse_set_grabbed

`mouse_set_grabbed(grabbed)`

Locks the cursor to the window (for FPS-style control).

```lua
mouse_set_grabbed(true)
```

### mouse_is_down

`mouse_is_down(button) -> bool`

Returns true if a mouse button is held (1=left, 2=middle, 3=right).

```lua
if mouse_is_down(1) then
    -- firing
end
```

### mouse_is_pressed

`mouse_is_pressed(button) -> bool`

Returns true on the frame a mouse button was pressed.

```lua
if mouse_is_pressed(1) then
    shoot()
end
```

### mouse_is_released

`mouse_is_released(button) -> bool`

Returns true on the frame a mouse button was released.

```lua
if mouse_is_released(1) then
    release_shot()
end
```

### mouse_wheel

`mouse_wheel() -> wx, wy`

Returns mouse wheel movement this frame.

```lua
local _, scroll = mouse_wheel()
zoom = zoom + scroll * 0.1
```

---

## Input: Action Binding

### input_bind

`input_bind(action, control) -> bool`

Binds a control to an action.

```lua
input_bind("jump", "key:space")
input_bind("jump", "pad:a")
input_bind("fire", "mouse:1")
```

### input_bind_chord

`input_bind_chord(name, {action, ...}) -> bool`

Creates a chord (all actions must be held simultaneously).

```lua
input_bind_chord("super_attack", {"attack", "special"})
```

### input_bind_sequence

`input_bind_sequence(name, {action, delay, action, delay, action, ...}) -> bool`

Creates a sequence (actions in order within time windows).

```lua
input_bind_sequence("hadouken", {"down", 0.2, "forward", 0.2, "punch"})
```

### input_bind_hold

`input_bind_hold(name, duration, source_action) -> bool`

Creates a hold action that triggers after holding source_action for duration.

```lua
input_bind_hold("charge_attack", 1.0, "attack")
```

### input_get_hold_duration

`input_get_hold_duration(name) -> number`

Returns how long the source action has been held.

```lua
local held = input_get_hold_duration("charge_attack")
local power = math.min(held / 1.0, 1.0)  -- 0 to 1 over 1 second
```

### input_get_last_type

`input_get_last_type() -> string`

Returns "keyboard", "mouse", or "gamepad" based on last input.

```lua
if input_get_last_type() == "gamepad" then
    show_gamepad_prompts()
end
```

### input_start_capture

`input_start_capture()`

Starts capturing the next input for rebinding.

```lua
input_start_capture()
show_prompt("Press a key...")
```

### input_get_captured

`input_get_captured() -> string | nil`

Returns the captured input string, or nil if still waiting.

```lua
local captured = input_get_captured()
if captured then
    input_unbind_all("jump")
    input_bind("jump", captured)
end
```

### input_stop_capture

`input_stop_capture()`

Stops capture mode without binding.

```lua
input_stop_capture()
```

### input_unbind

`input_unbind(action, control) -> bool`

Removes a specific control from an action.

```lua
input_unbind("jump", "key:space")
```

### input_unbind_all

`input_unbind_all(action)`

Removes all controls from an action.

```lua
input_unbind_all("jump")
```

### input_bind_all

`input_bind_all()`

Binds default controls for all common actions.

```lua
input_bind_all()
```

### input_get_axis

`input_get_axis(negative, positive) -> number`

Returns -1, 0, or 1 based on action states.

```lua
local horizontal = input_get_axis("left", "right")
```

### input_get_vector

`input_get_vector(left, right, up, down) -> x, y`

Returns a 2D direction vector based on action states.

```lua
local vx, vy = input_get_vector("left", "right", "up", "down")
```

### input_set_deadzone

`input_set_deadzone(deadzone)`

Sets the gamepad stick deadzone (0-1).

```lua
input_set_deadzone(0.2)
```

### is_down

`is_down(action) -> bool`

Returns true if an action is held (works with simple actions and chords).

```lua
if is_down("fire") then
    -- shooting
end
```

### is_pressed

`is_pressed(action) -> bool`

Returns true on the frame an action was activated.

```lua
if is_pressed("jump") then
    jump()
end
```

### is_released

`is_released(action) -> bool`

Returns true on the frame an action was deactivated.

```lua
if is_released("charge") then
    release_charge()
end
```

### input_any_pressed

`input_any_pressed() -> bool`

Returns true if any action was pressed this frame.

```lua
if input_any_pressed() then
    start_game()
end
```

### input_get_pressed_action

`input_get_pressed_action() -> string | nil`

Returns the name of the action that was just pressed.

```lua
local action = input_get_pressed_action()
if action then
    print("Pressed: " .. action)
end
```

---

## Input: Gamepad

### gamepad_is_connected

`gamepad_is_connected() -> bool`

Returns true if a gamepad is connected.

```lua
if gamepad_is_connected() then
    show_gamepad_ui()
end
```

### gamepad_get_axis

`gamepad_get_axis(axis) -> number`

Returns a gamepad axis value (-1 to 1). Axes: "leftx", "lefty", "rightx", "righty", "triggerleft", "triggerright".

```lua
local lx = gamepad_get_axis("leftx")
local ly = gamepad_get_axis("lefty")
```
