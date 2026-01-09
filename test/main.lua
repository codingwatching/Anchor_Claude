print("main.lua loaded - Physics Test (Phase 7 - Step 10: Spatial Queries)")

set_shape_filter("rough")
input_bind_all()

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

-- Helper to get a random ball
local function get_random_ball()
    if #dynamic_bodies == 0 then return nil end
    return dynamic_bodies[math.random(1, #dynamic_bodies)]
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
    -- Create body on Space
    if is_pressed('space') then
        local x = math.random(50, screen_w - 50)
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

    -- P: Random ball gets random high impulse
    if is_pressed('p') then
        local ball = get_random_ball()
        if ball then
            local ix = math.random(-25, 25)
            local iy = math.random(-40, -10)
            physics_apply_impulse(ball, ix, iy)
            print(string.format("Applied impulse (%.0f, %.0f) to random ball", ix, iy))
        end
    end

    -- L: Random ball gets random high angular impulse
    if is_pressed('l') then
        local ball = get_random_ball()
        if ball then
            local angular = (math.random() - 0.5) * 2.0  -- Range: -1.0 to 1.0
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
