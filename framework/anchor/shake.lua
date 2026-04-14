require('anchor.class')

--[[
  Shake child object for camera shake effects.

  Usage:
    an.camera:add(shake())
    an.camera.shake:trauma(0.5, 0.3)

  Shake is added as a child of camera. It implements get_transform() which
  camera calls to collect effects from all children.

  Shake types:
    trauma - Perlin noise based, accumulates and decays
]]
shake = object:extend()

--[[
  Creates a new shake container.

  Usage:
    an.camera:add(shake())

  Behavior:
    - Automatically named 'shake' and accessible as parent.shake
    - Initializes trauma system
]]
function shake:new(name)
  object.new(self, name or 'shake')
  self.trauma_instances = {}
  self.trauma_amplitude = {x = 24, y = 24, rotation = 0.2, zoom = 0.2}
  self.trauma_time = 0  -- offset for Perlin noise

  -- Springs for spring-based shakes
  self:add(spring())
  self.spring:add('x', 0, 3, 0.5)
  self.spring:add('y', 0, 3, 0.5)
  self.push_cap = nil
  self.push_used = 0

  -- Handcam (continuous subtle motion)
  self.handcam_enabled = false
  self.handcam_amplitude = {x = 5, y = 5, rotation = 0.02, zoom = 0.02}
  self.handcam_frequency = 0.5
  self.handcam_time = 0
end

--[[
  Adds trauma which produces Perlin noise shake.

  Usage:
    shake:trauma(0.5, 0.3)    -- amount, duration
    shake:trauma(1, 1)         -- full trauma over 1 second

  Parameters:
    amount   - trauma amount (affects intensity via amount^2)
    duration - time in seconds for this trauma to decay to zero (default 0.5)

  Behavior:
    - Multiple trauma calls create independent instances
    - Each instance decays at its own rate
    - Total trauma = sum of all active instances
    - Shake intensity = total_trauma^2 * amplitude * noise
    - Affects all axes (x, y, rotation, zoom) based on configured amplitudes
]]
function shake:trauma(amount, duration, amplitude)
  duration = duration or 0.5
  table.insert(self.trauma_instances, {
    value = amount,
    decay = amount/duration,
    amplitude = amplitude,
  })
end

--[[
  Sets trauma amplitude parameters.

  Usage:
    shake:set_trauma_parameters({x = 20, y = 20, rotation = 0.1, zoom = 0.05})

  Parameters:
    amplitude - table with {x, y, rotation, zoom} amplitudes

  Behavior:
    - Configure once during setup
    - Affects all subsequent trauma calls
]]
function shake:set_trauma_parameters(amplitude)
  if amplitude.x then self.trauma_amplitude.x = amplitude.x end
  if amplitude.y then self.trauma_amplitude.y = amplitude.y end
  if amplitude.rotation then self.trauma_amplitude.rotation = amplitude.rotation end
  if amplitude.zoom then self.trauma_amplitude.zoom = amplitude.zoom end
end

--[[
  Adds a random shake with decay.

  Usage:
    shake:shake(10, 0.3)        -- amplitude, duration
    shake:shake(20, 0.5, 30)    -- with slower jitter rate (30 changes/sec)

  Parameters:
    amplitude - maximum displacement in pixels
    duration  - time in seconds for shake to decay to zero
    frequency - (optional) how many times per second to pick new random offset (default 60)

  Behavior:
    - Random displacement each frame (jittery/chaotic)
    - Amplitude decays linearly over duration
    - Multiple calls create independent instances
]]
function shake:shake(amplitude, duration, frequency)
  frequency = frequency or 60
  if not self.shake_instances then self.shake_instances = {} end
  table.insert(self.shake_instances, {
    amplitude = amplitude,
    duration = duration,
    frequency = frequency,
    time = 0,
    current_x = 0,
    current_y = 0,
    last_change = 0,
  })
end

--[[
  Applies a directional spring impulse.

  Usage:
    shake:push(0, 20)                    -- rightward impulse (angle 0)
    shake:push(math.pi, 15)              -- leftward impulse
    shake:push(math.pi/2, 10, 8, 0.7)   -- downward with custom frequency/bounce

  Parameters:
    angle     - direction in radians (0 = right, pi/2 = down)
    amount    - impulse strength in pixels
    frequency - (optional) oscillation frequency (default 5)
    bounce    - (optional) bounciness 0-1 (default 0.5)

  Behavior:
    - Applies impulse in the specified direction
    - Spring oscillates and settles naturally
    - Multiple calls combine additively
]]
function shake:push(angle, amount, frequency, bounce)
  if self.push_cap then
    local remaining = self.push_cap - self.push_used
    if remaining <= 0 then return end
    amount = math.min(amount, remaining)
    self.push_used = self.push_used + amount
  end
  self.spring:pull('x', math.cos(angle)*amount, frequency, bounce)
  self.spring:pull('y', math.sin(angle)*amount, frequency, bounce)
end

--[[
  Applies a sine wave oscillation.

  Usage:
    shake:sine(0, 10, 5, 0.5)        -- rightward, 10 pixels, 5 Hz, 0.5 seconds
    shake:sine(math.pi/2, 8, 3, 1)   -- downward oscillation

  Parameters:
    angle     - direction in radians (0 = right, pi/2 = down)
    amplitude - maximum displacement in pixels
    frequency - oscillations per second
    duration  - time until oscillation stops

  Behavior:
    - Smooth sinusoidal oscillation along direction
    - Amplitude decays linearly over duration
    - Multiple calls create independent instances
]]
function shake:sine(angle, amplitude, frequency, duration)
  if not self.sine_instances then self.sine_instances = {} end
  table.insert(self.sine_instances, {
    angle = angle,
    amplitude = amplitude,
    frequency = frequency,
    duration = duration,
    time = 0,
  })
end

--[[
  Applies a square wave oscillation.

  Usage:
    shake:square(0, 10, 5, 0.5)        -- rightward, 10 pixels, 5 Hz, 0.5 seconds
    shake:square(math.pi/2, 8, 3, 1)   -- downward oscillation

  Parameters:
    angle     - direction in radians (0 = right, pi/2 = down)
    amplitude - maximum displacement in pixels
    frequency - oscillations per second
    duration  - time until oscillation stops

  Behavior:
    - Sharp alternating displacement (snaps between +/- amplitude)
    - Amplitude decays linearly over duration
    - Multiple calls create independent instances
]]
function shake:square(angle, amplitude, frequency, duration)
  if not self.square_instances then self.square_instances = {} end
  table.insert(self.square_instances, {
    angle = angle,
    amplitude = amplitude,
    frequency = frequency,
    duration = duration,
    time = 0,
  })
end

--[[
  Enables or disables handcam effect (continuous subtle motion).

  Usage:
    shake:handcam(true)                                              -- enable with defaults
    shake:handcam(true, {x = 3, y = 3, rotation = 0.02}, 0.5)       -- custom amplitude, frequency
    shake:handcam(false)                                             -- disable

  Parameters:
    enabled   - true to enable, false to disable
    amplitude - (optional) table with {x, y, rotation} amplitudes
    frequency - (optional) noise frequency multiplier (default 1, higher = faster)

  Behavior:
    - Adds subtle continuous Perlin noise motion
    - Simulates handheld camera feel
    - Runs constantly while enabled (doesn't decay)
]]
function shake:handcam(enabled, amplitude, frequency)
  self.handcam_enabled = enabled
  if amplitude then
    if amplitude.x then self.handcam_amplitude.x = amplitude.x end
    if amplitude.y then self.handcam_amplitude.y = amplitude.y end
    if amplitude.rotation then self.handcam_amplitude.rotation = amplitude.rotation end
    if amplitude.zoom then self.handcam_amplitude.zoom = amplitude.zoom end
  end
  if frequency then self.handcam_frequency = frequency end
end

--[[
  Internal: returns current transform offset for camera.

  Called by camera:get_effects() to collect all child effects.

  Returns: {x, y, rotation, zoom} offsets
]]
function shake:get_transform()
  local ox, oy, r, z = 0, 0, 0, 0

  -- Handcam effect (continuous subtle motion)
  if self.handcam_enabled then
    local t = self.handcam_time*self.handcam_frequency
    ox = ox + self.handcam_amplitude.x*noise(t, 0)
    oy = oy + self.handcam_amplitude.y*noise(0, t)
    r = r + self.handcam_amplitude.rotation*noise(t, t)
    z = z + self.handcam_amplitude.zoom*noise(t*0.7, 0, t)
  end

  -- Trauma effect (Perlin noise, per-instance amplitude or global)
  for _, instance in ipairs(self.trauma_instances) do
    local amp = instance.amplitude or self.trauma_amplitude
    local intensity = instance.value*instance.value
    ox = ox + intensity*amp.x*noise(self.trauma_time*10, 0)
    oy = oy + intensity*amp.y*noise(0, self.trauma_time*10)
    r = r + intensity*amp.rotation*noise(self.trauma_time*10, self.trauma_time*10)
    z = z + intensity*amp.zoom*noise(self.trauma_time*5, 0, self.trauma_time*5)
  end

  -- Spring contribution (offset from rest position)
  ox = ox + self.spring.x.x
  oy = oy + self.spring.y.x

  -- Shake instances contribution
  if self.shake_instances then
    for _, instance in ipairs(self.shake_instances) do
      ox = ox + instance.current_x
      oy = oy + instance.current_y
    end
  end

  -- Sine instances contribution
  if self.sine_instances then
    for _, instance in ipairs(self.sine_instances) do
      local decay = 1 - (instance.time/instance.duration)
      local wave = math.sin(instance.time*instance.frequency*2*math.pi)
      local offset = decay*instance.amplitude*wave
      ox = ox + offset*math.cos(instance.angle)
      oy = oy + offset*math.sin(instance.angle)
    end
  end

  -- Square instances contribution
  if self.square_instances then
    for _, instance in ipairs(self.square_instances) do
      local decay = 1 - (instance.time/instance.duration)
      local wave = math.sin(instance.time*instance.frequency*2*math.pi) > 0 and 1 or -1
      local offset = decay*instance.amplitude*wave
      ox = ox + offset*math.cos(instance.angle)
      oy = oy + offset*math.sin(instance.angle)
    end
  end

  return {x = ox, y = oy, rotation = r, zoom = z}
end

--[[
  Internal: updates shake effects each frame.

  Called automatically during early_update phase.
]]
function shake:early_update(dt)
  self.push_used = 0

  -- Update handcam time
  if self.handcam_enabled then
    self.handcam_time = self.handcam_time + dt
  end

  -- Decay trauma instances independently, remove when depleted (iterate backwards)
  for i = #self.trauma_instances, 1, -1 do
    local instance = self.trauma_instances[i]
    instance.value = instance.value - instance.decay*dt
    if instance.value <= 0 then
      table.remove(self.trauma_instances, i)
    end
  end

  if #self.trauma_instances > 0 then
    self.trauma_time = self.trauma_time + dt
  end

  -- Update shake instances
  if self.shake_instances then
    for i = #self.shake_instances, 1, -1 do
      local instance = self.shake_instances[i]
      instance.time = instance.time + dt

      -- Check if it's time to pick new random values
      local change_interval = 1/instance.frequency
      if instance.time - instance.last_change >= change_interval then
        instance.last_change = instance.time
        local decay = 1 - (instance.time/instance.duration)
        if decay > 0 then
          instance.current_x = decay*instance.amplitude*random_float(-1, 1)
          instance.current_y = decay*instance.amplitude*random_float(-1, 1)
        else
          instance.current_x = 0
          instance.current_y = 0
        end
      end

      -- Remove when done
      if instance.time >= instance.duration then
        table.remove(self.shake_instances, i)
      end
    end
  end

  -- Update sine instances
  if self.sine_instances then
    for i = #self.sine_instances, 1, -1 do
      local instance = self.sine_instances[i]
      instance.time = instance.time + dt
      if instance.time >= instance.duration then
        table.remove(self.sine_instances, i)
      end
    end
  end

  -- Update square instances
  if self.square_instances then
    for i = #self.square_instances, 1, -1 do
      local instance = self.square_instances[i]
      instance.time = instance.time + dt
      if instance.time >= instance.duration then
        table.remove(self.square_instances, i)
      end
    end
  end
end
