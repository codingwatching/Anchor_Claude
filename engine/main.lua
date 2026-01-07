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
--}}}
