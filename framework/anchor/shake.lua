do














local _class_0;local _parent_0 = object;local _base_0 = { trauma = function(self, amount, duration)if 













































duration == nil then duration = 0.5 end;local _obj_0 = 
self.trauma_instances
_obj_0[#_obj_0 + 1] = { value = amount, decay = 
amount / duration }end, set_trauma_parameters = function(self, amplitude)if 
















amplitude.x then self.trauma_amplitude.x = amplitude.x end;if 
amplitude.y then self.trauma_amplitude.y = amplitude.y end;if 
amplitude.rotation then self.trauma_amplitude.rotation = amplitude.rotation end;if 
amplitude.zoom then self.trauma_amplitude.zoom = amplitude.zoom end end, shake = function(self, amplitude, duration, frequency)if 


















frequency == nil then frequency = 60 end;if not 
self.shake_instances then self.shake_instances = {  }end;local _obj_0 = 
self.shake_instances
_obj_0[#_obj_0 + 1] = { amplitude = amplitude, duration = 
duration, frequency = 
frequency, time = 
0, current_x = 
0, current_y = 
0, last_change = 
0 }end, push = function(self, angle, amount, frequency, bounce)






















self.spring:pull('x', math.cos(angle) * amount, frequency, bounce)return 
self.spring:pull('y', math.sin(angle) * amount, frequency, bounce)end, sine = function(self, angle, amplitude, frequency, duration)if not 




















self.sine_instances then self.sine_instances = {  }end;local _obj_0 = 
self.sine_instances
_obj_0[#_obj_0 + 1] = { angle = angle, amplitude = 
amplitude, frequency = 
frequency, duration = 
duration, time = 
0 }end, square = function(self, angle, amplitude, frequency, duration)if not 





















self.square_instances then self.square_instances = {  }end;local _obj_0 = 
self.square_instances
_obj_0[#_obj_0 + 1] = { angle = angle, amplitude = 
amplitude, frequency = 
frequency, duration = 
duration, time = 
0 }end, handcam = function(self, enabled, amplitude, frequency)





















self.handcam_enabled = enabled;if 
amplitude then if 
amplitude.x then self.handcam_amplitude.x = amplitude.x end;if 
amplitude.y then self.handcam_amplitude.y = amplitude.y end;if 
amplitude.rotation then self.handcam_amplitude.rotation = amplitude.rotation end;if 
amplitude.zoom then self.handcam_amplitude.zoom = amplitude.zoom end end;if 
frequency then self.handcam_frequency = frequency end end, get_transform = function(self)local ox,oy,rotation,zoom = 









0, 0, 0, 0;if 


self.handcam_enabled then local t = 
self.handcam_time * self.handcam_frequency
ox = ox + (self.handcam_amplitude.x * noise(t, 0))
oy = oy + (self.handcam_amplitude.y * noise(0, t))
rotation = rotation + (self.handcam_amplitude.rotation * noise(t, t))
zoom = zoom + (self.handcam_amplitude.zoom * noise(t * 0.7, 0, t))end;local total_trauma = 


0;local _list_0 = 
self.trauma_instances;for _index_0 = 1, #_list_0 do local instance = _list_0[_index_0]
total_trauma = total_trauma + instance.value end;if 


total_trauma > 0 then local intensity = 
total_trauma * total_trauma
ox = ox + (intensity * self.trauma_amplitude.x * noise(self.trauma_time * 10, 0))
oy = oy + (intensity * self.trauma_amplitude.y * noise(0, self.trauma_time * 10))
rotation = rotation + (intensity * self.trauma_amplitude.rotation * noise(self.trauma_time * 10, self.trauma_time * 10))
zoom = zoom + (intensity * self.trauma_amplitude.zoom * noise(self.trauma_time * 5, 0, self.trauma_time * 5))end


ox = ox + self.spring.x.x
oy = oy + self.spring.y.x;if 


self.shake_instances then local _list_1 = 
self.shake_instances;for _index_0 = 1, #_list_1 do local instance = _list_1[_index_0]
ox = ox + instance.current_x
oy = oy + instance.current_y end end;if 


self.sine_instances then local _list_1 = 
self.sine_instances;for _index_0 = 1, #_list_1 do local instance = _list_1[_index_0]local decay = 
1 - (instance.time / instance.duration)local wave = 
math.sin(instance.time * instance.frequency * 2 * math.pi)local offset = 
decay * instance.amplitude * wave
ox = ox + (offset * math.cos(instance.angle))
oy = oy + (offset * math.sin(instance.angle))end end;if 


self.square_instances then local _list_1 = 
self.square_instances;for _index_0 = 1, #_list_1 do local instance = _list_1[_index_0]local decay = 
1 - (instance.time / instance.duration)local wave = 
math.sin(instance.time * instance.frequency * 2 * math.pi) > 0 and 1 or -1;local offset = 
decay * instance.amplitude * wave
ox = ox + (offset * math.cos(instance.angle))
oy = oy + (offset * math.sin(instance.angle))end end;return { x = 

ox, y = oy, rotation = rotation, zoom = zoom }end, early_update = function(self, dt)if 








self.handcam_enabled then
self.handcam_time = self.handcam_time + dt end;for i = #


self.trauma_instances, 1, -1 do local instance = 
self.trauma_instances[i]
instance.value = instance.value - (instance.decay * dt)if 
instance.value <= 0 then
table.remove(self.trauma_instances, i)end end;if #

self.trauma_instances > 0 then
self.trauma_time = self.trauma_time + dt end;if 


self.shake_instances then for i = #
self.shake_instances, 1, -1 do local instance = 
self.shake_instances[i]
instance.time = instance.time + dt;local change_interval = 


1 / instance.frequency;if 
instance.time - instance.last_change >= change_interval then
instance.last_change = instance.time;local decay = 
1 - (instance.time / instance.duration)if 
decay > 0 then
instance.current_x = decay * instance.amplitude * random_float(-1, 1)
instance.current_y = decay * instance.amplitude * random_float(-1, 1)else

instance.current_x = 0
instance.current_y = 0 end end;if 


instance.time >= instance.duration then
table.remove(self.shake_instances, i)end end end;if 


self.sine_instances then for i = #
self.sine_instances, 1, -1 do local instance = 
self.sine_instances[i]
instance.time = instance.time + dt;if 
instance.time >= instance.duration then
table.remove(self.sine_instances, i)end end end;if 


self.square_instances then for i = #
self.square_instances, 1, -1 do local instance = 
self.square_instances[i]
instance.time = instance.time + dt;if 
instance.time >= instance.duration then
table.remove(self.square_instances, i)end end end end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self, name)if name == nil then name = 'shake'end;_class_0.__parent.__init(self, name)self.trauma_instances = {  }self.trauma_amplitude = { x = 24, y = 24, rotation = 0.2, zoom = 0.2 }self.trauma_time = 0;self:add(spring())self.spring:add('x', 0, 3, 0.5)self.spring:add('y', 0, 3, 0.5)self.handcam_enabled = false;self.handcam_amplitude = { x = 5, y = 5, rotation = 0.02, zoom = 0.02 }self.handcam_frequency = 0.5;self.handcam_time = 0 end, __base = _base_0, __name = "shake", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;shake = _class_0;return _class_0 end