print("Input Test")

set_shape_filter("rough")

local screen_w, screen_h = 480, 270

input_bind_all()
input_bind_chord("super_action", {"lshift", "space"})
input_bind_sequence("dash", {"d", 0.3, "d"})
input_bind_hold("charge", 1.0, "space")

input_bind("move_up", "key:w")
input_bind("move_up", "key:up")
input_bind("move_down", "key:s")
input_bind("move_down", "key:down")
input_bind("move_left", "key:a")
input_bind("move_left", "key:left")
input_bind("move_right", "key:d")
input_bind("move_right", "key:right")
input_bind("action", "key:space")
input_bind("action", "mouse:1")

local bg_color = rgba(231, 232, 233, 255)
local bg_layer = layer_create("background")
local shadow_layer = layer_create("shadow")
local outline_layer = layer_create("outline")
local game_layer = layer_create("game")

local shadow_shader = shader_load_file("shaders/shadow.frag")
local outline_shader = shader_load_file("shaders/outline.frag")

local smile_tex = texture_load("assets/slight_smile.png")
local star_tex = texture_load("assets/star.png")

local smile_size = 36
local star_size = 14
local smile_scale = smile_size / texture_get_width(smile_tex)
local star_scale = star_size / texture_get_width(star_tex)

local function hsv_to_rgb(h, s, v)
    local c = v * s
    local x = c * (1 - math.abs((h / 60) % 2 - 1))
    local m = v - c
    local r, g, b = 0, 0, 0
    if h < 60 then r, g, b = c, x, 0
    elseif h < 120 then r, g, b = x, c, 0
    elseif h < 180 then r, g, b = 0, c, x
    elseif h < 240 then r, g, b = 0, x, c
    elseif h < 300 then r, g, b = x, 0, c
    else r, g, b = c, 0, x
    end
    return math.floor((r + m) * 255), math.floor((g + m) * 255), math.floor((b + m) * 255)
end

local ball = { x = screen_w / 4, y = screen_h / 4, radius = 20, hue = 0, hue_speed = 60 }
local emoji = { x = screen_w * 3 / 4, y = screen_h * 3 / 4, vx = 80, vy = 60, rotation = 0, rotation_speed = 1.5 }

local num_stars = 5
local orbit_radius = 35
local orbit_speed = 2.0
local star_spin_speed = 3.0
local game_time = 0
local move_speed = 150
local pressed_flash = 0
local cursor_radius = 8
local last_input_type = "keyboard"
local capture_active = false

local function draw_objects(layer)
    local r, g, b = hsv_to_rgb(ball.hue, 1, 1)
    if pressed_flash > 0 then
        local flash_t = pressed_flash / 0.3
        r = math.floor(r + (255 - r) * flash_t)
        g = math.floor(g + (255 - g) * flash_t)
        b = math.floor(b + (255 - b) * flash_t)
    end
    layer_circle(layer, ball.x, ball.y, ball.radius, rgba(r, g, b, 255))

    local mx, my = mouse_position()
    local cursor_color = rgba(255, 255, 255, 255)
    if mouse_is_down(1) then cursor_color = rgba(100, 255, 100, 255) end
    layer_circle(layer, mx, my, cursor_radius, cursor_color)

    if not key_is_down("shift") then
        layer_push(layer, emoji.x, emoji.y, emoji.rotation, smile_scale, smile_scale)
        layer_draw_texture(layer, smile_tex, 0, 0)
        layer_pop(layer)
    end

    for i = 0, num_stars - 1 do
        local angle_offset = (i / num_stars) * math.pi * 2
        local orbit_angle = game_time * orbit_speed + angle_offset
        local star_spin = game_time * star_spin_speed * (i % 2 == 0 and 1 or -1)
        layer_push(layer, emoji.x, emoji.y, orbit_angle, 1, 1)
        layer_push(layer, orbit_radius, 0, star_spin, star_scale, star_scale)
        layer_draw_texture(layer, star_tex, 0, 0)
        layer_pop(layer)
        layer_pop(layer)
    end
end

function update(dt)
    game_time = game_time + dt

    local move_x, move_y = input_get_vector("move_left", "move_right", "move_up", "move_down")
    ball.x = ball.x + move_x * move_speed * dt
    ball.y = ball.y + move_y * move_speed * dt

    if is_pressed("action") then pressed_flash = 0.3; print("ACTION!") end
    if is_pressed("super_action") then pressed_flash = 0.5; print("SUPER ACTION!") end
    if is_pressed("dash") then pressed_flash = 0.5; print("DASH!") end
    if is_pressed("charge") then pressed_flash = 0.5; print("CHARGE!") end

    if is_pressed("c") and not capture_active then
        input_start_capture(); capture_active = true; print("CAPTURE MODE...")
    end
    if capture_active then
        local captured = input_get_captured()
        if captured then print("CAPTURED: " .. captured); input_stop_capture(); capture_active = false end
    end

    local wheel_x, wheel_y = mouse_wheel()
    if wheel_y ~= 0 then cursor_radius = math.max(4, math.min(30, cursor_radius + wheel_y * 2)) end

    if pressed_flash > 0 then pressed_flash = pressed_flash - dt end
    ball.hue = (ball.hue + ball.hue_speed * dt) % 360

    if ball.x - ball.radius < 0 then ball.x = ball.radius end
    if ball.x + ball.radius > screen_w then ball.x = screen_w - ball.radius end
    if ball.y - ball.radius < 0 then ball.y = ball.radius end
    if ball.y + ball.radius > screen_h then ball.y = screen_h - ball.radius end

    emoji.x = emoji.x + emoji.vx * dt
    emoji.y = emoji.y + emoji.vy * dt
    emoji.rotation = emoji.rotation + emoji.rotation_speed * dt

    local half = smile_size / 2 + orbit_radius + star_size / 2
    if emoji.x - half < 0 then emoji.x = half; emoji.vx = -emoji.vx end
    if emoji.x + half > screen_w then emoji.x = screen_w - half; emoji.vx = -emoji.vx end
    if emoji.y - half < 0 then emoji.y = half; emoji.vy = -emoji.vy end
    if emoji.y + half > screen_h then emoji.y = screen_h - half; emoji.vy = -emoji.vy end

    layer_rectangle(bg_layer, 0, 0, screen_w, screen_h, bg_color)
    draw_objects(shadow_layer)
    draw_objects(outline_layer)
    draw_objects(game_layer)

    layer_apply_shader(shadow_layer, shadow_shader)
    layer_shader_set_vec2(outline_layer, outline_shader, "u_pixel_size", 1/screen_w, 1/screen_h)
    layer_apply_shader(outline_layer, outline_shader)

    layer_draw(bg_layer)
    layer_draw(shadow_layer, 4, 4)
    layer_draw(outline_layer)
    layer_draw(game_layer)
end
