print("main.lua loaded - Audio Test (Phase 6)")

set_shape_filter("rough")

local screen_w, screen_h = 480, 270

-- Load sound and music
local hit = sound_load('assets/player_death.ogg')
local bgm = music_load('assets/Recettear OST - Closed Shop.ogg')
print("Sound and music loaded!")

-- Background layer
local game_layer = layer_create('game')
local bg_color = rgba(48, 49, 50, 255)
local white = rgba(255, 255, 255, 255)

-- Moving ball
local ball_x = screen_w / 2
local ball_y = screen_h / 2
local ball_vx = 100
local ball_vy = 80

-- Master pitch state
local master_pitch = 1.0

-- Volume oscillation state
local volume_time = 0
local cycle_duration = 10  -- 5 seconds up, 5 seconds down

function update(dt)
    -- Update volume oscillation (triangle wave: 0→1 in 5s, 1→0 in 5s)
    volume_time = volume_time + dt
    local cycle_pos = volume_time % cycle_duration
    local volume
    if cycle_pos < 5 then
        volume = cycle_pos / 5  -- 0 to 1 over 5 seconds
    else
        volume = 1 - (cycle_pos - 5) / 5  -- 1 to 0 over 5 seconds
    end
    music_set_volume(volume)
    print(string.format("Music volume: %.0f%%", volume * 100))
    -- Update ball position
    ball_x = ball_x + ball_vx * dt
    ball_y = ball_y + ball_vy * dt

    -- Bounce off walls
    if ball_x < 30 or ball_x > screen_w - 30 then ball_vx = -ball_vx end
    if ball_y < 30 or ball_y > screen_h - 30 then ball_vy = -ball_vy end

    -- === Sound effects ===
    -- Space: normal playback
    if key_is_pressed('space') then
        sound_play(hit)
        print("Sound: normal")
    end

    -- R: random pitch
    if key_is_pressed('r') then
        local pitch = 0.9 + math.random() * 0.2
        sound_play(hit, 1.0, pitch)
        print(string.format("Sound: pitch=%.2f", pitch))
    end

    -- === Music ===
    -- M: play music (loop)
    if key_is_pressed('m') then
        music_play(bgm, true)
        print("Music: playing (loop)")
    end

    -- N: stop music
    if key_is_pressed('n') then
        music_stop()
        print("Music: stopped")
    end

    -- === Master pitch (slow-mo) ===
    -- Down arrow: decrease pitch (slow-mo)
    if key_is_pressed('down') then
        master_pitch = math.max(0.25, master_pitch - 0.25)
        audio_set_master_pitch(master_pitch)
        print(string.format("Master pitch: %.2f", master_pitch))
    end

    -- Up arrow: increase pitch
    if key_is_pressed('up') then
        master_pitch = math.min(2.0, master_pitch + 0.25)
        audio_set_master_pitch(master_pitch)
        print(string.format("Master pitch: %.2f", master_pitch))
    end

    -- === Volume controls ===
    -- 1: sound volume 50%
    if key_is_pressed('1') then
        sound_set_volume(0.5)
        print("Sound volume: 50%")
    end
    -- 2: sound volume 100%
    if key_is_pressed('2') then
        sound_set_volume(1.0)
        print("Sound volume: 100%")
    end
    -- Music volume auto-oscillates (0→100%→0 over 10 seconds)

    -- Draw
    layer_rectangle(game_layer, 0, 0, screen_w, screen_h, bg_color)
    layer_circle(game_layer, ball_x, ball_y, 30, white)
end
