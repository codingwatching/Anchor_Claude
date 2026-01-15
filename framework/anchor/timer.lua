

require('anchor.object')do

local _class_0;local _parent_0 = object;local _base_0 = { uid = function(self)local id = 







"_anon_" .. tostring(self.next_id)
self.next_id = self.next_id + 1;return 
id end, find = function(self, name)for index, entry in 



ipairs(self.entries) do if 
entry.name == name then return index end end;return 
nil end, after = function(self, delay, name_or_callback, callback_function)



local name,callback;if type(name_or_callback) == 'string' then
name, callback = name_or_callback, callback_function else

name, callback = self:uid(), name_or_callback end;local entry = { name = 
name, mode = 'after', time = 0, delay = delay, callback = callback }local index = 
self:find(name)if 
index then
self.entries[index] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, every = function(self, delay, name_or_callback, callback_or_times, times_or_after, after_function)



local name,callback,times,after;if type(name_or_callback) == 'string' then
name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function else

name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after end;local entry = { name = 
name, mode = 'every', time = 0, delay = delay, callback = callback, times = times, after = after, count = 0 }local index = 
self:find(name)if 
index then
self.entries[index] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, during = function(self, duration, name_or_callback, callback_or_after, after_function)




local name,callback,after;if type(name_or_callback) == 'string' then
name, callback, after = name_or_callback, callback_or_after, after_function else

name, callback, after = self:uid(), name_or_callback, callback_or_after end;local entry = { name = 
name, mode = 'during', time = 0, duration = duration, callback = callback, after = after }local index = 
self:find(name)if 
index then
self.entries[index] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, tween = function(self, duration, name_or_target, target_or_values, values_or_easing, easing_or_after, after_function)



local name,target,values,easing,after;if type(name_or_target) == 'string' then
name, target, values, easing, after = name_or_target, target_or_values, values_or_easing, easing_or_after, after_function else

name, target, values, easing, after = self:uid(), name_or_target, target_or_values, values_or_easing, easing_or_after end
easing = easing or math.linear;local initial_values = 

{  }for key, _ in 
pairs(values) do
initial_values[key] = target[key]end;local entry = { name = 
name, mode = 'tween', time = 0, duration = duration, target = target, values = values, initial_values = initial_values, easing = easing, after = after }local index = 
self:find(name)if 
index then
self.entries[index] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, watch = function(self, field, name_or_callback, callback_or_times, times_or_after, after_function)




local name,callback,times,after;if type(name_or_callback) == 'string' then
name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function else

name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after end;local initial_value = 
self.parent[field]local entry = { name = 
name, mode = 'watch', time = 0, field = field, current = initial_value, previous = initial_value, callback = callback, times = times, after = after, count = 0 }if 
self:find(name) then
self.entries[self:find(name)] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, when = function(self, condition_fn, name_or_callback, callback_or_times, times_or_after, after_function)




local name,callback,times,after;if type(name_or_callback) == 'string' then
name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function else

name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after end;local entry = { name = 
name, mode = 'when', time = 0, condition = condition_fn, last_condition = false, callback = callback, times = times, after = after, count = 0 }if 
self:find(name) then
self.entries[self:find(name)] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, cooldown = function(self, delay, condition_fn, name_or_callback, callback_or_times, times_or_after, after_function)




local name,callback,times,after;if type(name_or_callback) == 'string' then
name, callback, times, after = name_or_callback, callback_or_times, times_or_after, after_function else

name, callback, times, after = self:uid(), name_or_callback, callback_or_times, times_or_after end;local entry = { name = 
name, mode = 'cooldown', time = 0, delay = delay, condition = condition_fn, last_condition = false, callback = callback, times = times, after = after, count = 0 }if 
self:find(name) then
self.entries[self:find(name)] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, every_step = function(self, start_delay, end_delay, times, name_or_callback, callback_or_step, step_or_after, after_function)





local name,callback,step_method,after;if type(name_or_callback) == 'string' then
name, callback, step_method, after = name_or_callback, callback_or_step, step_or_after, after_function else

name, callback, step_method, after = self:uid(), name_or_callback, callback_or_step, step_or_after end
step_method = step_method or math.linear;local delays = 

{  }for i = 
1, times do local t = (
i - 1) / (times - 1)
t = step_method(t)
delays[i] = math.lerp(t, start_delay, end_delay)end;local entry = { name = 
name, mode = 'every_step', time = 0, delays = delays, callback = callback, after = after, step_index = 1 }if 
self:find(name) then
self.entries[self:find(name)] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, during_step = function(self, duration, start_delay, end_delay, name_or_callback, callback_or_step, step_or_after, after_function)





local name,callback,step_method,after;if type(name_or_callback) == 'string' then
name, callback, step_method, after = name_or_callback, callback_or_step, step_or_after, after_function else

name, callback, step_method, after = self:uid(), name_or_callback, callback_or_step, step_or_after end
step_method = step_method or math.linear;local times = 

math.ceil(2 * duration / (start_delay + end_delay))
times = math.max(times, 2)local delays = 

{  }for i = 
1, times do local t = (
i - 1) / (times - 1)
t = step_method(t)
delays[i] = math.lerp(t, start_delay, end_delay)end;local entry = { name = 
name, mode = 'during_step', time = 0, delays = delays, callback = callback, after = after, step_index = 1 }if 
self:find(name) then
self.entries[self:find(name)] = entry else local _obj_0 = 

self.entries;_obj_0[#_obj_0 + 1] = entry end end, cancel = function(self, name)local index = 


self:find(name)if 
index then self.entries[index].cancelled = true end end, trigger = function(self, name)local index = 



self:find(name)if not 
index then return end;local entry = 
self.entries[index]local _exp_0 = 
entry.mode;if 
'after' == _exp_0 then
entry.callback()
entry.cancelled = true elseif 
'every' == _exp_0 then
entry.callback()
entry.time = 0 elseif 
'cooldown' == _exp_0 then
entry.callback()
entry.time = 0 elseif 
'every_step' == _exp_0 then
entry.callback()
entry.time = 0 elseif 
'during_step' == _exp_0 then
entry.callback()
entry.time = 0 elseif 
'watch' == _exp_0 then return 
entry.callback(entry.current, entry.previous)elseif 
'when' == _exp_0 then return 
entry.callback()end end, set_multiplier = function(self, name, multiplier)if 


multiplier == nil then multiplier = 1 end;local index = 
self:find(name)if not 
index then return end
self.entries[index].multiplier = multiplier end, get_time_left = function(self, name)local index = 



self:find(name)if not 
index then return nil end;local entry = 
self.entries[index]local _exp_0 = 
entry.mode;if 
'after' == _exp_0 or 'every' == _exp_0 or 'cooldown' == _exp_0 then local delay = 
entry.delay * (entry.multiplier or 1)return 
delay - entry.time elseif 
'during' == _exp_0 or 'tween' == _exp_0 then local duration = 
entry.duration * (entry.multiplier or 1)return 
duration - entry.time elseif 
'every_step' == _exp_0 or 'during_step' == _exp_0 then return 
entry.delays[entry.step_index] - entry.time else return 

nil end end, update = function(self, dt)local to_remove = 


{  }for index, entry in 
ipairs(self.entries) do if 
entry.cancelled then
to_remove[#to_remove + 1] = index
goto _continue_0 end
entry.time = entry.time + dt;local _exp_0 = 

entry.mode;if 
'after' == _exp_0 then local delay = 
entry.delay * (entry.multiplier or 1)if 
entry.time >= delay then
entry.callback()
to_remove[#to_remove + 1] = index end elseif 

'every' == _exp_0 then local delay = 
entry.delay * (entry.multiplier or 1)if 
entry.time >= delay then
entry.callback()
entry.time = entry.time - delay;if 
entry.times then
entry.count = entry.count + 1;if 
entry.count >= entry.times then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end end end elseif 

'during' == _exp_0 then local duration = 
entry.duration * (entry.multiplier or 1)local progress = 
math.min(entry.time / duration, 1)
entry.callback(dt, progress)if 
entry.time >= duration then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end elseif 

'tween' == _exp_0 then local duration = 
entry.duration * (entry.multiplier or 1)local progress = 
math.min(entry.time / duration, 1)local eased = 
entry.easing(progress)for key, target_value in 
pairs(entry.values) do
entry.target[key] = math.lerp(eased, entry.initial_values[key], target_value)end;if 
entry.time >= duration then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end elseif 

'watch' == _exp_0 then
entry.previous = entry.current
entry.current = self.parent[entry.field]if 
entry.previous ~= entry.current then
entry.callback(entry.current, entry.previous)if 
entry.times then
entry.count = entry.count + 1;if 
entry.count >= entry.times then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end end end elseif 

'when' == _exp_0 then local current_condition = 
entry.condition()if 
current_condition and not entry.last_condition then
entry.callback()if 
entry.times then
entry.count = entry.count + 1;if 
entry.count >= entry.times then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end end end
entry.last_condition = current_condition elseif 

'cooldown' == _exp_0 then local delay = 
entry.delay * (entry.multiplier or 1)local current_condition = 
entry.condition()if 

current_condition and not entry.last_condition then
entry.time = 0 end;if 

entry.time >= delay and current_condition then
entry.callback()
entry.time = 0;if 
entry.times then
entry.count = entry.count + 1;if 
entry.count >= entry.times then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end end end
entry.last_condition = current_condition elseif 

'every_step' == _exp_0 then if 
entry.time >= entry.delays[entry.step_index] then
entry.callback()
entry.time = entry.time - entry.delays[entry.step_index]
entry.step_index = entry.step_index + 1;if 
entry.step_index > #entry.delays then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end end elseif 

'during_step' == _exp_0 then if 
entry.time >= entry.delays[entry.step_index] then
entry.callback()
entry.time = entry.time - entry.delays[entry.step_index]
entry.step_index = entry.step_index + 1;if 
entry.step_index > #entry.delays then if 
entry.after then entry.after()end
to_remove[#to_remove + 1] = index end end end::_continue_0::end;for i = #


to_remove, 1, -1 do
table.remove(self.entries, to_remove[i])end end }for _key_0, _val_0 in pairs(_parent_0.__base) do if _base_0[_key_0] == nil and _key_0:match("^__") and not (_key_0 == "__index" and _val_0 == _parent_0.__base) then _base_0[_key_0] = _val_0 end end;if _base_0.__index == nil then _base_0.__index = _base_0 end;setmetatable(_base_0, _parent_0.__base)_class_0 = setmetatable({ __init = function(self)_class_0.__parent.__init(self, 'timer')self.entries = {  }self.next_id = 1 end, __base = _base_0, __name = "timer", __parent = _parent_0 }, { __index = function(cls, name)local val = rawget(_base_0, name)if val == nil then local parent = rawget(cls, "__parent")if parent then return parent[name]end else return val end end, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;if _parent_0.__inherited then _parent_0.__inherited(_parent_0, _class_0)end;timer = _class_0;return _class_0 end