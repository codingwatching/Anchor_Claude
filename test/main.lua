print("main.lua loaded - Effects Test")

set_shape_filter("rough")

local screen_w, screen_h = 480, 270

-- Background color from twitter_emoji theme (48, 49, 50)
local bg_color = rgba(231, 232, 233, 255)

-- Create layers (order matters: first = bottom, last = top)
local bg_layer = layer_create('background')
local shadow_layer = layer_create('shadow')
local outline_layer = layer_create('outline')
local game_layer = layer_create('game')

-- Load shaders
local shadow_shader = shader_load_file('shaders/shadow.frag')
local outline_shader = shader_load_file('shaders/outline.frag')
print("Shadow shader loaded: " .. tostring(shadow_shader))
print("Outline shader loaded: " .. tostring(outline_shader))

-- Load textures
local smile_tex = texture_load("assets/slight_smile.png")
local star_tex = texture_load("assets/star.png")

-- Target display sizes
local smile_size = 36
local star_size = 14

-- Calculate scale factors (textures are 512x512)
local smile_scale = smile_size / texture_get_width(smile_tex)
local star_scale = star_size / texture_get_width(star_tex)

-- HSV to RGB (h: 0-360, s: 0-1, v: 0-1)
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

-- DVD-style bouncing ball
local ball = {
    x = screen_w / 4,
    y = screen_h / 4,
    vx = 100,
    vy = 80,
    radius = 20,
    hue = 0,
    hue_speed = 60,
}

-- Emoji with orbiting stars
local emoji = {
    x = screen_w * 3 / 4,
    y = screen_h * 3 / 4,
    vx = 80,
    vy = 60,
    rotation = 0,
    rotation_speed = 1.5,
}

-- Stars orbiting the smile
local num_stars = 5
local orbit_radius = 35
local orbit_speed = 2.0
local star_spin_speed = 3.0

local game_time = 0

-- Helper to draw objects only (no background) - for outline layer
local function draw_objects(layer)
    -- Draw ball
    local r, g, b = hsv_to_rgb(ball.hue, 1, 1)
    layer_circle(layer, ball.x, ball.y, ball.radius, rgba(r, g, b, 255))

    -- Draw emoji
    layer_push(layer, emoji.x, emoji.y, emoji.rotation, smile_scale, smile_scale)
        layer_draw_texture(layer, smile_tex, 0, 0)
    layer_pop(layer)

    -- Draw orbiting stars
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

    -- === Update ball ===
    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt
    ball.hue = (ball.hue + ball.hue_speed * dt) % 360

    -- Bounce off walls
    if ball.x - ball.radius < 0 then
        ball.x = ball.radius
        ball.vx = -ball.vx
    elseif ball.x + ball.radius > screen_w then
        ball.x = screen_w - ball.radius
        ball.vx = -ball.vx
    end
    if ball.y - ball.radius < 0 then
        ball.y = ball.radius
        ball.vy = -ball.vy
    elseif ball.y + ball.radius > screen_h then
        ball.y = screen_h - ball.radius
        ball.vy = -ball.vy
    end

    -- === Update emoji ===
    emoji.x = emoji.x + emoji.vx * dt
    emoji.y = emoji.y + emoji.vy * dt
    emoji.rotation = emoji.rotation + emoji.rotation_speed * dt

    -- Bounce off walls
    local half_w = smile_size / 2 + orbit_radius + star_size / 2
    local half_h = smile_size / 2 + orbit_radius + star_size / 2
    if emoji.x - half_w < 0 then
        emoji.x = half_w
        emoji.vx = -emoji.vx
    elseif emoji.x + half_w > screen_w then
        emoji.x = screen_w - half_w
        emoji.vx = -emoji.vx
    end
    if emoji.y - half_h < 0 then
        emoji.y = half_h
        emoji.vy = -emoji.vy
    elseif emoji.y + half_h > screen_h then
        emoji.y = screen_h - half_h
        emoji.vy = -emoji.vy
    end

    -- === Draw to layers ===
    -- Background layer: just the background color
    layer_rectangle(bg_layer, 0, 0, screen_w, screen_h, bg_color)

    -- Shadow layer: objects (no offset here - offset applied during compositing)
    draw_objects(shadow_layer)

    -- Outline layer: objects only (no background) so outline shader can detect edges
    draw_objects(outline_layer)

    -- Game layer: objects only (transparent, drawn on top)
    draw_objects(game_layer)

    -- === Apply shaders ===
    layer_apply_shader(shadow_layer, shadow_shader)

    layer_shader_set_vec2(outline_layer, outline_shader, 'u_pixel_size', 1/screen_w, 1/screen_h)
    layer_apply_shader(outline_layer, outline_shader)

    -- === Composite layers manually (with shadow offset) ===
    layer_draw(bg_layer)
    layer_draw(shadow_layer, 4, 4)
    layer_draw(outline_layer)
    layer_draw(game_layer)
end
