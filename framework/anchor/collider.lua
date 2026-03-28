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
