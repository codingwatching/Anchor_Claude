require('anchor.class')

--[[
  Timer child object for scheduling delayed, repeating, and conditional callbacks.

  Usage:
    self:add(timer())
    self.timer:after(1, function() print('fired after 1s') end)
    self.timer:every(0.5, 'attack', function() self:attack() end)

  Timer is added as a child object. When the parent dies, the timer dies automatically.
  All timer methods support optional naming - named timers can be cancelled, triggered,
  and automatically replace previous timers with the same name.

  Timer methods:
    Delayed:     after, every, during, tween
    Conditional: watch, when, cooldown
    Varying:     every_step, during_step
    Utility:     cancel, trigger, set_multiplier, get_time_left
]]
timer = object:extend()

--[[
  Creates a new timer.

  Usage:
    self:add(timer())

  The timer is automatically named 'timer' and accessible as self.timer on the parent.
]]
function timer:new()
  object.new(self, 'timer')
  self.entries = {}
  self.next_id = 1
end

--[[
  Internal: generates unique ID for anonymous timers.
]]
function timer:uid()
  local id = "_anon_" .. self.next_id
  self.next_id = self.next_id + 1
  return id
end

--[[
  Internal: finds entry index by name.

  Returns: index or nil
]]
function timer:find(name)
  for index, entry in ipairs(self.entries) do
    if entry.name == name then return index end
  end
  return nil
end

--[[
  Calls callback once after delay seconds.

  Usage:
    self.timer:after(2, function() print('fired') end)
    self.timer:after(2, 'explosion', function() self:explode() end)

  Behavior:
    - Anonymous timers get auto-generated names
    - Named timers replace previous timers with same name
    - Removed after firing

  Returns: nothing
]]
function timer:after(delay, name_or_callback, callback_function)
  local name, callback
  if type(name_or_callback) == 'string' then
    name, callback = name_or_callback, callback_function
  else
    name, callback = self:uid(), name_or_callback
  end
  local entry = {name = name, mode = 'after', time = 0, delay = delay, callback = callback}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Calls callback repeatedly every delay seconds.

  Usage:
    self.timer:every(0.5, function() print('tick') end)
    self.timer:every(0.5, 'spawn', function() self:spawn() end, 10, function() print('done') end)

  Parameters:
    delay    - seconds between calls
    name     - (optional) timer name for cancellation/replacement
    callback - function to call
    times    - (optional) limit number of calls, then stop
    after    - (optional) callback when times limit reached

  Behavior:
    - Fires indefinitely unless times is specified
    - After callback only runs if times limit is reached, not on cancel

  Returns: nothing
]]
function timer:every(delay, name_or_callback, callback_or_times, times_or_after, after_function)
  local name, callback, times, after
  if type(name_or_callback) == 'string' then
    name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function
  else
    name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after
  end
  local entry = {name = name, mode = 'every', time = 0, delay = delay, callback = callback, times = times, after = after, count = 0}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Calls callback every frame for duration seconds.

  Usage:
    self.timer:during(1, function(dt, progress) self.alpha = 1 - progress end)
    self.timer:during(1, 'fade', function(dt, progress) self.alpha = progress end, function() self:kill() end)

  Parameters:
    duration - total seconds to run
    name     - (optional) timer name
    callback - function receiving (dt, progress) where progress is 0 to 1
    after    - (optional) callback when duration completes

  Behavior:
    - Callback runs every frame with dt and progress (0-1)
    - Progress reaches 1 on the final frame

  Returns: nothing
]]
function timer:during(duration, name_or_callback, callback_or_after, after_function)
  local name, callback, after
  if type(name_or_callback) == 'string' then
    name, callback, after = name_or_callback, callback_or_after, after_function
  else
    name, callback, after = self:uid(), name_or_callback, callback_or_after
  end
  local entry = {name = name, mode = 'during', time = 0, duration = duration, callback = callback, after = after}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Interpolates target properties over duration using easing.

  Usage:
    self.timer:tween(0.5, self, {x = 100, y = 200})
    self.timer:tween(0.5, 'move', self, {x = 100}, math.cubic_out, function() print('done') end)

  Parameters:
    duration - seconds for interpolation
    name     - (optional) timer name
    target   - object whose properties will be modified
    values   - table of {property = target_value} pairs
    easing   - (optional) easing function, defaults to math.linear
    after    - (optional) callback when tween completes

  Behavior:
    - Captures initial values at creation time
    - Interpolates each property from initial to target value
    - Properties are set to exact target values on completion

  Returns: nothing
]]
function timer:tween(duration, name_or_target, target_or_values, values_or_easing, easing_or_after, after_function)
  local name, target, values, easing, after
  if type(name_or_target) == 'string' then
    name, target, values, easing, after = name_or_target, target_or_values, values_or_easing, easing_or_after, after_function
  else
    name, target, values, easing, after = self:uid(), name_or_target, target_or_values, values_or_easing, easing_or_after
  end
  easing = easing or math.linear
  local initial_values = {}
  for key, _ in pairs(values) do
    initial_values[key] = target[key]
  end
  local entry = {name = name, mode = 'tween', time = 0, duration = duration, target = target, values = values, initial_values = initial_values, easing = easing, after = after}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Calls callback when parent[field] changes value.

  Usage:
    self.timer:watch('hp', function(current, previous) print("HP: " .. previous .. " -> " .. current) end)

  Parameters:
    field    - name of field on parent object to watch
    name     - (optional) timer name
    callback - function receiving (current_value, previous_value)
    times    - (optional) limit number of triggers
    after    - (optional) callback when times limit reached

  Behavior:
    - Compares field value each frame to previous frame
    - Only fires when value actually changes (using ~= comparison)
    - Watches self.parent[field], not self

  Returns: nothing
]]
function timer:watch(field, name_or_callback, callback_or_times, times_or_after, after_function)
  local name, callback, times, after
  if type(name_or_callback) == 'string' then
    name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function
  else
    name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after
  end
  local initial_value = self.parent[field]
  local entry = {name = name, mode = 'watch', time = 0, field = field, current = initial_value, previous = initial_value, callback = callback, times = times, after = after, count = 0}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Calls callback when condition transitions from false to true.

  Usage:
    self.timer:when(function() return self.hp < 20 end, function() print('low hp!') end)

  Parameters:
    condition_fn - function returning boolean
    name         - (optional) timer name
    callback     - function to call when condition becomes true
    times        - (optional) limit number of triggers
    after        - (optional) callback when times limit reached

  Behavior:
    - Edge trigger: only fires on false->true transition
    - Does NOT fire every frame the condition is true
    - If condition starts true, fires on first frame

  Returns: nothing
]]
function timer:when(condition_fn, name_or_callback, callback_or_times, times_or_after, after_function)
  local name, callback, times, after
  if type(name_or_callback) == 'string' then
    name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function
  else
    name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after
  end
  local entry = {name = name, mode = 'when', time = 0, condition = condition_fn, last_condition = false, callback = callback, times = times, after = after, count = 0}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Calls callback every delay seconds while condition is true.

  Usage:
    self.timer:cooldown(2, function() return self.target end, function() self:attack() end)

  Parameters:
    delay        - cooldown time between fires
    condition_fn - function returning boolean
    name         - (optional) timer name
    callback     - function to call
    times        - (optional) limit number of fires
    after        - (optional) callback when times limit reached

  Behavior:
    - Timer resets to 0 when condition transitions false->true
    - Only fires when BOTH delay elapsed AND condition is true
    - Timer keeps counting while condition is false (holds cooldown)

  Returns: nothing
]]
function timer:cooldown(delay, condition_fn, name_or_callback, callback_or_times, times_or_after, after_function)
  local name, callback, times, after
  if type(name_or_callback) == 'string' then
    name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function
  else
    name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after
  end
  local entry = {name = name, mode = 'cooldown', time = 0, delay = delay, condition = condition_fn, last_condition = false, callback = callback, times = times, after = after, count = 0}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Calls callback with delays varying from start_delay to end_delay.

  Usage:
    self.timer:every_step(0.1, 0.5, 10, function() self:spawn_particle() end)

  Parameters:
    start_delay - delay before first call
    end_delay   - delay before last call
    times       - total number of calls
    name        - (optional) timer name
    callback    - function to call
    step_method - (optional) easing function for delay interpolation, defaults to math.linear
    after       - (optional) callback when all calls complete

  Behavior:
    - Precomputes all delays at creation using step_method easing
    - Useful for effects that speed up or slow down over time

  Returns: nothing
]]
function timer:every_step(start_delay, end_delay, times, name_or_callback, callback_or_step, step_or_after, after_function)
  local name, callback, step_method, after
  if type(name_or_callback) == 'string' then
    name, callback, step_method, after = name_or_callback, callback_or_step, step_or_after, after_function
  else
    name, callback, step_method, after = self:uid(), name_or_callback, callback_or_step, step_or_after
  end
  step_method = step_method or math.linear
  local delays = {}
  for i = 1, times do
    local t = (i - 1)/(times - 1)
    t = step_method(t)
    delays[i] = math.lerp(t, start_delay, end_delay)
  end
  local entry = {name = name, mode = 'every_step', time = 0, delays = delays, callback = callback, after = after, step_index = 1}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Fits as many calls as possible within duration with varying delays.

  Usage:
    self.timer:during_step(2, 0.3, 0.1, function() self:blink() end)

  Parameters:
    duration    - total time window
    start_delay - initial delay between calls
    end_delay   - final delay between calls
    name        - (optional) timer name
    callback    - function to call
    step_method - (optional) easing function, defaults to math.linear
    after       - (optional) callback when complete

  Behavior:
    - Calculates how many calls fit: ceil(2 * duration / (start + end))
    - Like every_step but you specify duration instead of times

  Returns: nothing
]]
function timer:during_step(duration, start_delay, end_delay, name_or_callback, callback_or_step, step_or_after, after_function)
  local name, callback, step_method, after
  if type(name_or_callback) == 'string' then
    name, callback, step_method, after = name_or_callback, callback_or_step, step_or_after, after_function
  else
    name, callback, step_method, after = self:uid(), name_or_callback, callback_or_step, step_or_after
  end
  step_method = step_method or math.linear
  local times = math.ceil(2*duration/(start_delay + end_delay))
  times = math.max(times, 2)
  local delays = {}
  for i = 1, times do
    local t = (i - 1)/(times - 1)
    t = step_method(t)
    delays[i] = math.lerp(t, start_delay, end_delay)
  end
  local entry = {name = name, mode = 'during_step', time = 0, delays = delays, callback = callback, after = after, step_index = 1}
  if self:find(name) then
    self.entries[self:find(name)] = entry
  else
    table.insert(self.entries, entry)
  end
end

--[[
  Cancels a named timer.

  Usage:
    self.timer:cancel('attack')

  Behavior:
    - Marks timer for removal at end of frame
    - Does NOT call the after callback
    - Safe to call during iteration (uses cancelled flag)

  Returns: nothing
]]
function timer:cancel(name)
  local index = self:find(name)
  if index then self.entries[index].cancelled = true end
end

--[[
  Fires a named timer immediately and resets it.

  Usage:
    self.timer:every(10, 'attack', function() self:attack() end)
    self.timer:trigger('attack')  -- fires now, resets timer

  Behavior:
    - after: fires callback, marks as cancelled (one-shot)
    - every, cooldown, every_step, during_step: fires callback, resets time to 0
    - watch: fires callback with current/previous values
    - when: fires callback
    - during, tween: not supported (continuous, not discrete)

  Returns: nothing
]]
function timer:trigger(name)
  local index = self:find(name)
  if not index then return end
  local entry = self.entries[index]
  if entry.mode == 'after' then
    entry.callback()
    entry.cancelled = true
  elseif entry.mode == 'every' then
    entry.callback()
    entry.time = 0
  elseif entry.mode == 'cooldown' then
    entry.callback()
    entry.time = 0
  elseif entry.mode == 'every_step' then
    entry.callback()
    entry.time = 0
  elseif entry.mode == 'during_step' then
    entry.callback()
    entry.time = 0
  elseif entry.mode == 'watch' then
    entry.callback(entry.current, entry.previous)
  elseif entry.mode == 'when' then
    entry.callback()
  end
end

--[[
  Dynamically adjusts timer speed.

  Usage:
    self.timer:every(1, 'attack', function() self:attack() end)
    self.timer:set_multiplier('attack', 0.5)  -- now fires every 2s
    self.timer:set_multiplier('attack', 2)    -- now fires every 0.5s

  Behavior:
    - Multiplier affects delay/duration: actual_delay = delay * multiplier
    - Multiplier of 2 = twice as fast (half the delay)
    - Multiplier of 0.5 = half speed (double the delay)
    - Defaults to 1 if not set

  Returns: nothing
]]
function timer:set_multiplier(name, multiplier)
  multiplier = multiplier or 1
  local index = self:find(name)
  if not index then return end
  self.entries[index].multiplier = multiplier
end

--[[
  Returns remaining time until timer fires.

  Usage:
    remaining = self.timer:get_time_left('attack')

  Behavior:
    - For after, every, cooldown: returns delay - elapsed time
    - For during, tween: returns duration - elapsed time
    - For every_step, during_step: returns current step delay - elapsed time
    - For watch, when: returns nil (not time-based)
    - Accounts for multiplier

  Returns: seconds remaining, or nil
]]
function timer:get_time_left(name)
  local index = self:find(name)
  if not index then return nil end
  local entry = self.entries[index]
  if entry.mode == 'after' or entry.mode == 'every' or entry.mode == 'cooldown' then
    local delay = entry.delay*(entry.multiplier or 1)
    return delay - entry.time
  elseif entry.mode == 'during' or entry.mode == 'tween' then
    local duration = entry.duration*(entry.multiplier or 1)
    return duration - entry.time
  elseif entry.mode == 'every_step' or entry.mode == 'during_step' then
    return entry.delays[entry.step_index] - entry.time
  else
    return nil
  end
end

--[[
  Internal: processes all timer entries each frame.

  Called automatically by the object update system.
]]
function timer:update(dt)
  for index, entry in ipairs(self.entries) do
    if entry.cancelled then
      entry.to_be_removed = true
      goto continue
    end
    entry.time = entry.time + dt

    if entry.mode == 'after' then
      local delay = entry.delay*(entry.multiplier or 1)
      if entry.time >= delay then
        entry.callback()
        entry.to_be_removed = true
      end

    elseif entry.mode == 'every' then
      local delay = entry.delay*(entry.multiplier or 1)
      if entry.time >= delay then
        entry.callback()
        entry.time = entry.time - delay
        if entry.times then
          entry.count = entry.count + 1
          if entry.count >= entry.times then
            if entry.after then entry.after() end
            entry.to_be_removed = true
          end
        end
      end

    elseif entry.mode == 'during' then
      local duration = entry.duration*(entry.multiplier or 1)
      local progress = math.min(entry.time/duration, 1)
      entry.callback(dt, progress)
      if entry.time >= duration then
        if entry.after then entry.after() end
        entry.to_be_removed = true
      end

    elseif entry.mode == 'tween' then
      local duration = entry.duration*(entry.multiplier or 1)
      local progress = math.min(entry.time/duration, 1)
      local eased = entry.easing(progress)
      for key, target_value in pairs(entry.values) do
        entry.target[key] = math.lerp(eased, entry.initial_values[key], target_value)
      end
      if entry.time >= duration then
        if entry.after then entry.after() end
        entry.to_be_removed = true
      end

    elseif entry.mode == 'watch' then
      entry.previous = entry.current
      entry.current = self.parent[entry.field]
      if entry.previous ~= entry.current then
        entry.callback(entry.current, entry.previous)
        if entry.times then
          entry.count = entry.count + 1
          if entry.count >= entry.times then
            if entry.after then entry.after() end
            entry.to_be_removed = true
          end
        end
      end

    elseif entry.mode == 'when' then
      local current_condition = entry.condition()
      if current_condition and not entry.last_condition then
        entry.callback()
        if entry.times then
          entry.count = entry.count + 1
          if entry.count >= entry.times then
            if entry.after then entry.after() end
            entry.to_be_removed = true
          end
        end
      end
      entry.last_condition = current_condition

    elseif entry.mode == 'cooldown' then
      local delay = entry.delay*(entry.multiplier or 1)
      local current_condition = entry.condition()
      if current_condition and not entry.last_condition then
        entry.time = 0
      end
      if entry.time >= delay and current_condition then
        entry.callback()
        entry.time = 0
        if entry.times then
          entry.count = entry.count + 1
          if entry.count >= entry.times then
            if entry.after then entry.after() end
            entry.to_be_removed = true
          end
        end
      end
      entry.last_condition = current_condition

    elseif entry.mode == 'every_step' then
      if entry.time >= entry.delays[entry.step_index] then
        entry.callback()
        entry.time = entry.time - entry.delays[entry.step_index]
        entry.step_index = entry.step_index + 1
        if entry.step_index > #entry.delays then
          if entry.after then entry.after() end
          entry.to_be_removed = true
        end
      end

    elseif entry.mode == 'during_step' then
      if entry.time >= entry.delays[entry.step_index] then
        entry.callback()
        entry.time = entry.time - entry.delays[entry.step_index]
        entry.step_index = entry.step_index + 1
        if entry.step_index > #entry.delays then
          if entry.after then entry.after() end
          entry.to_be_removed = true
        end
      end
    end

    ::continue::
  end

  for i = #self.entries, 1, -1 do
    if self.entries[i].to_be_removed then
      table.remove(self.entries, i)
    end
  end
end
