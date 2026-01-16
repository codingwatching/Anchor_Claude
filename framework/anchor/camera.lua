do



























local _class_0;local _parent_0 = object;local _base_0 = { get_effects = function(self)local ox,oy,rotation,zoom = 










































0, 0, 0, 0;local _list_0 = 
self.children;for _index_0 = 1, #_list_0 do local child = _list_0[_index_0]if 
child.get_transform then local t = 
child:get_transform()
ox = ox + (t.x or 0)
oy = oy + (t.y or 0)
rotation = rotation + (t.rotation or 0)
zoom = zoom + (t.zoom or 0)end end;return { x = 
ox, y = oy, rotation = rotation, zoom = zoom }end, to_world = function(self, sx, sy)local effects = 


















self:get_effects()local cx = 
self.x + effects.x;local cy = 
self.y + effects.y;local rot = 
self.rotation + effects.rotation;local zoom = 
self.zoom * (1 + effects.zoom)local x = 

sx - self.w / 2;local y = 
sy - self.h / 2
x = x / zoom
y = y / zoom;local cos_r = 
math.cos(-rot)local sin_r = 
math.sin(-rot)local rx = 
x * cos_r - y * sin_r;local ry = 
x * sin_r + y * cos_r;return 
rx + cx, ry + cy end, to_screen = function(self, wx, wy)local effects = 

















self:get_effects()local cx = 
self.x + effects.x;local cy = 
self.y + effects.y;local rot = 
self.rotation + effects.rotation;local zoom = 
self.zoom * (1 + effects.zoom)local x = 

wx - cx;local y = 
wy - cy;local cos_r = 
math.cos(rot)local sin_r = 
math.sin(rot)local rx = 
x * cos_r - y * sin_r;local ry = 
x * sin_r + y * cos_r;return 
rx * zoom + self.w / 2, ry * zoom + self.h / 2 end, attach = function(self, layer, parallax_x, parallax_y)if 



















parallax_x == nil then parallax_x = 1 end;if parallax_y == nil then parallax_y = 1 end;local effects = 
self:get_effects()local cx = 
self.x * parallax_x + effects.x;local cy = 
self.y * parallax_y + effects.y;local rot = 
self.rotation + effects.rotation;local zoom = 
self.zoom * (1 + effects.zoom)

layer:push(self.w / 2, self.h / 2, rot, zoom, zoom)return 
layer:push(-cx, -cy, 0, 1, 1)end, detach = function(self, layer)















layer:pop()return 
layer:pop()end, follow = function(self, target, lerp, lerp_time, lead)





















self.follow_target = target;if 
lerp then self.follow_lerp = lerp end;if 
lerp_time then self.follow_lerp_time = lerp_time end;if 
lead then self.follow_lead = lead end end, set_bounds = function(self, min_x, max_x, min_y, max_y)if 
















min_x then
self.bounds = { min_x = min_x, max_x = max_x, min_y = min_y, max_y = max_y }else

self.bounds = nil end end, early_update = function(self, dt)if 








self.follow_target and not self.follow_target.dead then local target_x = 
self.follow_target.x;local target_y = 
self.follow_target.y;if 

self.follow_lead > 0 and self.follow_target.collider then local vx,vy = 
self.follow_target.collider:get_velocity()
target_x = target_x + (vx * self.follow_lead)
target_y = target_y + (vy * self.follow_lead)end
self.x = math.lerp_dt(self.follow_lerp, self.follow_lerp_time, dt, self.x, target_x)
self.y = math.lerp_dt(self.follow_lerp, self.follow_lerp_time, dt, self.y, target_y)end;if 


self.bounds then local half_w = 
self.w / (2 * self.zoom)local half_h = 
self.h / (2 * self.zoom)
self.x = math.clamp(self.x, self.bounds.min_x + half_w, self.bounds.max_x - half_w)
self.y = math.clamp(self.y, self.bounds.min_y + half_h, self.bounds.max_y - half_h)end;local mx,my = 


mouse_position()
self.mouse.x, self.mouse.y = self:to_world(mx, my)end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, w, h)if w == nil then w = W end;if h == nil then h = H end;self.w = w;self.h = h;_class_0.__parent.__init(self, 'camera')self.x = self.w / 2;self.y = self.h / 2;self.rotation = 0;self.zoom = 1;self.mouse = { x = 0, y = 0 }self.follow_target = nil;self.follow_lerp = 0.9;self.follow_lerp_time = 0.5;self.follow_lead = 0;self.bounds = nil end, __base = _base_0, __name = "camera", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;camera = _class_0;return _class_0 end