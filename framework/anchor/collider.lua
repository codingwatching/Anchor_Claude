

















collider_next_id = 1;do

local _class_0;local _parent_0 = object;local _base_0 = { destroy = function(self)































an.colliders[self.id] = nil;return 
physics_destroy_body(self.body)end, get_position = function(self)return 


physics_get_position(self.body)end, set_position = function(self, x, y)return 
physics_set_position(self.body, x, y)end, get_angle = function(self)return 
physics_get_angle(self.body)end, set_angle = function(self, angle)return 
physics_set_angle(self.body, angle)end, get_velocity = function(self)return 


physics_get_velocity(self.body)end, set_velocity = function(self, vx, vy)return 
physics_set_velocity(self.body, vx, vy)end, get_angular_velocity = function(self)return 
physics_get_angular_velocity(self.body)end, set_angular_velocity = function(self, av)return 
physics_set_angular_velocity(self.body, av)end, apply_force = function(self, fx, fy)return 


physics_apply_force(self.body, fx, fy)end, apply_force_at = function(self, fx, fy, px, py)return 
physics_apply_force_at(self.body, fx, fy, px, py)end, apply_impulse = function(self, ix, iy)return 
physics_apply_impulse(self.body, ix, iy)end, apply_impulse_at = function(self, ix, iy, px, py)return 
physics_apply_impulse_at(self.body, ix, iy, px, py)end, apply_torque = function(self, torque)return 
physics_apply_torque(self.body, torque)end, apply_angular_impulse = function(self, impulse)return 
physics_apply_angular_impulse(self.body, impulse)end, set_linear_damping = function(self, damping)return 


physics_set_linear_damping(self.body, damping)end, set_angular_damping = function(self, damping)return 
physics_set_angular_damping(self.body, damping)end, set_gravity_scale = function(self, scale)return 
physics_set_gravity_scale(self.body, scale)end, set_fixed_rotation = function(self, fixed)return 
physics_set_fixed_rotation(self.body, fixed)end, set_bullet = function(self, bullet)return 
physics_set_bullet(self.body, bullet)end, set_friction = function(self, friction, shape)if 


shape == nil then shape = self.shape end;return physics_shape_set_friction(shape, friction)end, get_friction = function(self, shape)if 
shape == nil then shape = self.shape end;return physics_shape_get_friction(shape)end, set_restitution = function(self, restitution, shape)if 
shape == nil then shape = self.shape end;return physics_shape_set_restitution(shape, restitution)end, get_restitution = function(self, shape)if 
shape == nil then shape = self.shape end;return physics_shape_get_restitution(shape)end, set_density = function(self, density, shape)if 
shape == nil then shape = self.shape end;return physics_shape_set_density(shape, density)end, get_density = function(self, shape)if 
shape == nil then shape = self.shape end;return physics_shape_get_density(shape)end, add_circle = function(self, tag, radius, opts)if 









opts == nil then opts = {  }end;return 
physics_add_circle(self.body, tag, radius, opts)end, add_box = function(self, tag, width, height, opts)if 









opts == nil then opts = {  }end;return 
physics_add_box(self.body, tag, width, height, opts)end, add_capsule = function(self, tag, length, radius, opts)if 









opts == nil then opts = {  }end;return 
physics_add_capsule(self.body, tag, length, radius, opts)end, add_polygon = function(self, tag, vertices, opts)if 











opts == nil then opts = {  }end;return 
physics_add_polygon(self.body, tag, vertices, opts)end, get_mass = function(self)return 




physics_get_mass(self.body)end, set_center_of_mass = function(self, x, y)return 








physics_set_center_of_mass(self.body, x, y)end, get_body_type = function(self)return 




physics_get_body_type(self.body)end, is_awake = function(self)return 





physics_is_awake(self.body)end, set_awake = function(self, awake)return 







physics_set_awake(self.body, awake)end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, tag, body_type, shape_type, ...)self.tag = tag;self.body_type = body_type;self.shape_type = shape_type;_class_0.__parent.__init(self, 'collider')self.body = physics_create_body(self.body_type, 0, 0)local shape_args = { ... }local opts = {  }if type(shape_args[#shape_args]) == 'table' then opts = table.remove(shape_args)end;do local _exp_0 = self.shape_type;if 'circle' == _exp_0 then self.shape = physics_add_circle(self.body, self.tag, shape_args[1], opts)elseif 'box' == _exp_0 then self.shape = physics_add_box(self.body, self.tag, shape_args[1], shape_args[2], opts)elseif 'capsule' == _exp_0 then self.shape = physics_add_capsule(self.body, self.tag, shape_args[1], shape_args[2], opts)elseif 'polygon' == _exp_0 then self.shape = physics_add_polygon(self.body, self.tag, shape_args[1], opts)end end;self.id = collider_next_id;collider_next_id = collider_next_id + 1;physics_set_user_data(self.body, self.id)an.colliders[self.id] = self;return self:early_action('sync', function(self)self.parent.x, self.parent.y = physics_get_position(self.body)end)end, __base = _base_0, __name = "collider", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;collider = _class_0;return _class_0 end