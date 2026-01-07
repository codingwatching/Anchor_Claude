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
  --]]
  --}}}

  --{{{ Transform stack test
  --[[
  local game_time = 0

  an:action(function(self, dt)
    game_time = game_time + dt

    -- Test 1: Simple rotation at screen center
    -- Red rectangle rotating at center
    game:push_trs(240, 135, game_time, 1, 1)
        game:rectangle(0, 0, 80, 40, 0, 0, {r=1, g=0.31, b=0.31, a=1})
    game:pop()

    -- Test 2: Circle orbiting around center
    -- Green circle orbiting at distance 80
    game:push_trs(240, 135, game_time * 2, 1, 1)
        game:push_trs(80, 0, 0, 1, 1)
            game:circle(0, 0, 15, {r=0.31, g=1, b=0.31, a=1})
        game:pop()
    game:pop()

    -- Test 3: Scaled and rotated rectangle
    -- Blue rectangle at top-left, scaled 1.5x, rotating slowly
    game:push_trs(80, 60, game_time * 0.5, 1.5, 1.5)
        game:rectangle(0, 0, 40, 30, 0, 0, {r=0.31, g=0.31, b=1, a=1})
    game:pop()

    -- Test 4: Nested transforms - rectangle with orbiting circle
    -- Yellow rectangle at bottom-right with cyan circle orbiting it
    game:push_trs(400, 200, -game_time * 0.3, 1, 1)
        game:rectangle(0, 0, 50, 50, 0, 0, {r=1, g=1, b=0.31, a=1})

        -- Circle orbiting the rectangle
        game:push_trs(0, 0, game_time * 3, 1, 1)
            game:push_trs(50, 0, 0, 1, 1)
                game:circle(0, 0, 10, {r=0.31, g=1, b=1, a=1})
            game:pop()
        game:pop()
    game:pop()

    -- Test 5: Non-uniform scale (rectangle becomes stretched)
    -- White rectangle stretched horizontally, rotating
    game:push_trs(400, 60, game_time * 0.7, 2, 0.5)
        game:rectangle(0, 0, 40, 40, 0, 0, {r=1, g=1, b=1, a=0.78})
    game:pop()

    -- Test 6: Rainbow circles with individual rotations
    -- Row of circles that each rotate independently
    for i = 0, 5 do
        local x = 40 + i * 50
        local hue = (i / 6) * 360 + game_time * 30
        local cr, cg, cb = hsv_to_rgb(hue % 360, 1, 1)
        game:push_trs(x, 230, game_time * (i + 1) * 0.5, 1, 1)
            game:rectangle(0, 0, 16, 16, 0, 0, {r=cr, g=cg, b=cb, a=1})
        game:pop()
    end

    -- Test 7: Static reference shapes (no transform)
    -- Small white dots at corners for reference
    game:circle(10, 10, 5, {r=1, g=1, b=1, a=0.5})
    game:circle(470, 10, 5, {r=1, g=1, b=1, a=0.5})
    game:circle(10, 260, 5, {r=1, g=1, b=1, a=0.5})
    game:circle(470, 260, 5, {r=1, g=1, b=1, a=0.5})

    -- Test 8: Complex nested rotations (top center of screen)
    -- Center rectangle (orange) - rotates in place
    game:push_trs(240, 60, game_time * 0.5, 1, 1)
        game:rectangle(0, 0, 40, 24, 0, 0, {r=1, g=0.59, b=0.2, a=1})

        -- Rectangle A (pink) - orbits around center rect AND spins
        game:push_trs(50, 0, game_time * 2, 1, 1)
            game:rectangle(0, 0, 20, 12, 0, 0, {r=1, g=0.39, b=0.78, a=1})
        game:pop()

        -- Rectangle B (lime) - orbits around orange rect's TOP-RIGHT CORNER
        -- Orange rect is 40x24 centered, so top-right corner is at (20, -12)
        game:circle(20, -12, 2, {r=1, g=1, b=1, a=1})  -- mark the corner
        game:push_trs(20, -12, 0, 1, 1)  -- move to corner
            game:push_trs(0, 0, game_time * 1.5, 1, 1)  -- rotate around that point
                game:push_trs(25, 0, 0, 1, 1)  -- offset from orbit center
                    game:rectangle(0, 0, 16, 10, 0, 0, {r=0.59, g=1, b=0.2, a=1})
                game:pop()
            game:pop()
        game:pop()

        -- Rectangle C (purple) - orbits center rect's center, but rotates around its OWN CORNER
        -- Orbit around parent center, then offset to orbit radius, then rotate around corner
        game:push_trs(0, 0, -game_time * 1.2, 1, 1)  -- orbit rotation
            game:push_trs(0, 40, 0, 1, 1)  -- offset to orbit radius (below center)
                game:push_trs(0, 0, game_time * 3, 1, 1)  -- spin around...
                    -- Draw rect with corner at origin (not centered)
                    -- rectangle_lt draws from top-left, so (0, 0, 14, 10) has corner at origin
                    game:rectangle_lt(0, 0, 14, 10, 0, 0, {r=0.71, g=0.31, b=1, a=1})
                game:pop()
            game:pop()
        game:pop()

    game:pop()

    -- Draw a small marker at the test center for reference
    game:circle(240, 60, 2, {r=1, g=1, b=1, a=1})
  end)
  --]]
  --}}}

  --{{{ Bouncing emoji with orbiting stars test
  --[[
  an:image('smile', 'assets/slight_smile.png')
  an:image('star', 'assets/star.png')

  -- Target display sizes
  local smile_size = 36
  local star_size = 14

  -- Calculate scale factors (textures are 512x512)
  local smile_scale = smile_size / an.images.smile.w
  local star_scale = star_size / an.images.star.w

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

  an:action(function(self, dt)
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
    game:push_trs(ball.x, ball.y, ball.rotation, smile_scale, smile_scale)
        game:draw_image('smile', 0, 0)
    game:pop()

    -- Draw orbiting stars
    for i = 0, num_stars - 1 do
      local angle_offset = (i / num_stars) * math.pi * 2
      local orbit_angle = game_time * orbit_speed + angle_offset
      local star_spin = game_time * star_spin_speed * (i % 2 == 0 and 1 or -1)

      -- Stars orbit the smile and spin around themselves
      game:push_trs(ball.x, ball.y, orbit_angle, 1, 1)
          game:push_trs(orbit_radius, 0, star_spin, star_scale, star_scale)
              game:draw_image('star', 0, 0)
          game:pop()
      game:pop()
    end
  end)
  --]]
  --}}}

  --{{{ Combined bouncing circle and emoji test
  an:image('smile', 'assets/slight_smile.png')
  an:image('star', 'assets/star.png')

  -- Target display sizes
  local smile_size = 36
  local star_size = 14

  -- Calculate scale factors (textures are 512x512)
  local smile_scale = smile_size / an.images.smile.w
  local star_scale = star_size / an.images.star.w

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

  an:action(function(self, dt)
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
    local cr, cg, cb = hsv_to_rgb(circle.hue, 1, 1)
    game:circle(circle.x, circle.y, circle.radius, {r=cr, g=cg, b=cb, a=1})

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
    game:push_trs(emoji.x, emoji.y, emoji.rotation, smile_scale, smile_scale)
        game:draw_image('smile', 0, 0)
    game:pop()

    -- Draw orbiting stars
    for i = 0, num_stars - 1 do
      local angle_offset = (i / num_stars) * math.pi * 2
      local orbit_angle = game_time * orbit_speed + angle_offset
      local star_spin = game_time * star_spin_speed * (i % 2 == 0 and 1 or -1)

      -- Stars orbit the smile and spin around themselves
      game:push_trs(emoji.x, emoji.y, orbit_angle, 1, 1)
          game:push_trs(orbit_radius, 0, star_spin, star_scale, star_scale)
              game:draw_image('star', 0, 0)
          game:pop()
      game:pop()
    end
  end)
  --}}}
end
