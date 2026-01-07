require 'anchor'

function init()
  an:anchor_start('.', 480, 270, 3, 3, 'bytepath')

  an:font('JPN12', 'assets/Mx437_DOS-V_re_JPN12.ttf', 12)
  an:font('lana_pixel', 'assets/LanaPixel.ttf', 11)
  an:font('fat_pixel', 'assets/FatPixelFont.ttf', 8)

  game = object():layer()

  function an:draw_layers()
    game:layer_draw_commands()

    self:layer_draw_to_canvas('main', function()
      game:layer_draw()
    end)

    self:layer_draw('main', 0, 0, 0, self.sx, self.sy)
  end

  -- layer:circle(x, y, rs, color, line_width, z)
  -- layer:rectangle(x, y, w, h, rx, ry, color, line_width, z)

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
    return r + m, g + m, b + m
  end

  --{{{ Circle size comparison test
  --[[
  local top_circles = {}
  local rows = {{}, {}}
  local radius = 1
  local spacing = 2
  local x = 0
  local row = 0
  local half_h = screen_h / 2

  while row < 2 do
    local cx = x + radius
    if cx + radius > screen_w then
      row = row + 1
      x = 0
      cx = x + radius
    end

    if row < 2 then
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

  -- Center each row
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

  an:action(function()
    -- Draw white rectangle for top half
    game:rectangle(screen_w/2, half_h/2, screen_w, half_h, 0, 0, {r=1, g=1, b=1, a=1})
    -- Draw black rectangle for bottom half
    game:rectangle(screen_w/2, half_h + half_h/2, screen_w, half_h, 0, 0, {r=0, g=0, b=0, a=1})

    -- Draw top half circles (white bg)
    for i, c in ipairs(top_circles) do
      local hue = ((i - 1) / total_top) * 360
      local cr, cg, cb = hsv_to_rgb(hue, 1, 1)
      game:circle(c.x, c.y, c.r, {r=cr, g=cg, b=cb, a=1})
    end

    -- Draw bottom half circles (black bg)
    for i, c in ipairs(top_circles) do
      local hue = ((i - 1) / total_top) * 360
      local cr, cg, cb = hsv_to_rgb(hue, 1, 1)
      game:circle(c.x, c.y + half_h, c.r, {r=cr, g=cg, b=cb, a=1})
    end
  end)
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

  an:action(function(self, dt)
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
    local cr, cg, cb = hsv_to_rgb(ball.hue, 1, 1)
    game:circle(ball.x, ball.y, ball.radius, {r=cr, g=cg, b=cb, a=1})
  end)
  --}}}
end
