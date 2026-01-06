print("main.lua loaded")

local game = layer_create('game')
local step_count = 0

function update(dt)
    step_count = step_count + 1
    if step_count % 144 == 0 then
        print("1 second passed (" .. step_count .. " steps)")
    end

    -- Draw rectangles
    layer_rectangle(game, 190, 85, 100, 100, 0xFF8000FF)   -- Orange center
    layer_rectangle(game, 10, 10, 50, 30, 0xFF0000FF)      -- Red top-left
    layer_rectangle(game, 420, 230, 50, 30, 0x00FF00FF)    -- Green bottom-right

    -- Test rgba helper
    layer_rectangle(game, 200, 200, 30, 30, rgba(0, 128, 255, 255))  -- Blue
end
