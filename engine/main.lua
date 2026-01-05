print("main.lua loaded")

local step_count = 0

function update(dt)
    step_count = step_count + 1
    if step_count % 144 == 0 then
        print("1 second passed (" .. step_count .. " steps)")
    end
end
