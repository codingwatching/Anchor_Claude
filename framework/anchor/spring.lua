require('anchor.class')

--[[
  Spring child object for damped spring animations.

  Usage:
    self:add(spring())
    self.spring:add('scale', 1, 5, 0.5)      -- 5 Hz, moderate bounce
    self.spring:pull('scale', 0.5)

  Spring is added as a child object. When the parent dies, the spring dies automatically.
  A default 'main' spring at value 1 is created on construction.

  Springs are accessed directly: self.spring.main.x, self.spring.scale.x

  Spring methods:
    add          - Add a named spring with frequency/bounce
    pull         - Apply impulse to a spring
    set_target   - Change resting point
    at_rest      - Check if spring has settled
]]
spring = object:extend()

--[[
  Creates a new spring container with default 'main' spring.

  Usage:
    self:add(spring())

  The spring is automatically named 'spring' and accessible as self.spring on the parent.
  A 'main' spring at value 1 is created by default (0.3s duration, 0.5 bounce).
]]
function spring:new()
  object.new(self, 'spring')
  self.spring_names = {}
  self:add('main', 1)
end

--[[
  Adds a new named spring.

  Usage:
    self.spring:add('scale', 1)                 -- default: 5 Hz, 0.5 bounce
    self.spring:add('rotation', 0, 3, 0.3)      -- 3 oscillations/sec, low bounce
    self.spring:add('position', 100, 10, 0.8)   -- 10 oscillations/sec, high bounce

  Parameters:
    name      - string identifier for the spring
    x         - initial value (default 0)
    frequency - oscillations per second (default 5)
    bounce    - bounciness 0-1 (default 0.5, where 0=no overshoot, 1=infinite oscillation)

  Behavior:
    - Spring is accessible as self.spring.name.x
    - Higher frequency = faster oscillation
    - Higher bounce = more overshoot and oscillation
    - bounce=0 is critically damped (smooth, no overshoot)
    - bounce=0.5 has moderate overshoot
    - bounce approaching 1 oscillates forever

  Returns: nothing
]]
function spring:add(name, x, frequency, bounce)
  x = x or 0
  frequency = frequency or 5
  bounce = bounce or 0.5
  if not self[name] then
    table.insert(self.spring_names, name)
  end
  local k = (2*math.pi*frequency)^2
  local d = 4*math.pi*(1 - bounce)*frequency
  self[name] = {
    x = x,
    target_x = x,
    v = 0,
    k = k,
    d = d,
  }
end

--[[
  Applies an impulse to a named spring.

  Usage:
    self.spring:pull('main', 0.5)              -- add 0.5 to current value
    self.spring:pull('scale', 0.3, 10, 0.7)    -- pull with 10 Hz, high bounce

  Parameters:
    name      - spring identifier
    force     - amount to add to current value
    frequency - (optional) new oscillations per second
    bounce    - (optional) new bounciness 0-1

  Behavior:
    - Adds force directly to spring's current x value
    - Spring will oscillate around target_x and settle back
    - Optionally updates frequency/bounce for this spring permanently

  Returns: nothing
]]
function spring:pull(name, force, frequency, bounce)
  local s = self[name]
  if not s then return end
  if frequency then
    s.k = (2*math.pi*frequency)^2
    s.d = 4*math.pi*(1 - (bounce or 0.5))*frequency
  end
  s.x = s.x + force
end

--[[
  Changes the resting point of a named spring.

  Usage:
    self.spring:set_target('scale', 2)  -- spring will animate toward 2

  Parameters:
    name  - spring identifier
    value - new target value

  Behavior:
    - Spring will smoothly animate toward the new target
    - Unlike pull, this changes where the spring settles

  Returns: nothing
]]
function spring:set_target(name, value)
  local s = self[name]
  if s then s.target_x = value end
end

--[[
  Checks if a spring has settled (stopped moving).

  Usage:
    if self.spring:at_rest('scale') then
      print('animation complete')
    end

  Parameters:
    name      - spring identifier
    threshold - (optional) how close to target counts as "at rest" (default 0.01)

  Returns: true if spring value and velocity are both near zero/target
]]
function spring:at_rest(name, threshold)
  threshold = threshold or 0.01
  local s = self[name]
  if not s then return true end
  return math.abs(s.x - s.target_x) < threshold and math.abs(s.v) < threshold
end

--[[
  Internal: updates all springs each frame.

  Called automatically during early_update phase.
  Uses standard damped spring equation: a = -k*(x - target) - d*v
]]
function spring:early_update(dt)
  for _, spring_name in ipairs(self.spring_names) do
    local s = self[spring_name]
    local a = -s.k*(s.x - s.target_x) - s.d*s.v
    s.v = s.v + a*dt
    s.x = s.x + s.v*dt
  end
end
