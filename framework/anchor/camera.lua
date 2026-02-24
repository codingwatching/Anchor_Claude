require('anchor.class')

--[[
  Camera child object for viewport control.

  Usage:
    an:add(camera(480, 270))
    an.camera.x = 100
    an.camera.y = 200
    an.camera.zoom = 2

  Camera is added as a child object, typically to 'an'.
  Layers automatically use an.camera by default (configurable via layer.camera).

  Camera methods:
    attach       - Apply camera transform to a layer (called automatically)
    detach       - Remove camera transform from a layer (called automatically)
    to_world     - Convert screen coordinates to world coordinates
    to_screen    - Convert world coordinates to screen coordinates
    get_effects  - Collect transform effects from child objects

  Properties:
    x, y         - Camera center position in world coordinates
    w, h         - Viewport dimensions
    rotation     - Rotation in radians
    zoom         - Zoom level (1 = normal, 2 = 2x magnification)
    mouse        - Table with x, y of mouse in world coordinates
]]
camera = object:extend()

--[[
  Creates a new camera with the given viewport dimensions.

  Usage:
    an:add(camera(480, 270))

  Parameters:
    w - Viewport width (default W)
    h - Viewport height (default H)

  Behavior:
    - Camera starts centered at (w/2, h/2)
    - Zoom defaults to 1, rotation to 0
    - Mouse position in world coords updated each frame
]]
function camera:new(w, h)
  object.new(self, 'camera')
  self.w = w or W
  self.h = h or H
  self.x = self.w/2
  self.y = self.h/2
  self.rotation = 0
  self.zoom = 1
  self.mouse = {x = 0, y = 0}
  self.follow_target = nil
  self.follow_lerp = 0.9
  self.follow_lerp_time = 0.5
  self.follow_lead = 0
  self.bounds = nil
end

--[[
  Collects transform effects from children that implement get_transform.

  Usage:
    effects = camera:get_effects()

  Returns: {x, y, rotation, zoom} - summed offsets from all child effects

  Behavior:
    - Iterates through children looking for get_transform method
    - Child effects (shake, handcam, etc.) return {x, y, rotation, zoom}
    - All effects are summed together
]]
function camera:get_effects()
  local ox, oy, r, z = 0, 0, 0, 0
  for _, child in ipairs(self.children) do
    if child.get_transform then
      local t = child:get_transform()
      ox = ox + (t.x or 0)
      oy = oy + (t.y or 0)
      r = r + (t.rotation or 0)
      z = z + (t.zoom or 0)
    end
  end
  return {x = ox, y = oy, rotation = r, zoom = z}
end

--[[
  Converts screen coordinates to world coordinates.

  Usage:
    world_x, world_y = camera:to_world(screen_x, screen_y)
    world_x, world_y = camera:to_world(mouse_position())

  Parameters:
    sx, sy - Screen coordinates

  Returns: world_x, world_y

  Behavior:
    - Accounts for camera position, zoom, rotation
    - Accounts for effects from child objects (shake, etc.)
]]
function camera:to_world(sx, sy)
  local effects = self:get_effects()
  local cx = self.x + effects.x
  local cy = self.y + effects.y
  local rot = self.rotation + effects.rotation
  local zoom = self.zoom*(1 + effects.zoom)

  local x = sx - self.w/2
  local y = sy - self.h/2
  x = x/zoom
  y = y/zoom
  local cos_r = math.cos(-rot)
  local sin_r = math.sin(-rot)
  local rx = x*cos_r - y*sin_r
  local ry = x*sin_r + y*cos_r
  return rx + cx, ry + cy
end

--[[
  Converts world coordinates to screen coordinates.

  Usage:
    screen_x, screen_y = camera:to_screen(world_x, world_y)

  Parameters:
    wx, wy - World coordinates

  Returns: screen_x, screen_y

  Behavior:
    - Accounts for camera position, zoom, rotation
    - Accounts for effects from child objects (shake, etc.)
]]
function camera:to_screen(wx, wy)
  local effects = self:get_effects()
  local cx = self.x + effects.x
  local cy = self.y + effects.y
  local rot = self.rotation + effects.rotation
  local zoom = self.zoom*(1 + effects.zoom)

  local x = wx - cx
  local y = wy - cy
  local cos_r = math.cos(rot)
  local sin_r = math.sin(rot)
  local rx = x*cos_r - y*sin_r
  local ry = x*sin_r + y*cos_r
  return rx*zoom + self.w/2, ry*zoom + self.h/2
end

--[[
  Applies camera transform to a layer.

  Usage:
    camera:attach(layer)
    camera:attach(layer, 0.5, 0.5)  -- parallax

  Parameters:
    layer      - Layer to apply transform to
    parallax_x - Horizontal parallax multiplier (default 1)
    parallax_y - Vertical parallax multiplier (default 1)

  Behavior:
    - Pushes two transforms onto layer's stack
    - Parallax < 1 makes layer scroll slower (background effect)
    - Parallax = 0 makes layer stationary (fixed background)
    - Called automatically by update loop for layers with camera set
]]
function camera:attach(layer, parallax_x, parallax_y)
  parallax_x = parallax_x or 1
  parallax_y = parallax_y or 1
  local effects = self:get_effects()
  local cx = self.x*parallax_x + effects.x
  local cy = self.y*parallax_y + effects.y
  local rot = self.rotation + effects.rotation
  local zoom = self.zoom*(1 + effects.zoom)

  layer:push(self.w/2, self.h/2, rot, zoom, zoom)
  layer:push(-cx, -cy, 0, 1, 1)
end

--[[
  Removes camera transform from a layer.

  Usage:
    camera:detach(layer)

  Parameters:
    layer - Layer to remove transform from

  Behavior:
    - Pops two transforms from layer's stack
    - Called automatically by update loop after all updates complete
]]
function camera:detach(layer)
  layer:pop()
  layer:pop()
end

--[[
  Sets the target for the camera to follow.

  Usage:
    camera:follow(player)
    camera:follow(player, 0.9, 0.3)        -- cover 90% of distance in 0.3 seconds
    camera:follow(player, 0.9, 0.5, 0.1)   -- with lead

  Parameters:
    target    - Object with x, y properties (and optionally collider for velocity)
    lerp      - Percentage of distance to cover (default: 0.9 = 90%)
    lerp_time - Time in seconds to cover that percentage (default: 0.5)
    lead      - Lead multiplier (default: 0, how far ahead to look based on velocity)

  Behavior:
    - Camera lerps towards target position each frame
    - If lead > 0 and target has a collider, camera looks ahead in movement direction
    - Pass nil to stop following
]]
function camera:follow(target, lerp, lerp_time, lead)
  self.follow_target = target
  if lerp then self.follow_lerp = lerp end
  if lerp_time then self.follow_lerp_time = lerp_time end
  if lead then self.follow_lead = lead end
end

--[[
  Sets the camera bounds.

  Usage:
    camera:set_bounds(-50, W + 50, -50, H + 50)

  Parameters:
    min_x, max_x - Horizontal limits for camera center
    min_y, max_y - Vertical limits for camera center

  Behavior:
    - Camera position is clamped to these bounds after following
    - Pass nil to remove bounds: camera:set_bounds()
]]
function camera:set_bounds(min_x, max_x, min_y, max_y)
  if min_x then
    self.bounds = {min_x = min_x, max_x = max_x, min_y = min_y, max_y = max_y}
  else
    self.bounds = nil
  end
end

--[[
  Internal: updates follow, bounds, and mouse world position each frame.

  Called automatically during early_update phase.
]]
function camera:early_update(dt)
  -- Follow target
  if self.follow_target and not self.follow_target.dead then
    local target_x = self.follow_target.x
    local target_y = self.follow_target.y
    -- Add lead based on velocity
    if self.follow_lead > 0 and self.follow_target.collider then
      local vx, vy = self.follow_target.collider:get_velocity()
      target_x = target_x + vx*self.follow_lead
      target_y = target_y + vy*self.follow_lead
    end
    self.x = math.lerp_dt(self.follow_lerp, self.follow_lerp_time, dt, self.x, target_x)
    self.y = math.lerp_dt(self.follow_lerp, self.follow_lerp_time, dt, self.y, target_y)
  end

  -- Apply bounds
  if self.bounds then
    local half_w = self.w/(2*self.zoom)
    local half_h = self.h/(2*self.zoom)
    self.x = math.clamp(self.x, self.bounds.min_x + half_w, self.bounds.max_x - half_w)
    self.y = math.clamp(self.y, self.bounds.min_y + half_h, self.bounds.max_y - half_h)
  end

  -- Update mouse world position
  local mx, my = mouse_position()
  self.mouse.x, self.mouse.y = self:to_world(mx, my)
end
