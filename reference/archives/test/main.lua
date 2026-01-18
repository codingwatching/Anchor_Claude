print("main.lua loaded - Phase 8: Random + Physics Test")

set_filter_mode("rough")
input_bind_all()

-- Test random determinism: seed and print first 5 values
print("Testing random determinism:")
random_seed(12345)
print(string.format("  Seed 12345 -> float_01: %.6f, %.6f, %.6f",
    random_float_01(), random_float_01(), random_float_01()))
print(string.format("  Seed 12345 -> int(1,10): %d, %d, %d",
    random_int(1, 10), random_int(1, 10), random_int(1, 10)))

-- Re-seed and verify same sequence
random_seed(12345)
print(string.format("  Re-seed 12345 -> float_01: %.6f, %.6f, %.6f",
    random_float_01(), random_float_01(), random_float_01()))
print(string.format("  Re-seed 12345 -> int(1,10): %d, %d, %d",
    random_int(1, 10), random_int(1, 10), random_int(1, 10)))

-- Verify get_seed works
random_seed(99999)
print(string.format("  After seed(99999), get_seed() = %d", random_get_seed()))

-- Test convenience functions
print("Testing convenience functions:")
random_seed(12345)
print(string.format("  random_angle(): %.4f, %.4f, %.4f", random_angle(), random_angle(), random_angle()))
print(string.format("  random_sign(): %d, %d, %d, %d, %d", random_sign(), random_sign(), random_sign(), random_sign(), random_sign()))
print(string.format("  random_bool(): %s, %s, %s, %s, %s", tostring(random_bool()), tostring(random_bool()), tostring(random_bool()), tostring(random_bool()), tostring(random_bool())))
local bool80 = {}
for i = 1, 10 do bool80[i] = tostring(random_bool(80)) end
print(string.format("  random_bool(80): %s (80%% chance true)", table.concat(bool80, ", ")))
local sign80 = {}
for i = 1, 10 do sign80[i] = tostring(random_sign(80)) end
print(string.format("  random_sign(80): %s (80%% chance +1)", table.concat(sign80, ", ")))

-- Test separate RNG instance
print("Testing separate RNG instance:")
local level_rng = random_create(42)
print(string.format("  level_rng seed 42 -> int(1,100): %d, %d, %d",
    random_int(1, 100, level_rng), random_int(1, 100, level_rng), random_int(1, 100, level_rng)))
-- Recreate to verify determinism
level_rng = random_create(42)
print(string.format("  level_rng re-seed 42 -> int(1,100): %d, %d, %d (should match)",
    random_int(1, 100, level_rng), random_int(1, 100, level_rng), random_int(1, 100, level_rng)))

-- Test random_normal (Gaussian distribution)
print("Testing random_normal:")
random_seed(12345)
local normals = {}
for i = 1, 10 do normals[i] = string.format("%.2f", random_normal()) end
print(string.format("  random_normal(): %s", table.concat(normals, ", ")))
local normals2 = {}
for i = 1, 10 do normals2[i] = string.format("%.2f", random_normal(100, 15)) end
print(string.format("  random_normal(100, 15): %s", table.concat(normals2, ", ")))

-- Test array functions
print("Testing array functions:")
random_seed(12345)
local fruits = {'apple', 'banana', 'cherry', 'date', 'elderberry'}
print(string.format("  random_choice(fruits): %s, %s, %s", random_choice(fruits), random_choice(fruits), random_choice(fruits)))

local deck = {'A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K'}
local hand = random_choices(deck, 5)
print(string.format("  random_choices(deck, 5): %s", table.concat(hand, ", ")))

-- Test weighted selection (5 runs of 20 picks each)
print("Testing random_weighted (70/25/5 = common/rare/epic):")
random_seed(12345)
local weights = {70, 25, 5}
local loot = {'common', 'rare', 'epic'}
for run = 1, 5 do
    local results = {}
    for i = 1, 20 do
        local idx = random_weighted(weights)
        results[i] = loot[idx]
    end
    print(string.format("  Run %d: %s", run, table.concat(results, ", ")))
end

-- Test Perlin noise
print("Testing noise (Perlin):")
local noise_vals = {}
for i = 0, 9 do
    noise_vals[i+1] = string.format("%.3f", noise(i * 0.1))
end
print(string.format("  noise(0..0.9 step 0.1): %s", table.concat(noise_vals, ", ")))

-- 2D noise - show a 8x8 grid (small scale 0.1 to show smoothness)
print("  2D noise grid (8x8, scale 0.1):")
for y = 0, 7 do
    local row = {}
    for x = 0, 7 do
        row[x+1] = string.format("%+.2f", noise(x * 0.1 + 0.05, y * 0.1 + 0.05))
    end
    print(string.format("    %s", table.concat(row, " ")))
end

-- Verify noise is deterministic (same input = same output)
print(string.format("  noise(1.5, 2.5, 3.5) = %.6f (should be constant)", noise(1.5, 2.5, 3.5)))
print(string.format("  noise(1.5, 2.5, 3.5) = %.6f (should match above)", noise(1.5, 2.5, 3.5)))

-- Set seed for gameplay
random_seed(os.time())
print(string.format("  Gameplay seed: %d", random_get_seed()))

local screen_w, screen_h = 480, 270

-- Colors (twitter_emoji theme from super emoji box)
local bg_color = rgba(231, 232, 233, 255)
local green = rgba(122, 179, 87, 255)

-- Layers (bottom to top: background, shadow, outline, game, ui)
local bg_layer = layer_create('background')
local shadow_layer = layer_create('shadow')
local outline_layer = layer_create('outline')
local game_layer = layer_create('game')
local ui_layer = layer_create('ui')  -- No shaders, for overlays like sensor zones

-- Shaders
local shadow_shader = shader_load_file("shaders/shadow.frag")
local outline_shader = shader_load_file("shaders/outline.frag")

-- Font
font_load('main', 'assets/LanaPixel.ttf', 11)

-- Textures
local smile_tex = texture_load("assets/slight_smile.png")
local smile_size = 20
local smile_scale = smile_size / texture_get_width(smile_tex)

-- Initialize physics
physics_init()
physics_set_gravity(0, 500)
physics_set_meter_scale(64)

-- Register physics tags
physics_register_tag('ground')
physics_register_tag('ball')

-- Enable collision between balls and ground
physics_enable_collision('ball', 'ground')
physics_enable_collision('ball', 'ball')

-- Register a sensor zone tag
physics_register_tag('zone')
physics_enable_sensor('zone', 'ball')  -- Zone detects balls entering/exiting
physics_enable_sensor('ball', 'zone')  -- Ball also needs enableSensorEvents=true

-- Create static ground body with box shape (90% width, drawn as rounded rectangle)
local ground_width = screen_w * 0.9
local ground_height = 12
local ground_radius = 6
local ground_body = physics_create_body('static', screen_w/2, screen_h - 20)
physics_add_box(ground_body, 'ground', ground_width, ground_height)

-- Create left and right walls (go up to 10% from top)
local wall_width = 12
local wall_top = screen_h * 0.1
local ground_top = screen_h - 20 - ground_height / 2
local wall_height = ground_top - wall_top
local wall_x_offset = ground_width / 2 - wall_width / 2
local wall_y = wall_top + wall_height / 2

local left_wall = physics_create_body('static', screen_w/2 - wall_x_offset, wall_y)
physics_add_box(left_wall, 'ground', wall_width, wall_height)

local right_wall = physics_create_body('static', screen_w/2 + wall_x_offset, wall_y)
physics_add_box(right_wall, 'ground', wall_width, wall_height)

-- Create a sensor zone in the middle (balls pass through but trigger events)
local zone_body = physics_create_body('static', screen_w/2, screen_h/2)
local zone_shape = physics_add_box(zone_body, 'zone', 100, 60, {sensor = true})

print("Created ground, walls, and sensor zone")

-- Table to track dynamic bodies and their shapes
local dynamic_bodies = {}
local dynamic_shapes = {}

-- Helper to get a random ball (using new random_int)
local function get_random_ball()
    if #dynamic_bodies == 0 then return nil end
    return dynamic_bodies[random_int(1, #dynamic_bodies)]
end

local function draw_objects(layer)
    -- Draw ground (plain rectangle)
    local gx, gy = physics_get_position(ground_body)
    layer_rectangle(layer, gx - ground_width/2, gy - ground_height/2, ground_width, ground_height, green)

    -- Draw walls (rounded tops)
    local lx, ly = physics_get_position(left_wall)
    layer_rectangle(layer, lx - wall_width/2, ly - wall_height/2 + ground_radius, wall_width, wall_height - ground_radius, green)
    layer_circle(layer, lx, ly - wall_height/2 + ground_radius, ground_radius, green)

    local rx, ry = physics_get_position(right_wall)
    layer_rectangle(layer, rx - wall_width/2, ry - wall_height/2 + ground_radius, wall_width, wall_height - ground_radius, green)
    layer_circle(layer, rx, ry - wall_height/2 + ground_radius, ground_radius, green)

    -- Draw dynamic bodies (emoji balls)
    for i, body in ipairs(dynamic_bodies) do
        if physics_body_is_valid(body) then
            local x, y = physics_get_position(body)
            local angle = physics_get_angle(body)
            layer_push(layer, x, y, angle, smile_scale, smile_scale)
            layer_draw_texture(layer, smile_tex, 0, 0)
            layer_pop(layer)
        end
    end
end

function update(dt)
    -- Create body on Space (using new random_int)
    if is_pressed('space') then
        local x = random_int(50, screen_w - 50)
        local y = 50
        local body = physics_create_body('dynamic', x, y)
        local shape = physics_add_circle(body, 'ball', smile_size / 2)
        physics_shape_set_restitution(shape, 1.0)  -- Bouncy!
        table.insert(dynamic_bodies, body)
        table.insert(dynamic_shapes, shape)
        print(string.format("Created bouncy ball at %.0f, %.0f", x, y))
    end

    -- Destroy oldest body on D
    if is_pressed('d') and #dynamic_bodies > 0 then
        local body = table.remove(dynamic_bodies, 1)
        table.remove(dynamic_shapes, 1)
        physics_destroy_body(body)
        print(string.format("Destroyed body - Remaining: %d", #dynamic_bodies))
    end

    -- P: Random ball gets random high impulse (using new random_int)
    if is_pressed('p') then
        local ball = get_random_ball()
        if ball then
            local ix = random_int(-25, 25)
            local iy = random_int(-40, -10)
            physics_apply_impulse(ball, ix, iy)
            print(string.format("Applied impulse (%.0f, %.0f) to random ball", ix, iy))
        end
    end

    -- L: Random ball gets random high angular impulse (using new random_float)
    if is_pressed('l') then
        local ball = get_random_ball()
        if ball then
            local angular = random_float(-1.0, 1.0)
            physics_apply_angular_impulse(ball, angular)
            print(string.format("Applied angular impulse %.2f to random ball", angular))
        end
    end

    -- K held: Apply wind force (up and right) to all balls
    if is_down('k') then
        for _, body in ipairs(dynamic_bodies) do
            if physics_body_is_valid(body) then
                physics_apply_force(body, 60, -40)
            end
        end
    end

    -- G held: Lower gravity scale, released goes back to normal
    if is_pressed('g') then
        for _, body in ipairs(dynamic_bodies) do
            if physics_body_is_valid(body) then
                physics_set_gravity_scale(body, 0.2)
            end
        end
        print("Low gravity!")
    end
    if is_released('g') then
        for _, body in ipairs(dynamic_bodies) do
            if physics_body_is_valid(body) then
                physics_set_gravity_scale(body, 1.0)
            end
        end
        print("Normal gravity")
    end

    -- E: Print event counts (debug)
    if is_pressed('e') then
        physics_debug_events()
    end

    -- Q: Test spatial query - find all balls in a circle around mouse position
    if is_pressed('q') then
        local mx, my = mouse_position()
        local radius = 50
        local found = physics_query_circle(mx, my, radius, {'ball'})
        print(string.format("Query circle at (%.0f, %.0f) r=%.0f: found %d balls", mx, my, radius, #found))
    end

    -- R: Test AABB query in the zone area
    if is_pressed('r') then
        local zx, zy = physics_get_position(zone_body)
        local found = physics_query_aabb(zx, zy, 100, 60, {'ball'})
        print(string.format("Query AABB in zone: found %d balls", #found))
    end

    -- T: Test point query at mouse position
    if is_pressed('t') then
        local mx, my = mouse_position()
        local found = physics_query_point(mx, my, {'ball', 'ground'})
        print(string.format("Query point at (%.0f, %.0f): found %d bodies", mx, my, #found))
    end

    -- Y: Test raycast from top-left to mouse position
    if is_pressed('y') then
        local mx, my = mouse_position()
        local hit = physics_raycast(0, 0, mx, my, {'ball', 'ground'})
        if hit then
            print(string.format("Raycast hit at (%.1f, %.1f), normal=(%.2f, %.2f), fraction=%.3f",
                hit.point_x, hit.point_y, hit.normal_x, hit.normal_y, hit.fraction))
        else
            print("Raycast: no hit")
        end
    end

    -- U: Test raycast_all from left edge to right edge at mouse Y
    if is_pressed('u') then
        local mx, my = mouse_position()
        local hits = physics_raycast_all(0, my, screen_w, my, {'ball', 'ground'})
        print(string.format("Raycast_all at y=%.0f: %d hits", my, #hits))
        for i, hit in ipairs(hits) do
            print(string.format("  Hit %d: (%.1f, %.1f) fraction=%.3f", i, hit.point_x, hit.point_y, hit.fraction))
        end
    end

    -- Query and print sensor events
    for _, e in ipairs(physics_get_sensor_begin('zone', 'ball')) do
        print("Sensor BEGIN: ball entered zone")
    end
    for _, e in ipairs(physics_get_sensor_end('zone', 'ball')) do
        print("Sensor END: ball left zone")
    end

    -- Query and print collision events
    for _, e in ipairs(physics_get_collision_begin('ball', 'ball')) do
        print("Collision BEGIN: ball hit ball")
    end
    for _, e in ipairs(physics_get_collision_begin('ball', 'ground')) do
        print("Collision BEGIN: ball hit ground")
    end

    -- Draw background
    layer_rectangle(bg_layer, 0, 0, screen_w, screen_h, bg_color)

    -- Draw objects to shadow, outline, and game layers
    draw_objects(shadow_layer)
    draw_objects(outline_layer)
    draw_objects(game_layer)

    -- Draw sensor zone to ui layer (no shaders)
    local zx, zy = physics_get_position(zone_body)
    layer_rectangle(ui_layer, zx - 50, zy - 30, 100, 60, rgba(100, 150, 255, 160))

    -- Draw text
    local white = rgba(255, 255, 255, 255)
    local black = rgba(0, 0, 0, 255)
    layer_draw_text(ui_layer, "Balls: " .. #dynamic_bodies, 'main', 8, 8, black)
    layer_draw_text(ui_layer, "SPACE: spawn  D: destroy  P: impulse  K: wind", 'main', 8, screen_h - 20, black)

    -- Apply shaders
    layer_apply_shader(shadow_layer, shadow_shader)
    layer_shader_set_vec2(outline_layer, outline_shader, "u_pixel_size", 1/screen_w, 1/screen_h)
    layer_apply_shader(outline_layer, outline_shader)

    -- Composite layers (shadow at offset for drop shadow effect)
    layer_draw(bg_layer)
    layer_draw(shadow_layer, 4, 4)
    layer_draw(outline_layer)
    layer_draw(game_layer)
    layer_draw(ui_layer)  -- No shaders, drawn on top
end
