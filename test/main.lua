print("main.lua loaded - Input Test (Step 11: Rebinding Capture)")

set_shape_filter("rough")

local screen_w, screen_h = 480, 270

-- Use input_bind_all to create default bindings for all keys/mouse buttons
input_bind_all()
print("input_bind_all() called - all keys now have actions")

-- Set up chord: Shift+Space triggers 'super_action'
input_bind_chord('super_action', {'lshift', 'space'})
print("Chord 'super_action' = Shift + Space")

-- Set up sequence: double-tap D triggers 'dash'
-- Format: {action1, delay, action2} - press D, then D again within 0.3s
input_bind_sequence('dash', {'d', 0.3, 'd'})
print("Sequence 'dash' = D, D (within 0.3s)")

-- Set up hold: hold Space for 1 second triggers 'charge'
input_bind_hold('charge', 1.0, 'space')
print("Hold 'charge' = hold Space for 1.0s")

-- Set up custom action bindings (keyboard + mouse)
input_bind('move_up', 'key:w')
input_bind('move_up', 'key:up')
input_bind('move_down', 'key:s')
input_bind('move_down', 'key:down')
input_bind('move_left', 'key:a')
input_bind('move_left', 'key:left')
input_bind('move_right', 'key:d')
input_bind('move_right', 'key:right')
input_bind('action', 'key:space')
input_bind('action', 'mouse:1')

-- Add gamepad bindings
input_bind('move_up', 'button:dpup')
input_bind('move_up', 'axis:lefty-')     -- Left stick up (negative Y)
input_bind('move_down', 'button:dpdown')
input_bind('move_down', 'axis:lefty+')   -- Left stick down (positive Y)
input_bind('move_left', 'button:dpleft')
input_bind('move_left', 'axis:leftx-')   -- Left stick left (negative X)
input_bind('move_right', 'button:dpright')
input_bind('move_right', 'axis:leftx+')  -- Left stick right (positive X)
input_bind('action', 'button:a')
input_bind('action', 'button:x')

print("Gamepad: " .. (gamepad_is_connected() and "Connected" or "Not connected"))
print("Custom action bindings set up (keyboard + mouse + gamepad)")

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

-- Input test state
local move_speed = 150
local pressed_flash = 0  -- Flash timer when action is triggered
local cursor_radius = 8  -- Cursor size (affected by scroll wheel)
local last_input_type = "keyboard"  -- Track for change detection
local capture_active = false  -- Track capture mode state

-- Helper to draw objects only (no background) - for outline layer
local function draw_objects(layer)
    -- Draw ball (flashes white when action is triggered)
    local r, g, b = hsv_to_rgb(ball.hue, 1, 1)
    if pressed_flash > 0 then
        -- Flash to white
        local flash_t = pressed_flash / 0.3
        r = math.floor(r + (255 - r) * flash_t)
        g = math.floor(g + (255 - g) * flash_t)
        b = math.floor(b + (255 - b) * flash_t)
    end
    layer_circle(layer, ball.x, ball.y, ball.radius, rgba(r, g, b, 255))

    -- Draw mouse cursor (green when clicking, white otherwise)
    local mx, my = mouse_position()
    local cursor_color
    if mouse_is_down(1) then
        cursor_color = rgba(100, 255, 100, 255)  -- Green when left click
    elseif mouse_is_down(2) then
        cursor_color = rgba(100, 100, 255, 255)  -- Blue when middle click (SDL button 2)
    elseif mouse_is_down(3) then
        cursor_color = rgba(255, 100, 100, 255)  -- Red when right click (SDL button 3)
    else
        cursor_color = rgba(255, 255, 255, 255)  -- White normally
    end
    layer_circle(layer, mx, my, cursor_radius, cursor_color)

    -- Draw emoji (disappears when holding shift)
    if not key_is_down('shift') then
        layer_push(layer, emoji.x, emoji.y, emoji.rotation, smile_scale, smile_scale)
            layer_draw_texture(layer, smile_tex, 0, 0)
        layer_pop(layer)
    end

    -- Draw orbiting stars (disappear when pressing their number key 1-5)
    for i = 0, num_stars - 1 do
        local angle_offset = (i / num_stars) * math.pi * 2
        local orbit_angle = game_time * orbit_speed + angle_offset
        local star_spin = game_time * star_spin_speed * (i % 2 == 0 and 1 or -1)

        -- Each star hides when its number key is held
        local key_name = tostring(i + 1)
        if not key_is_down(key_name) then
            layer_push(layer, emoji.x, emoji.y, orbit_angle, 1, 1)
                layer_push(layer, orbit_radius, 0, star_spin, star_scale, star_scale)
                    layer_draw_texture(layer, star_tex, 0, 0)
                layer_pop(layer)
            layer_pop(layer)
        end
    end
end

function update(dt)
    game_time = game_time + dt

    -- === Input Test: Control ball with input_get_vector (normalized diagonal) ===
    -- This replaces 4 separate is_down checks and normalizes diagonal movement
    local move_x, move_y = input_get_vector('move_left', 'move_right', 'move_up', 'move_down')

    -- Apply movement (diagonal will be ~0.707 speed, not 1.414x)
    ball.x = ball.x + move_x * move_speed * dt
    ball.y = ball.y + move_y * move_speed * dt

    -- Check is_pressed for flash effect - triggers on space OR left click
    if is_pressed('action') then
        pressed_flash = 0.3
        print("ACTION PRESSED! (space or left click)")
    end

    -- Check is_released
    if is_released('action') then
        print("ACTION RELEASED!")
    end

    -- === Chord test: Shift+Space triggers 'super_action' ===
    if is_pressed('super_action') then
        pressed_flash = 0.5  -- Longer flash for chord
        print("SUPER ACTION PRESSED! (Shift + Space chord)")
    end
    if is_released('super_action') then
        print("SUPER ACTION RELEASED!")
    end

    -- === Sequence test: double-tap D triggers 'dash' ===
    if is_pressed('dash') then
        pressed_flash = 0.5  -- Flash for sequence
        print("DASH PRESSED! (D, D sequence)")
    end
    if is_released('dash') then
        print("DASH RELEASED!")
    end

    -- === Hold test: hold Space for 1s triggers 'charge' ===
    -- Show charge progress while holding (before it triggers)
    local charge_duration = input_get_hold_duration('charge')
    if charge_duration > 0 and charge_duration < 1.0 then
        -- Could use this to show a charge bar: charge_duration / 1.0 = progress 0-1
        print(string.format("Charging: %.1f%%", charge_duration * 100))
    end
    if is_pressed('charge') then
        pressed_flash = 0.5
        print("CHARGE TRIGGERED! (held Space for 1s)")
    end
    if is_released('charge') then
        print("CHARGE RELEASED!")
    end

    -- === Test unbind/rebind with U/R keys ===
    if is_pressed('u') then
        input_unbind('action', 'mouse:1')
        print("UNBIND: mouse:1 removed from 'action' - left click no longer triggers flash!")
    end
    if is_pressed('r') then
        input_bind('action', 'mouse:1')
        print("REBIND: mouse:1 added back to 'action' - left click works again!")
    end

    -- Test bind_all actions (pressing Q should print via 'q' action)
    if is_pressed('q') then
        print("Q pressed via bind_all action 'q'")
    end
    if is_pressed('mouse_2') then
        print("Middle mouse pressed via bind_all action 'mouse_2'")
    end
    if is_pressed('mouse_3') then
        print("Right mouse pressed via bind_all action 'mouse_3'")
    end

    -- === Gamepad tests ===
    if gamepad_is_connected() then
        -- Test gamepad buttons via action system (A and X are bound to 'action')
        -- These should flash the ball just like space/left-click

        -- Test specific button presses
        if is_pressed('button_a') then
            print("Gamepad A pressed!")
            pressed_flash = 0.3
        end
        if is_pressed('button_b') then
            print("Gamepad B pressed!")
        end
        if is_pressed('button_x') then
            print("Gamepad X pressed!")
            pressed_flash = 0.3
        end
        if is_pressed('button_y') then
            print("Gamepad Y pressed!")
        end

        -- Print raw axis values on G key (for debugging)
        if is_pressed('g') then
            print(string.format("Gamepad axes - LX:%.2f LY:%.2f RX:%.2f RY:%.2f LT:%.2f RT:%.2f",
                gamepad_get_axis('leftx'), gamepad_get_axis('lefty'),
                gamepad_get_axis('rightx'), gamepad_get_axis('righty'),
                gamepad_get_axis('triggerleft'), gamepad_get_axis('triggerright')))
        end

        -- Show significant stick movement
        local lx, ly = gamepad_get_axis('leftx'), gamepad_get_axis('lefty')
        if math.abs(lx) > 0.5 or math.abs(ly) > 0.5 then
            -- Left stick controls ball movement via action bindings (move_left, etc.)
            -- This should already work via input_get_vector
        end

        -- Show trigger values when pressed
        local lt, rt = gamepad_get_axis('triggerleft'), gamepad_get_axis('triggerright')
        if lt > 0.5 then
            print(string.format("Left trigger: %.2f", lt))
        end
        if rt > 0.5 then
            print(string.format("Right trigger: %.2f", rt))
        end
    end

    -- === Raw mouse tests (for cursor color, wheel, delta) ===
    if mouse_is_pressed(2) then
        print("Middle mouse PRESSED!")  -- SDL button 2 = middle
    end
    if mouse_is_pressed(3) then
        print("Right mouse PRESSED!")  -- SDL button 3 = right
    end

    if mouse_is_released(2) then
        print("Middle mouse RELEASED!")
    end
    if mouse_is_released(3) then
        print("Right mouse RELEASED!")
    end

    -- Scroll wheel changes cursor size
    local wheel_x, wheel_y = mouse_wheel()
    if wheel_y ~= 0 then
        cursor_radius = math.max(4, math.min(30, cursor_radius + wheel_y * 2))
        print("Wheel Y: " .. wheel_y .. ", cursor_radius: " .. cursor_radius)
    end
    if wheel_x ~= 0 then
        print("Wheel X: " .. wheel_x)
    end

    -- === Input type detection test ===
    local current_input_type = input_get_last_type()
    if current_input_type ~= last_input_type then
        print("Input type changed: " .. last_input_type .. " -> " .. current_input_type)
        last_input_type = current_input_type
    end

    -- === Capture mode test (press C to start, then press any key/button) ===
    if is_pressed('c') and not capture_active then
        input_start_capture()
        capture_active = true
        print("CAPTURE MODE (C): Press any key, mouse button, or gamepad button...")
    end

    if capture_active then
        local captured = input_get_captured()
        if captured then
            print("CAPTURED: " .. captured)
            input_stop_capture()
            capture_active = false
        end
    end

    -- === Utility function test: input_any_pressed / input_get_pressed_action ===
    -- Press P to enable, then press any key to see it reported via Lua
    if is_pressed('p') then
        print("Press any key to see input_get_pressed_action() in action...")
    end
    if input_any_pressed() then
        local action = input_get_pressed_action()
        if action and action ~= 'p' then  -- Don't spam on P itself
            print("input_get_pressed_action(): " .. action)
        end
    end

    -- Decay flash timers
    if pressed_flash > 0 then
        pressed_flash = pressed_flash - dt
    end

    -- === Update ball (no auto-movement, just keep in bounds) ===
    ball.hue = (ball.hue + ball.hue_speed * dt) % 360

    -- Keep ball in bounds
    if ball.x - ball.radius < 0 then ball.x = ball.radius end
    if ball.x + ball.radius > screen_w then ball.x = screen_w - ball.radius end
    if ball.y - ball.radius < 0 then ball.y = ball.radius end
    if ball.y + ball.radius > screen_h then ball.y = screen_h - ball.radius end

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
