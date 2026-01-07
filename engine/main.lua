print("main.lua loaded")

-- Set to "rough" for hard pixel edges, "smooth" for anti-aliased
set_shape_filter("rough")

local game = layer_create('game')
local screen_w, screen_h = 480, 270

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

--{{{ Circle size comparison test
--[[
local top_circles = {}
local rows = {{}, {}}  -- track circles per row for centering
local radius = 1
local spacing = 2  -- gap between circles
local x = 0
local row = 0
local half_h = screen_h / 2

-- Build top 2 rows
while row < 2 do
    -- Check if circle fits in current row
    local cx = x + radius
    if cx + radius > screen_w then
        -- Move to next row
        row = row + 1
        x = 0
        cx = x + radius
    end

    if row < 2 then
        -- Calculate row_y after determining final row
        local row_y
        if row == 0 then row_y = half_h / 4
        else row_y = half_h / 4 * 3
        end

        local circle = {x = cx, y = row_y, r = radius, row = row}
        table.insert(top_circles, circle)
        table.insert(rows[row + 1], circle)
        x = cx + radius + spacing
        radius = radius + 1
    end
end

-- Center each row by offsetting all circles
for _, row_circles in ipairs(rows) do
    if #row_circles > 0 then
        local last = row_circles[#row_circles]
        local rightmost = last.x + last.r
        local offset = (screen_w - rightmost) / 2
        for _, c in ipairs(row_circles) do
            c.x = c.x + offset
        end
    end
end

local total_top = #top_circles
print("Total circles per half: " .. total_top)

function update(dt)
    -- Draw black rectangle for bottom half
    layer_rectangle(game, 0, half_h, screen_w, half_h, 0x000000FF)

    -- Draw top half circles (white bg)
    for i, c in ipairs(top_circles) do
        local hue = ((i - 1) / total_top) * 360
        local r, g, b = hsv_to_rgb(hue, 1, 1)
        layer_circle(game, c.x, c.y, c.r, rgba(r, g, b, 255))
    end

    -- Draw bottom half circles (black bg) - same pattern, offset by half_h
    for i, c in ipairs(top_circles) do
        local hue = ((i - 1) / total_top) * 360
        local r, g, b = hsv_to_rgb(hue, 1, 1)
        layer_circle(game, c.x, c.y + half_h, c.r, rgba(r, g, b, 255))
    end
end
--]]
--}}}

--{{{ Bouncing DVD circle test
--[[
local ball = {
    x = screen_w / 2,
    y = screen_h / 2,
    vx = 100,
    vy = 80,
    radius = 20,
    min_radius = 1,
    max_radius = 40,
    radius_speed = 15,
    radius_dir = 1,
    hue = 0,
    hue_speed = 60,
}

function update(dt)
    -- Update position
    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt

    -- Update radius
    ball.radius = ball.radius + ball.radius_speed * ball.radius_dir * dt
    if ball.radius >= ball.max_radius then
        ball.radius = ball.max_radius
        ball.radius_dir = -1
    elseif ball.radius <= ball.min_radius then
        ball.radius = ball.min_radius
        ball.radius_dir = 1
    end

    -- Update color
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

    -- Draw
    local r, g, b = hsv_to_rgb(ball.hue, 1, 1)
    layer_circle(game, ball.x, ball.y, ball.radius, rgba(r, g, b, 255))
end
--]]
--}}}

--{{{ Transform stack test
--[[
local game_time = 0

function update(dt)
    game_time = game_time + dt

    -- Test 1: Simple rotation at screen center
    -- Red rectangle rotating at center
    layer_push(game, 240, 135, game_time, 1, 1)
        layer_rectangle(game, -40, -20, 80, 40, rgba(255, 80, 80, 255))
    layer_pop(game)

    -- Test 2: Circle orbiting around center
    -- Green circle orbiting at distance 80
    layer_push(game, 240, 135, game_time * 2, 1, 1)
        layer_push(game, 80, 0, 0, 1, 1)
            layer_circle(game, 0, 0, 15, rgba(80, 255, 80, 255))
        layer_pop(game)
    layer_pop(game)

    -- Test 3: Scaled and rotated rectangle
    -- Blue rectangle at top-left, scaled 1.5x, rotating slowly
    layer_push(game, 80, 60, game_time * 0.5, 1.5, 1.5)
        layer_rectangle(game, -20, -15, 40, 30, rgba(80, 80, 255, 255))
    layer_pop(game)

    -- Test 4: Nested transforms - rectangle with orbiting circle
    -- Yellow rectangle at bottom-right with cyan circle orbiting it
    layer_push(game, 400, 200, -game_time * 0.3, 1, 1)
        layer_rectangle(game, -25, -25, 50, 50, rgba(255, 255, 80, 255))

        -- Circle orbiting the rectangle
        layer_push(game, 0, 0, game_time * 3, 1, 1)
            layer_push(game, 50, 0, 0, 1, 1)
                layer_circle(game, 0, 0, 10, rgba(80, 255, 255, 255))
            layer_pop(game)
        layer_pop(game)
    layer_pop(game)

    -- Test 5: Non-uniform scale (rectangle becomes stretched)
    -- White rectangle stretched horizontally, rotating
    layer_push(game, 400, 60, game_time * 0.7, 2, 0.5)
        layer_rectangle(game, -20, -20, 40, 40, rgba(255, 255, 255, 200))
    layer_pop(game)

    -- Test 6: Rainbow circles with individual rotations
    -- Row of circles that each rotate independently
    for i = 0, 5 do
        local x = 40 + i * 50
        local hue = (i / 6) * 360 + game_time * 30
        local r, g, b = hsv_to_rgb(hue % 360, 1, 1)
        layer_push(game, x, 230, game_time * (i + 1) * 0.5, 1, 1)
            layer_rectangle(game, -8, -8, 16, 16, rgba(r, g, b, 255))
        layer_pop(game)
    end

    -- Test 7: Static reference shapes (no transform)
    -- Small white dots at corners for reference
    layer_circle(game, 10, 10, 5, rgba(255, 255, 255, 128))
    layer_circle(game, 470, 10, 5, rgba(255, 255, 255, 128))
    layer_circle(game, 10, 260, 5, rgba(255, 255, 255, 128))
    layer_circle(game, 470, 260, 5, rgba(255, 255, 255, 128))

    -- Test 8: Complex nested rotations (top center of screen)
    -- Center rectangle (orange) - rotates in place
    layer_push(game, 240, 60, game_time * 0.5, 1, 1)
        layer_rectangle(game, -20, -12, 40, 24, rgba(255, 150, 50, 255))

        -- Rectangle A (pink) - orbits around center rect AND spins
        layer_push(game, 50, 0, game_time * 2, 1, 1)  -- orbit offset + spin
            layer_rectangle(game, -10, -6, 20, 12, rgba(255, 100, 200, 255))
        layer_pop(game)

        -- Rectangle B (lime) - orbits around orange rect's TOP-RIGHT CORNER
        -- Orange rect is (-20, -12, 40, 24), so top-right corner is at (20, -12)
        layer_circle(game, 20, -12, 2, rgba(255, 255, 255, 255))  -- mark the corner
        layer_push(game, 20, -12, 0, 1, 1)  -- move to corner
            layer_push(game, 0, 0, game_time * 1.5, 1, 1)  -- rotate around that point
                layer_push(game, 25, 0, 0, 1, 1)  -- offset from orbit center
                    layer_rectangle(game, -8, -5, 16, 10, rgba(150, 255, 50, 255))
                layer_pop(game)
            layer_pop(game)
        layer_pop(game)

        -- Rectangle C (purple) - orbits center rect's center, but rotates around its OWN CORNER
        -- Orbit around parent center, then offset to orbit radius, then rotate around corner
        layer_push(game, 0, 0, -game_time * 1.2, 1, 1)  -- orbit rotation
            layer_push(game, 0, 40, 0, 1, 1)  -- offset to orbit radius (below center)
                layer_push(game, 0, 0, game_time * 3, 1, 1)  -- spin around...
                    -- Draw rect with corner at origin (not centered)
                    -- This makes it rotate around its top-left corner
                    layer_rectangle(game, 0, 0, 14, 10, rgba(180, 80, 255, 255))
                layer_pop(game)
            layer_pop(game)
        layer_pop(game)

    layer_pop(game)

    -- Draw a small marker at the test center for reference
    layer_circle(game, 240, 60, 2, rgba(255, 255, 255, 255))
end
--]]
--}}}

--{{{ Bouncing emoji with orbiting stars test
--[[
local smile_tex = texture_load("slight_smile.png")
local star_tex = texture_load("star.png")

-- Target display sizes
local smile_size = 36
local star_size = 14

-- Calculate scale factors (textures are 512x512)
local smile_scale = smile_size / texture_get_width(smile_tex)
local star_scale = star_size / texture_get_width(star_tex)

local ball = {
    x = screen_w / 2,
    y = screen_h / 2,
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

function update(dt)
    game_time = game_time + dt

    -- Update ball position
    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt

    -- Update ball rotation
    ball.rotation = ball.rotation + ball.rotation_speed * dt

    -- Bounce off walls (using display size as bounds)
    local half_w = smile_size / 2 + orbit_radius + star_size / 2
    local half_h = smile_size / 2 + orbit_radius + star_size / 2

    if ball.x - half_w < 0 then
        ball.x = half_w
        ball.vx = -ball.vx
    elseif ball.x + half_w > screen_w then
        ball.x = screen_w - half_w
        ball.vx = -ball.vx
    end

    if ball.y - half_h < 0 then
        ball.y = half_h
        ball.vy = -ball.vy
    elseif ball.y + half_h > screen_h then
        ball.y = screen_h - half_h
        ball.vy = -ball.vy
    end

    -- Draw the smile emoji rotating around its center
    layer_push(game, ball.x, ball.y, ball.rotation, smile_scale, smile_scale)
        layer_draw_texture(game, smile_tex, 0, 0)
    layer_pop(game)

    -- Draw orbiting stars
    for i = 0, num_stars - 1 do
        local angle_offset = (i / num_stars) * math.pi * 2
        local orbit_angle = game_time * orbit_speed + angle_offset
        local star_spin = game_time * star_spin_speed * (i % 2 == 0 and 1 or -1)

        -- Stars orbit the smile and spin around themselves
        layer_push(game, ball.x, ball.y, orbit_angle, 1, 1)
            layer_push(game, orbit_radius, 0, star_spin, star_scale, star_scale)
                layer_draw_texture(game, star_tex, 0, 0)
            layer_pop(game)
        layer_pop(game)
    end
end
--]]
--}}}

--{{{ Combined bouncing circle and emoji test
local smile_tex = texture_load("slight_smile.png")
local star_tex = texture_load("star.png")

-- Target display sizes
local smile_size = 36
local star_size = 14

-- Calculate scale factors (textures are 512x512)
local smile_scale = smile_size / texture_get_width(smile_tex)
local star_scale = star_size / texture_get_width(star_tex)

-- DVD circle (starts top-left)
local circle = {
    x = screen_w / 4,
    y = screen_h / 4,
    vx = 100,
    vy = 80,
    radius = 20,
    min_radius = 1,
    max_radius = 40,
    radius_speed = 15,
    radius_dir = 1,
    hue = 0,
    hue_speed = 60,
}

-- Emoji with stars (starts bottom-right)
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

function update(dt)
    game_time = game_time + dt

    -- === DVD Circle ===
    -- Update position
    circle.x = circle.x + circle.vx * dt
    circle.y = circle.y + circle.vy * dt

    -- Update radius
    circle.radius = circle.radius + circle.radius_speed * circle.radius_dir * dt
    if circle.radius >= circle.max_radius then
        circle.radius = circle.max_radius
        circle.radius_dir = -1
    elseif circle.radius <= circle.min_radius then
        circle.radius = circle.min_radius
        circle.radius_dir = 1
    end

    -- Update color
    circle.hue = (circle.hue + circle.hue_speed * dt) % 360

    -- Bounce off walls
    if circle.x - circle.radius < 0 then
        circle.x = circle.radius
        circle.vx = -circle.vx
    elseif circle.x + circle.radius > screen_w then
        circle.x = screen_w - circle.radius
        circle.vx = -circle.vx
    end

    if circle.y - circle.radius < 0 then
        circle.y = circle.radius
        circle.vy = -circle.vy
    elseif circle.y + circle.radius > screen_h then
        circle.y = screen_h - circle.radius
        circle.vy = -circle.vy
    end

    -- Draw circle
    local r, g, b = hsv_to_rgb(circle.hue, 1, 1)
    layer_circle(game, circle.x, circle.y, circle.radius, rgba(r, g, b, 255))

    -- === Emoji with orbiting stars ===
    -- Update position
    emoji.x = emoji.x + emoji.vx * dt
    emoji.y = emoji.y + emoji.vy * dt

    -- Update rotation
    emoji.rotation = emoji.rotation + emoji.rotation_speed * dt

    -- Bounce off walls (using display size as bounds)
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

    -- Draw the smile emoji rotating around its center
    layer_push(game, emoji.x, emoji.y, emoji.rotation, smile_scale, smile_scale)
        layer_draw_texture(game, smile_tex, 0, 0)
    layer_pop(game)

    -- Draw orbiting stars
    for i = 0, num_stars - 1 do
        local angle_offset = (i / num_stars) * math.pi * 2
        local orbit_angle = game_time * orbit_speed + angle_offset
        local star_spin = game_time * star_spin_speed * (i % 2 == 0 and 1 or -1)

        -- Stars orbit the smile and spin around themselves
        layer_push(game, emoji.x, emoji.y, orbit_angle, 1, 1)
            layer_push(game, orbit_radius, 0, star_spin, star_scale, star_scale)
                layer_draw_texture(game, star_tex, 0, 0)
            layer_pop(game)
        layer_pop(game)
    end
end
--}}}
