local M = {}

function M.do_something()
    print("other.do_something called")
    local x = nil
    x.foo = 1  -- attempt to index a nil value
end

return M
