require('anchor.object')

--[[
  Collider class - wraps a Box2D physics body.

  A child object added to game objects. Dies when parent dies.
  Name defaults to 'collider' so parent accesses it as self.collider.

  Usage:
    self:add(collider('player', 'dynamic', 'circle', 16))
    self:add(collider('wall', 'static', 'box', 64, 32))

  Multiple colliders on one object:
    hitbox = collider('player_hitbox', 'dynamic', 'circle', 12)
    hitbox.name = 'hitbox'
    self:add(hitbox)
]]
-- Unique ID counter for colliders
collider_next_id = 1

collider = object:extend()

function collider:new(tag, body_type, shape_type, ...)
  object.new(self, 'collider')
  self.tag = tag
  self.body_type = body_type
  self.shape_type = shape_type
  self.body = physics_create_body(self.body_type, 0, 0)

  -- Add initial shape based on shape_type
  -- Last arg can be opts table (e.g. {sensor = true})
  local shape_args = {...}
  if self.shape_type == 'chain' then
    -- Chain: args are (vertices, is_loop) — no opts table
    self.chain = physics_add_chain(self.body, self.tag, shape_args[1], shape_args[2] or true)
  else
    local opts = {}
    if type(shape_args[#shape_args]) == 'table' then
      opts = table.remove(shape_args)
    end
    if self.shape_type == 'circle' then
      self.shape = physics_add_circle(self.body, self.tag, shape_args[1], opts)
    elseif self.shape_type == 'box' then
      self.shape = physics_add_box(self.body, self.tag, shape_args[1], shape_args[2], opts)
    elseif self.shape_type == 'capsule' then
      self.shape = physics_add_capsule(self.body, self.tag, shape_args[1], shape_args[2], opts)
    elseif self.shape_type == 'polygon' then
      self.shape = physics_add_polygon(self.body, self.tag, shape_args[1], opts)
    end
  end

  -- Register with unique ID (userdata can't be compared directly)
  self.id = collider_next_id
  collider_next_id = collider_next_id + 1
  physics_set_user_data(self.body, self.id)
  an.colliders[self.id] = self

  -- Position sync: physics -> parent each frame
  self:early_action('sync', function()
    self.parent.x, self.parent.y = physics_get_position(self.body)
  end)
end

function collider:destroy()
  an.colliders[self.id] = nil
  physics_destroy_body(self.body)
end

-- Position
function collider:get_position() return physics_get_position(self.body) end
function collider:set_position(x, y) physics_set_position(self.body, x, y) end
function collider:get_angle() return physics_get_angle(self.body) end
function collider:set_angle(angle) physics_set_angle(self.body, angle) end

-- Velocity
function collider:get_velocity() return physics_get_velocity(self.body) end
function collider:set_velocity(vx, vy) physics_set_velocity(self.body, vx, vy) end
function collider:get_angular_velocity() return physics_get_angular_velocity(self.body) end
function collider:set_angular_velocity(av) physics_set_angular_velocity(self.body, av) end

-- Forces & impulses
function collider:apply_force(fx, fy) physics_apply_force(self.body, fx, fy) end
function collider:apply_force_at(fx, fy, px, py) physics_apply_force_at(self.body, fx, fy, px, py) end
function collider:apply_impulse(ix, iy) physics_apply_impulse(self.body, ix, iy) end
function collider:apply_impulse_at(ix, iy, px, py) physics_apply_impulse_at(self.body, ix, iy, px, py) end
function collider:apply_torque(torque) physics_apply_torque(self.body, torque) end
function collider:apply_angular_impulse(impulse) physics_apply_angular_impulse(self.body, impulse) end

-- Body properties
function collider:set_linear_damping(damping) physics_set_linear_damping(self.body, damping) end
function collider:set_angular_damping(damping) physics_set_angular_damping(self.body, damping) end
function collider:set_gravity_scale(scale) physics_set_gravity_scale(self.body, scale) end
function collider:set_fixed_rotation(fixed) physics_set_fixed_rotation(self.body, fixed) end
function collider:set_bullet(bullet) physics_set_bullet(self.body, bullet) end

-- Shape properties (operate on self.shape by default, or pass explicit shape)
function collider:set_friction(friction, shape) physics_shape_set_friction(shape or self.shape, friction) end
function collider:get_friction(shape) return physics_shape_get_friction(shape or self.shape) end
function collider:set_restitution(restitution, shape) physics_shape_set_restitution(shape or self.shape, restitution) end
function collider:get_restitution(shape) return physics_shape_get_restitution(shape or self.shape) end
function collider:set_density(density, shape) physics_shape_set_density(shape or self.shape, density) end
function collider:get_density(shape) return physics_shape_get_density(shape or self.shape) end
function collider:destroy_shape(shape, update_mass)
  if update_mass == nil then update_mass = true end
  physics_shape_destroy(shape, update_mass)
end
function collider:set_filter_group(group, shape) physics_shape_set_filter_group(shape or self.shape, group) end

--[[
  Adds an additional circle shape to this body.

  Usage:
    shape = self.collider:add_circle('hitbox', 8, {offset_x = 10})

  Returns: shape handle
]]
function collider:add_circle(tag, radius, opts)
  return physics_add_circle(self.body, tag, radius, opts or {})
end

--[[
  Adds an additional box shape to this body.

  Usage:
    shape = self.collider:add_box('hitbox', 32, 16, {offset_x = 0, offset_y = -8})

  Returns: shape handle
]]
function collider:add_box(tag, width, height, opts)
  return physics_add_box(self.body, tag, width, height, opts or {})
end

--[[
  Adds an additional capsule shape to this body.

  Usage:
    shape = self.collider:add_capsule('hitbox', 24, 8)

  Returns: shape handle
]]
function collider:add_capsule(tag, length, radius, opts)
  return physics_add_capsule(self.body, tag, length, radius, opts or {})
end

--[[
  Adds an additional polygon shape to this body.

  Usage:
    verts = {-16, -16, 16, -16, 16, 16, -16, 16}
    shape = self.collider:add_polygon('hitbox', verts)

  Vertices are a flat array: {x1, y1, x2, y2, ...}
  Returns: shape handle
]]
function collider:add_polygon(tag, vertices, opts)
  return physics_add_polygon(self.body, tag, vertices, opts or {})
end

--[[
  Adds a chain shape to this body (for terrain/wall boundaries).

  Usage:
    verts = {x1, y1, x2, y2, ...}  -- at least 4 points
    chain = self.collider:add_chain('wall', verts, true)  -- closed loop

  Vertices are a flat array: {x1, y1, x2, y2, ...}
  is_loop: true for closed loop, false for open chain
  Returns: chain ID handle
]]
function collider:add_chain(tag, vertices, is_loop)
  return physics_add_chain(self.body, tag, vertices, is_loop)
end

--[[
  Returns the total mass of this body (sum of all shape densities * areas).
]]
function collider:get_mass() return physics_get_mass(self.body) end

--[[
  Sets the center of mass relative to the body origin (in pixels).
  This overrides the computed center of mass from shapes.

  Usage:
    self.collider:set_center_of_mass(0, 0)  -- center at body origin
]]
function collider:set_center_of_mass(x, y) physics_set_center_of_mass(self.body, x, y) end

--[[
  Returns the body type: 'static', 'kinematic', or 'dynamic'.
]]
function collider:get_body_type() return physics_get_body_type(self.body) end

--[[
  Returns true if the body is awake (actively simulating).
  Bodies sleep when they come to rest to save CPU.
]]
function collider:is_awake() return physics_is_awake(self.body) end

--[[
  Wakes up or puts the body to sleep.

  Usage:
    self.collider:set_awake(true)  -- wake up
]]
function collider:set_awake(awake) physics_set_awake(self.body, awake) end

--[[
  Returns a table of all shapes on this body with world-space geometry.
  Each entry has: type ("circle", "polygon", "capsule", "segment"), tag, sensor
  Circle: x, y, radius
  Polygon: vertices {x1,y1,x2,y2,...}, count, radius
  Capsule: x1, y1, x2, y2, radius
  Segment: x1, y1, x2, y2
]]
function collider:get_shapes_geometry() return physics_get_shapes_geometry(self.body) end

-- Steering behaviors
-- Each returns (fx, fy) force vectors that can be combined and applied.
-- Usage:
--   local sx, sy = self.collider:steering_seek(target_x, target_y, max_speed, max_force)
--   local wx, wy = self.collider:steering_wander(50, 50, 20, dt, max_speed, max_force)
--   local rx, ry = self.collider:steering_separate(16, enemies, max_speed, max_force)
--   self.collider:apply_force(math.limit(sx + wx + rx, sy + wy + ry, max_force))

-- Seek: steer toward target at max_speed
function collider:steering_seek(x, y, max_speed, max_force)
  local dx, dy = x - self.parent.x, y - self.parent.y
  dx, dy = math.normalize(dx, dy)
  dx, dy = dx*max_speed, dy*max_speed
  local vx, vy = self:get_velocity()
  dx, dy = dx - vx, dy - vy
  dx, dy = math.limit(dx, dy, max_force or 1000)
  return dx, dy
end

-- Flee: steer away from target at max_speed
function collider:steering_flee(x, y, max_speed, max_force)
  local dx, dy = self.parent.x - x, self.parent.y - y
  dx, dy = math.normalize(dx, dy)
  dx, dy = dx*max_speed, dy*max_speed
  local vx, vy = self:get_velocity()
  dx, dy = dx - vx, dy - vy
  dx, dy = math.limit(dx, dy, max_force or 1000)
  return dx, dy
end

-- Arrive: steer toward target, decelerate within radius rs
function collider:steering_arrive(x, y, rs, max_speed, max_force)
  local dx, dy = x - self.parent.x, y - self.parent.y
  local d = math.length(dx, dy)
  dx, dy = math.normalize(dx, dy)
  if d < rs then
    dx, dy = dx*math.remap(d, 0, rs, 0, max_speed), dy*math.remap(d, 0, rs, 0, max_speed)
  else
    dx, dy = dx*max_speed, dy*max_speed
  end
  local vx, vy = self:get_velocity()
  dx, dy = dx - vx, dy - vy
  dx, dy = math.limit(dx, dy, max_force or 1000)
  return dx, dy
end

-- Pursuit: seek predicted future position of a moving target
-- target must have .x, .y and .collider with :get_velocity()
function collider:steering_pursuit(target, max_speed, max_force)
  local tx, ty = target.x - self.parent.x, target.y - self.parent.y
  local d = math.length(tx, ty)
  local tvx, tvy = target.collider:get_velocity()
  local target_speed = math.length(tvx, tvy)
  local look_ahead = d/(max_speed + target_speed + 0.001)
  return self:steering_seek(target.x + tvx*look_ahead, target.y + tvy*look_ahead, max_speed, max_force)
end

-- Evade: flee predicted future position of a pursuer
-- pursuer must have .x, .y and .collider with :get_velocity()
function collider:steering_evade(pursuer, max_speed, max_force)
  local tx, ty = pursuer.x - self.parent.x, pursuer.y - self.parent.y
  local d = math.length(tx, ty)
  local pvx, pvy = pursuer.collider:get_velocity()
  local pursuer_speed = math.length(pvx, pvy)
  local look_ahead = d/(max_speed + pursuer_speed + 0.001)
  return self:steering_flee(pursuer.x + pvx*look_ahead, pursuer.y + pvy*look_ahead, max_speed, max_force)
end

-- Wander: random jittery movement (jitter is in radians/second, scaled by dt)
-- wander_r is relative to heading so the target stays roughly in front
function collider:steering_wander(d, rs, jitter, dt, max_speed, max_force)
  local vx, vy = self:get_velocity()
  local nx, ny = math.normalize(vx, vy)
  local cx, cy = self.parent.x + nx*d, self.parent.y + ny*d
  if not self.wander_r then self.wander_r = 0 end
  self.wander_r = self.wander_r + an.random:float(-jitter*dt, jitter*dt)
  local heading_r = math.atan(ny, nx)
  local tx, ty = cx + rs*math.cos(heading_r + self.wander_r), cy + rs*math.sin(heading_r + self.wander_r)
  return self:steering_seek(tx, ty, max_speed, max_force)
end

-- Separate: push away from nearby others
function collider:steering_separate(rs, others, max_speed, max_force, spatial_hash)
  local dx, dy, n = 0, 0, 0
  local px, py = self.parent.x, self.parent.y
  local pid = self.parent.id
  if spatial_hash then
    local cell_size = spatial_hash.cell_size
    local cells = spatial_hash.cells
    local cx0 = math.floor((px - rs)/cell_size)
    local cy0 = math.floor((py - rs)/cell_size)
    local cx1 = math.floor((px + rs)/cell_size)
    local cy1 = math.floor((py + rs)/cell_size)
    for cx = cx0, cx1 do
      for cy = cy0, cy1 do
        local key = cx*73856093 + cy*19349663
        local cell = cells[key]
        if cell then
          for i = 1, #cell do
            local object = cell[i]
            if object.id ~= pid and math.distance(object.x, object.y, px, py) < rs then
              local tx, ty = px - object.x, py - object.y
              local nx, ny = math.normalize(tx, ty)
              local l = math.length(nx, ny)
              dx = dx + rs*(nx/l)
              dy = dy + rs*(ny/l)
              n = n + 1
            end
          end
        end
      end
    end
  else
    for _, object in ipairs(others) do
      if object.id ~= pid and math.distance(object.x, object.y, px, py) < rs then
        local tx, ty = px - object.x, py - object.y
        local nx, ny = math.normalize(tx, ty)
        local l = math.length(nx, ny)
        dx = dx + rs*(nx/l)
        dy = dy + rs*(ny/l)
        n = n + 1
      end
    end
  end
  if n > 0 then dx, dy = dx/n, dy/n end
  if math.length(dx, dy) > 0 then
    dx, dy = math.normalize(dx, dy)
    dx, dy = dx*max_speed, dy*max_speed
    local vx, vy = self:get_velocity()
    dx, dy = dx - vx, dy - vy
    dx, dy = math.limit(dx, dy, max_force or 1000)
  end
  return dx, dy
end

-- Align: match velocity direction with nearby others
function collider:steering_align(rs, others, max_speed, max_force)
  local dx, dy, n = 0, 0, 0
  for _, object in ipairs(others) do
    if object.id ~= self.parent.id and math.distance(object.x, object.y, self.parent.x, self.parent.y) < rs then
      local vx, vy = object.collider:get_velocity()
      dx, dy = dx + vx, dy + vy
      n = n + 1
    end
  end
  if n > 0 then dx, dy = dx/n, dy/n end
  if math.length(dx, dy) > 0 then
    dx, dy = math.normalize(dx, dy)
    dx, dy = dx*max_speed, dy*max_speed
    local vx, vy = self:get_velocity()
    dx, dy = dx - vx, dy - vy
    dx, dy = math.limit(dx, dy, max_force or 1000)
    return dx, dy
  else
    return 0, 0
  end
end

-- Cohesion: steer toward center of nearby others
function collider:steering_cohesion(rs, others, max_speed, max_force)
  local dx, dy, n = 0, 0, 0
  for _, object in ipairs(others) do
    if object.id ~= self.parent.id and math.distance(object.x, object.y, self.parent.x, self.parent.y) < rs then
      dx, dy = dx + object.x, dy + object.y
      n = n + 1
    end
  end
  if n > 0 then
    dx, dy = dx/n, dy/n
    return self:steering_seek(dx, dy, max_speed, max_force)
  else
    return 0, 0
  end
end
