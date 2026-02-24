require('anchor.object')

--[[
  Animation class for sprite sheet animations.

  Animations play through frames of a spritesheet with configurable timing,
  loop modes, and per-frame callbacks.

  Usage:
    -- As object child (recommended)
    self:add(animation('anim', an.spritesheets.hit, 0.03, 'once', {
      [3] = function(self) print("frame 3") end,
      [0] = function(self) self:kill() end,
    }))

    -- Drawing
    layer:animation(self.anim, x, y, r, sx, sy)

  Properties:
    self.spritesheet - spritesheet reference
    self.frame       - current frame (1-indexed)
    self.delay       - frame delay (number or table of per-frame delays)
    self.loop_mode   - 'once', 'loop', or 'bounce'
    self.actions     - table of callbacks indexed by frame number (0 = completion)
    self.playing     - whether animation is playing
    self.dead        - true when 'once' animation completes
    self.direction   - play direction (1 forward, -1 reverse for bounce)
]]
animation = object:extend()

--[[
  Creates a new animation.

  Parameters:
    spritesheet_name - spritesheet name to look up in an.spritesheets
    delay            - seconds per frame (number) or per-frame delays (table)
    loop_mode        - 'once' (play once, set dead), 'loop' (repeat), 'bounce' (ping-pong)
    actions          - optional table of callbacks: {[frame]: function, [0]: on_complete}

  Callbacks receive the parent object as self when using fat arrow syntax.
]]
function animation:new(spritesheet_name, delay, loop_mode, actions)
  object.new(self, spritesheet_name)
  self.spritesheet = an.spritesheets[spritesheet_name]
  self.delay = delay or 0.1
  self.loop_mode = loop_mode or 'loop'
  self.actions = actions or {}
  self.frame = 1
  self.timer = 0
  self.direction = 1
  self.playing = true
  self.dead = false
  -- Fire action for frame 1 on creation if it exists
  self:_fire_action(1)
end

--[[
  Updates animation timing. Called automatically when added to an object.

  Parameters:
    dt - delta time in seconds
]]
function animation:update(dt)
  if not self.playing then return end
  if self.dead then return end

  self.timer = self.timer + dt
  local current_delay = self:_get_delay(self.frame)

  while self.timer >= current_delay and self.playing and not self.dead do
    self.timer = self.timer - current_delay
    self:_advance_frame()
    current_delay = self:_get_delay(self.frame)
  end
end

--[[
  Starts or resumes playback.
]]
function animation:play()
  self.playing = true
end

--[[
  Pauses playback without resetting.
]]
function animation:stop()
  self.playing = false
end

--[[
  Resets animation to initial state and starts playing.
]]
function animation:reset()
  self.frame = 1
  self.timer = 0
  self.direction = 1
  self.dead = false
  self.playing = true
  self:_fire_action(1)
end

--[[
  Jumps to a specific frame.

  Parameters:
    frame - frame number (1-indexed)
]]
function animation:set_frame(frame)
  self.frame = math.clamp(frame, 1, self.spritesheet.frames)
  self.timer = 0
  self:_fire_action(self.frame)
end

-- Internal: get delay for a frame
function animation:_get_delay(frame)
  if type(self.delay) == 'table' then
    return self.delay[frame] or self.delay[1] or 0.1
  else
    return self.delay
  end
end

-- Internal: fire action callback for a frame
function animation:_fire_action(frame)
  if self.actions[frame] then
    if self.parent then
      self.actions[frame](self.parent)
    else
      self.actions[frame](self)
    end
  end
end

-- Internal: advance to next frame based on loop mode
function animation:_advance_frame()
  local next_frame = self.frame + self.direction

  if self.loop_mode == 'once' then
    if next_frame > self.spritesheet.frames then
      self:kill()
      self.playing = false
      self:_fire_action(0)
      return
    end
    self.frame = next_frame
    self:_fire_action(self.frame)

  elseif self.loop_mode == 'loop' then
    if next_frame > self.spritesheet.frames then
      self.frame = 1
      self:_fire_action(0)
      self:_fire_action(self.frame)
    else
      self.frame = next_frame
      self:_fire_action(self.frame)
    end

  elseif self.loop_mode == 'bounce' then
    if next_frame > self.spritesheet.frames then
      self.direction = -1
      self.frame = self.spritesheet.frames - 1
      self:_fire_action(0)
      if self.frame >= 1 then
        self:_fire_action(self.frame)
      end
    elseif next_frame < 1 then
      self.direction = 1
      self.frame = 2
      self:_fire_action(0)
      if self.frame <= self.spritesheet.frames then
        self:_fire_action(self.frame)
      end
    else
      self.frame = next_frame
      self:_fire_action(self.frame)
    end
  end
end
