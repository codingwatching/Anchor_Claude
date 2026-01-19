-- handcam.lua
-- A reusable camera effect for Anchor
-- Require this file after anchor is initialized to add the effect

-- Create the handcam object and add to 'an'
return function(drift_amount, drift_speed, rotation_amount, zoom_amount)
  an:add(object('handcam'):build(function(self)
    --[[
      HANDCAM CAMERA SYSTEM
      This implements a realistic handheld camera effect using smooth perlin noise and a trauma-based intensity system for dynamic camera reactions.

      The system creates organic camera movement through:
      1. Continuous smooth drift using perlin noise
      2. Force-based positional offsets with springy return-to-center
      3. Trauma system for dramatic camera shake moments
      4. Compositional transform system for multiple effects

      Usage:
        require('handcam')(5, 0.4, 0.05, 0.03) -- Customized parameters
        or
        require('handcam')() -- Default parameters

      To add custom transform sources:
        an.handcam:set_transform_source('my_effect', {
          offset_x = 10,  -- Optional
          offset_y = 5,   -- Optional
          rotation = 0.1, -- Optional
          zoom = 0.05     -- Optional
        })

      To remove a transform source:
        an.handcam:set_transform_source('my_effect', nil)
    ]]--

    -- Camera parameters with defaults if not provided
    self.base_drift_amount = drift_amount or 5
    self.drift_speed = drift_speed or 0.4
    self.rotation_amount = rotation_amount or 0.01
    self.zoom_amount = zoom_amount or 0.01

    -- Create transform sources table for compositional effects
    self.transform_sources = {
      -- Default transformations from the handcam itself
      drift = {
        offset_x = 0,
        offset_y = 0,
        rotation = 0,
        zoom = 0
      },
      -- Force-based transformations
      force = {
        offset_x = 0,
        offset_y = 0,
        rotation = 0,
        zoom = 0
      }
      -- Other sources can be added dynamically
    }

    -- Perlin noise sampling parameters
    self.base_time = 0                            -- Master time value that increments each frame
    self.time_x_offset, self.time_y_offset = 0, 0 -- Offset values allow changing perlin noise pattern without creating jitter
    self.time_rotation_offset = 0
    self.time_zoom_offset = 0

    -- Force systme parameters
    self.force_x = 0                              -- Current horizontal force
    self.force_y = 0                              -- Current vertical force
    self.force_damping = 0.5                      -- How quickly forces decay (higher = faster)
    self.force_return_speed = 1.5                 -- How quickly the camera returns to the center after a push (higher = faster)

    -- Force application variables
    self.offset_x, self.offset_y = 0, 0           -- Force-based position offsets

    -- Trauma system for shake intensity
    self.trauma = 0                               -- Current trauma level [0, 1]
    self.trauma_decay = 0.9                       -- How quickly trauma decays (higher = faster)

    self:timer()

  end):action(function(self, dt)
    -- Increment the base time value that drives perlin noise
    self.base_time = self.base_time + self.drift_speed*dt

    -- Calculate current time values by combining base time with offsets
    -- Each dimension uses slightly different multipliers to create variation
    local time_x = 1.0*self.base_time + self.time_x_offset
    local time_y = 1.3*self.base_time + self.time_y_offset
    local time_rotation = 0.7*self.base_time + self.time_rotation_offset
    local time_zoom = 0.5*self.base_time + self.time_zoom_offset

    -- Decay trauma over time
    self.trauma = math.max(0, self.trauma - self.trauma_decay*dt)

    -- Square trauma for non-linear falloff (more natural feeling)
    local trauma_squared = self.trauma*self.trauma

    -- Scale effect intensities based on trauma
    local current_drift = self.base_drift_amount*(1 + 3*trauma_squared)
    local current_rotation = self.rotation_amount*(1 + 4*trauma_squared)
    local current_zoom = self.zoom_amount*(1 + 2*trauma_squared)

    -- Generate smooth motion using perlin noise
    -- math.perlin_noise returns [0, 1], so we transform to [-1, 1] with (value*2-1)
    -- Store drift transformations in the source table
    self.transform_sources.drift = {
      offset_x = current_drift*(math.perlin_noise(time_x)*2 - 1),
      offset_y = current_drift*(math.perlin_noise(time_y)*2 - 1),
      rotation = current_rotation*(math.perlin_noise(time_rotation)*2 - 1),
      zoom = current_zoom*(math.perlin_noise(time_zoom)*2 - 1)
    }

    -- Update force-based position offsets
    self.offset_x = self.offset_x + self.force_x*dt
    self.offset_y = self.offset_y + self.force_y*dt

    -- Dampen forces over time (makes them decrease gradually)
    self.force_x = math.damping(0.9, self.force_damping, dt, self.force_x)
    self.force_y = math.damping(0.9, self.force_damping, dt, self.force_y)

    -- Return offsets towards center with spring-like movement
    self.offset_x = math.damping(0.8, self.force_return_speed, dt, self.offset_x)
    self.offset_y = math.damping(0.8, self.force_return_speed, dt, self.offset_y)

    -- Store force transformations
    self.transform_sources.force = {
      offset_x = self.offset_x,
      offset_y = self.offset_y,
      rotation = 0, -- Force doesn't affect rotation by default
      zoom = 0,     -- Force doesn't affect zoom by default
    }

    -- Apply final camera transformations by composing all sources
    local final_offset_x = 0
    local final_offset_y = 0
    local final_rotation = 0
    local final_zoom = 0

    -- Sum all transformation sources
    for source_name, transform in pairs(self.transform_sources) do
      final_offset_x = final_offset_x + (transform.offset_x or 0)
      final_offset_y = final_offset_y + (transform.offset_y or 0)
      final_rotation = final_rotation + (transform.rotation or 0)
      final_zoom = final_zoom + (transform.zoom or 0)
    end

    -- Apply combined transformations to the global camera
    an.camera_x = an.w/2 + final_offset_x -- assumes an.w/2, an.h/2 to be the camera's center, in games where the camera moves through the game world this needs to reflect that
    an.camera_y = an.h/2 + final_offset_y
    an.camera_r = final_rotation
    an.camera_sx = 1 + final_zoom
    an.camera_sy = 1 + final_zoom

  end):method('camera_push', function(self, force, angle, damping, return_speed)
    --[[
      Applies a directional force to the camera

      Parameters:
        force: Strength of the push
        angle: Direction of the push in radians (using +math.pi to reverse direction)
        damping: How quickly the force decays (optional, higher = faster decay)
        return_speed: How quickly the camera returns to center (optional, higher = faster return)
    --]]
    self.force_x = self.force_x + force*math.cos(angle + math.pi)
    self.force_y = self.force_y + force*math.sin(angle + math.pi)
    if damping then self.force_damping = damping end
    if return_speed then self.force_return_speed = return_speed end

  end):method('dramatic_camera_push', function(self, force, angle, damping, return_speed)
    --[[
      Creates a dramatic camera push with increased trauma

      This function:
      1. Applies a directional force
      2. Increases trauma based on force strength
      3. Slightly shifts perlin noise sampling position for variety without creating jitter

      The shift in perlin sampling (time offsets) creates a smooth change in camera behavior without causing abrupt transitions.
    --]]
    self:camera_push(force, angle, damping, return_speed)
    self.trauma = math.min(1.0, self.trauma + math.remap(force, 0, 100, 0, 1))

    -- These small additions change which part of the perlin noise we sample without causing jittery transitions - key to smooth camera bumps
    self.time_x_offset = self.time_x_offset + 0.1
    self.time_y_offset = self.time_y_offset + 0.13

  end):method('set_transform_source', function(self, source_name, transform)
    --[[
      Creates or updates a transformation source

      Parameters:
        source_name: Unique identifier for this transform source
        transform: Table with any of these properties:
          - offset_x: horizontal camera offset
          - offset_y: vertical camera offset
          - rotation: camera rotation in radians
          - zoom: camera zoom factor

        Settings transform to nil removes the source completely
    --]]
    -- Create or update the source, if transform is nil then it will also be nilled
    self.transform_sources[source_name] = transform
  end))
end
