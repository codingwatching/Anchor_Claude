do




















local _class_0;local _parent_0 = object;local _base_0 = { add = function(self, name, x, frequency, bounce)if 






































x == nil then x = 0 end;if frequency == nil then frequency = 5 end;if bounce == nil then bounce = 0.5 end;if not 
self[name] then local _obj_0 = self.spring_names;_obj_0[#_obj_0 + 1] = name end;local k = (
2 * math.pi * frequency) ^ 2;local d = 
4 * math.pi * (1 - bounce) * frequency

self[name] = { x = x, target_x = 
x, v = 
0, k = 
k, d = 
d }end, pull = function(self, name, force, frequency, bounce)local spring = 























self[name]if not 
spring then return end;if 
frequency then
spring.k = (2 * math.pi * frequency) ^ 2
spring.d = 4 * math.pi * (1 - (bounce or 0.5)) * frequency end
spring.x = spring.x + force end, set_target = function(self, name, value)local spring = 


















self[name]if 
spring then spring.target_x = value end end, at_rest = function(self, name, threshold)if 














threshold == nil then threshold = 0.01 end;local spring = 
self[name]if not 
spring then return true end;return 
math.abs(spring.x - spring.target_x) < threshold and math.abs(spring.v) < threshold end, early_update = function(self, dt)local _list_0 = 








self.spring_names;for _index_0 = 1, #_list_0 do local spring_name = _list_0[_index_0]local spring = 
self[spring_name]local a = -
spring.k * (spring.x - spring.target_x) - spring.d * spring.v
spring.v = spring.v + (a * dt)
spring.x = spring.x + (spring.v * dt)end end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self)_class_0.__parent.__init(self, 'spring')self.spring_names = {  }return self:add('main', 1)end, __base = _base_0, __name = "spring", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;spring = _class_0;return _class_0 end