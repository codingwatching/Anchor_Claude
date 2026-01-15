

require('anchor')


bg = an:layer('bg')
shadow = an:layer('shadow')
outline = an:layer('outline')
game = an:layer('game')
ui = an:layer('ui')


an:font('main', 'assets/LanaPixel.ttf', 11)
an:image('ball', 'assets/slight_smile.png')


shadow_shader = shader_load_file('shaders/shadow.frag')
outline_shader = shader_load_file('shaders/outline.frag')


W, H = 480, 270


an:physics_init()
an:physics_set_gravity(0, 500)
an:physics_set_meter_scale(64)


an:physics_tag('ball')
an:physics_tag('wall')
an:physics_tag('impulse_block')
an:physics_tag('slowing_zone')
an:physics_collision('ball', 'wall')
an:physics_collision('ball', 'ball')
an:physics_collision('ball', 'impulse_block')
an:physics_sensor('ball', 'slowing_zone')
an:physics_hit('ball', 'wall')


bg_color = rgba(231, 232, 233, 255)
green = rgba(122, 179, 87, 255)
blue = rgba(85, 172, 238, 255)
blue_transparent = rgba(85, 172, 238, 100)
yellow = rgba(255, 204, 77, 255)
red = rgba(221, 46, 68, 255)
orange = rgba(244, 144, 12, 255)
purple = rgba(170, 142, 214, 255)
black = rgba(0, 0, 0, 255)
white = rgba(255, 255, 255, 255)


ground_width = W * 0.9
ground_height = 12
ground_x = (W - ground_width) / 2
ground_y = H - 20 - ground_height / 2

wall_width = 12
wall_top = H * 0.1
wall_height = ground_y - wall_top
left_wall_x = ground_x
right_wall_x = ground_x + ground_width - wall_width;do


local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self, layer)if 








self.rounded_top then local radius = 
self.w / 2
layer:circle(self.x, self.y - self.h / 2 + radius, radius, self.color)return 
layer:rectangle(self.x - self.w / 2, self.y - self.h / 2 + radius, self.w, self.h - radius, self.color)elseif 
self.rounded_left then local radius = 
self.h / 2
layer:circle(self.x - self.w / 2 + radius, self.y, radius, self.color)return 
layer:rectangle(self.x - self.w / 2 + radius, self.y - self.h / 2, self.w - radius, self.h, self.color)else return 

layer:rectangle(self.x - self.w / 2, self.y - self.h / 2, self.w, self.h, self.color)end end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y, w, h, color, rounded_top, rounded_left)if color == nil then color = green end;if rounded_top == nil then rounded_top = false end;if rounded_left == nil then rounded_left = false end;self.w = w;self.h = h;self.color = color;self.rounded_top = rounded_top;self.rounded_left = rounded_left;_class_0.__parent.__init(self)self:tag('drawable')self:add(collider('wall', 'static', 'box', self.w, self.h))self.collider:set_position(x, y)return self.collider:set_friction(1)end, __base = _base_0, __name = "wall", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;wall = _class_0 end


an:add(wall(ground_x + ground_width / 2, ground_y + ground_height / 2, ground_width, ground_height))
an:add(wall(left_wall_x + wall_width / 2, wall_top + wall_height / 2, wall_width, wall_height, green, true))
an:add(wall(right_wall_x + wall_width / 2, wall_top + wall_height / 2, wall_width, wall_height))


ceiling_width = ground_width / 2
ceiling_height = ground_height
ceiling_x = right_wall_x + wall_width - ceiling_width / 2
ceiling_y = wall_top + ceiling_height / 2
an:add(wall(ceiling_x, ceiling_y, ceiling_width, ceiling_height, green, false, true))


impulse_width = ground_width * 0.1
impulse_height = ground_height
impulse_x = left_wall_x + wall_width
impulse_y = ground_y - impulse_height;do

local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self, layer)return 









layer:rectangle(self.x - self.w / 2, self.y - self.h / 2, self.w, self.h, blue)end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y, w, h)self.w = w;self.h = h;_class_0.__parent.__init(self)self:tag('drawable')self:add(collider('impulse_block', 'static', 'box', self.w, self.h))self.collider:set_position(x + self.w / 2, y + self.h / 2)self.collider:set_friction(1)return self.collider:set_restitution(1)end, __base = _base_0, __name = "impulse_block", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;impulse_block = _class_0 end

an:add(impulse_block(impulse_x, impulse_y, impulse_width, impulse_height))


box_interior_width = ground_width - 2 * wall_width
box_interior_height = wall_height
slowing_zone_width = box_interior_width / 6
slowing_zone_height = box_interior_height / 3
ceiling_left_edge = ceiling_x - ceiling_width / 2
slowing_zone_x = ceiling_left_edge
slowing_zone_y = wall_top + ceiling_height + slowing_zone_height / 2;do

local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self)return 







game:rectangle(self.x - self.w / 2, self.y - self.h / 2, self.w, self.h, blue_transparent)end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y, w, h)self.w = w;self.h = h;_class_0.__parent.__init(self)self:tag('slowing_zone')self:add(collider('slowing_zone', 'static', 'box', self.w, self.h, { sensor = true }))return self.collider:set_position(x, y)end, __base = _base_0, __name = "slowing_zone", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;slowing_zone = _class_0 end

an:add(slowing_zone(slowing_zone_x, slowing_zone_y, slowing_zone_width, slowing_zone_height))


ball_radius = 10
ball_scale = ball_radius * 2 / an.images.ball.width;do

local _class_0;local _parent_0 = object;local _base_0 = { draw = function(self, layer)local angle = 














self.collider:get_angle()
layer:push(self.x, self.y, angle, ball_scale, ball_scale)
layer:image(an.images.ball, 0, 0, nil, self.flash and white or nil)return 
layer:pop()end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, x, y)self.x = x;self.y = y;_class_0.__parent.__init(self)self:tag('ball')self:tag('drawable')self.impulsed = false;self.original_speed = 0;self.flash = false;self:add(timer())self:add(collider('ball', 'dynamic', 'circle', ball_radius))self.collider:set_position(self.x, self.y)self.collider:set_restitution(1)return self.collider:set_friction(1)end, __base = _base_0, __name = "ball", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;ball = _class_0 end


an:action(function(self)if 
key_is_pressed('k') then local spawn_x = 
left_wall_x + wall_width + ball_radius + 20;local spawn_y = 
wall_top - ball_radius - 5
an:add(ball(spawn_x, spawn_y))end;if 

key_is_pressed('p') then local _list_0 = 
an:all('ball')for _index_0 = 1, #_list_0 do local b = _list_0[_index_0]
print("Applying impulse to ball")
b.collider:apply_impulse(200, 0)end end end)


an:early_action('handle_collisions', function(self)local _list_0 = 
an:collision_begin_events('ball', 'impulse_block')for _index_0 = 1, #_list_0 do local event = _list_0[_index_0]local ball = 
event.a;if not 
ball.impulsed then
ball.impulsed = true
ball.collider:apply_impulse(random_float(20, 40), 0)end end;local _list_1 = 

an:sensor_begin_events('ball', 'slowing_zone')for _index_0 = 1, #_list_1 do local event = _list_1[_index_0]local ball = 
event.a;local vx,vy = 
ball.collider:get_velocity()
ball.original_speed = math.sqrt(vx * vx + vy * vy)
ball.collider:set_velocity(vx * 0.1, vy * 0.1)
ball.collider:set_gravity_scale(0.1)end;local _list_2 = 

an:sensor_end_events('ball', 'slowing_zone')for _index_0 = 1, #_list_2 do local event = _list_2[_index_0]local ball = 
event.a;local vx,vy = 
ball.collider:get_velocity()local current_speed = 
math.sqrt(vx * vx + vy * vy)if 
current_speed > 0 then local scale = 
ball.original_speed / current_speed
ball.collider:set_velocity(vx * scale, vy * scale)end
ball.collider:set_gravity_scale(1)end;local _list_3 = 

an:hit_events('ball', 'wall')for _index_0 = 1, #_list_3 do local event = _list_3[_index_0]local ball = 
event.a;if 
event.approach_speed > 300 then
ball.flash = true
ball.timer:after(0.15, 'flash', function()ball.flash = false end)end end end)return 


an:late_action('draw', function(self)

bg:rectangle(0, 0, W, H, bg_color)local _list_0 = 


an:all('slowing_zone')for _index_0 = 1, #_list_0 do local zone = _list_0[_index_0]
zone:draw()end;local _list_1 = 


an:all('drawable')for _index_0 = 1, #_list_1 do local obj = _list_1[_index_0]
obj:draw(shadow)
obj:draw(outline)
obj:draw(game)end


layer_apply_shader(shadow.handle, shadow_shader)
layer_shader_set_vec2(outline.handle, outline_shader, "u_pixel_size", 1 / W, 1 / H)
layer_apply_shader(outline.handle, outline_shader)


bg:draw()
shadow:draw(4, 4)
outline:draw()
game:draw()return 
ui:draw()end)