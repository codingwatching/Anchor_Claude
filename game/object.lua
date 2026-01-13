do

local _class_0;local _base_0 = { add = function(self, child)do local _obj_0 = 








self.children;_obj_0[#_obj_0 + 1] = child end
child.parent = self;if 
child.name then if 
self[child.name] then self[child.name]:kill()end
self[child.name] = child end;if 
self.name then
child[self.name] = self end;return 
self end, all = function(self, tag)local nodes = 


{  }local stack = 
{  }for i = #
self.children, 1, -1 do
stack[#stack + 1] = self.children[i]end;while #
stack > 0 do local node = 
table.remove(stack)if 
tag then if 
node.tags[tag] then nodes[#nodes + 1] = node end else

nodes[#nodes + 1] = node end;for i = #
node.children, 1, -1 do
stack[#stack + 1] = node.children[i]end end;return 
nodes end, kill = function(self, tag)if 


tag then if 
self.tags[tag] then
self:kill()else local _list_0 = 

self:all(tag)for _index_0 = 1, #_list_0 do local obj = _list_0[_index_0]
obj:kill()end end else

self.dead = true;local _list_0 = 
self:all()for _index_0 = 1, #_list_0 do local obj = _list_0[_index_0]
obj.dead = true end end;return 
self end, tag = function(self, ...)local _list_0 = { 


... }for _index_0 = 1, #_list_0 do local t = _list_0[_index_0]
self.tags[t] = true end;return 
self end, is = function(self, name_or_tag)return 


self.name == name_or_tag or self.tags[name_or_tag]end, early_action = function(self, name_or_fn, fn)if not 


self.early_actions then self.early_actions = {  }end;if not 
self.early_action_names then self.early_action_names = {  }end;if 
type(name_or_fn) == 'string' then local name = 
name_or_fn;for i, n in 
ipairs(self.early_action_names) do if 
n == name then
self.early_actions[i] = fn
self[name] = fn;return 
self end end;do local _obj_0 = 
self.early_actions;_obj_0[#_obj_0 + 1] = fn end;do local _obj_0 = 
self.early_action_names;_obj_0[#_obj_0 + 1] = name end
self[name] = fn else do local _obj_0 = 

self.early_actions;_obj_0[#_obj_0 + 1] = name_or_fn end;local _obj_0 = 
self.early_action_names;_obj_0[#_obj_0 + 1] = false end;return 
self end, action = function(self, name_or_fn, fn)if not 


self.actions then self.actions = {  }end;if not 
self.action_names then self.action_names = {  }end;if 
type(name_or_fn) == 'string' then local name = 
name_or_fn;for i, n in 
ipairs(self.action_names) do if 
n == name then
self.actions[i] = fn
self[name] = fn;return 
self end end;do local _obj_0 = 
self.actions;_obj_0[#_obj_0 + 1] = fn end;do local _obj_0 = 
self.action_names;_obj_0[#_obj_0 + 1] = name end
self[name] = fn else do local _obj_0 = 

self.actions;_obj_0[#_obj_0 + 1] = name_or_fn end;local _obj_0 = 
self.action_names;_obj_0[#_obj_0 + 1] = false end;return 
self end, late_action = function(self, name_or_fn, fn)if not 


self.late_actions then self.late_actions = {  }end;if not 
self.late_action_names then self.late_action_names = {  }end;if 
type(name_or_fn) == 'string' then local name = 
name_or_fn;for i, n in 
ipairs(self.late_action_names) do if 
n == name then
self.late_actions[i] = fn
self[name] = fn;return 
self end end;do local _obj_0 = 
self.late_actions;_obj_0[#_obj_0 + 1] = fn end;do local _obj_0 = 
self.late_action_names;_obj_0[#_obj_0 + 1] = name end
self[name] = fn else do local _obj_0 = 

self.late_actions;_obj_0[#_obj_0 + 1] = name_or_fn end;local _obj_0 = 
self.late_action_names;_obj_0[#_obj_0 + 1] = false end;return 
self end, _early_update = function(self, dt)if 


self.dead then return end;if 
self.early_update then self:early_update(dt)end;if 
self.early_actions then for i, fn in 
ipairs(self.early_actions) do if 
fn(self, dt) == true then if not 
self.early_actions_to_remove then self.early_actions_to_remove = {  }end;local _obj_0 = 
self.early_actions_to_remove;_obj_0[#_obj_0 + 1] = i end end end end, _update = function(self, dt)if 


self.dead then return end;if 
self.update then self:update(dt)end;if 
self.actions then for i, fn in 
ipairs(self.actions) do if 
fn(self, dt) == true then if not 
self.actions_to_remove then self.actions_to_remove = {  }end;local _obj_0 = 
self.actions_to_remove;_obj_0[#_obj_0 + 1] = i end end end end, _late_update = function(self, dt)if 


self.dead then return end;if 
self.late_update then self:late_update(dt)end;if 
self.late_actions then for i, fn in 
ipairs(self.late_actions) do if 
fn(self, dt) == true then if not 
self.late_actions_to_remove then self.late_actions_to_remove = {  }end;local _obj_0 = 
self.late_actions_to_remove;_obj_0[#_obj_0 + 1] = i end end end end, cleanup = function(self)local objects = { 


self }local _list_0 = 
self:all()for _index_0 = 1, #_list_0 do local obj = _list_0[_index_0]objects[#objects + 1] = obj end;for _index_0 = 


1, #objects do local obj = objects[_index_0]if 
obj.early_actions_to_remove then for i = #
obj.early_actions_to_remove, 1, -1 do local idx = 
obj.early_actions_to_remove[i]local name = 
obj.early_action_names[idx]if 
name then obj[name] = nil end
table.remove(obj.early_actions, idx)
table.remove(obj.early_action_names, idx)end
obj.early_actions_to_remove = nil end;if 
obj.actions_to_remove then for i = #
obj.actions_to_remove, 1, -1 do local idx = 
obj.actions_to_remove[i]local name = 
obj.action_names[idx]if 
name then obj[name] = nil end
table.remove(obj.actions, idx)
table.remove(obj.action_names, idx)end
obj.actions_to_remove = nil end;if 
obj.late_actions_to_remove then for i = #
obj.late_actions_to_remove, 1, -1 do local idx = 
obj.late_actions_to_remove[i]local name = 
obj.late_action_names[idx]if 
name then obj[name] = nil end
table.remove(obj.late_actions, idx)
table.remove(obj.late_action_names, idx)end
obj.late_actions_to_remove = nil end end;for i = #


objects, 1, -1 do local parent = 
objects[i]local j = 
1;while 
j <= #parent.children do local child = 
parent.children[j]if 
child.dead then if 
child.destroy then child:destroy()end;if 
child.name then parent[child.name] = nil end;if 
parent.name then child[parent.name] = nil end
child.parent = nil
table.remove(parent.children, j)else

j = j + 1 end end end end }if _base_0.__index == nil then _base_0.__index = _base_0 end;_class_0 = setmetatable({ __init = function(self, name)self.name = name;self.parent = nil;self.children = {  }self.dead = false;self.tags = {  }end, __base = _base_0, __name = "object" }, { __index = _base_0, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;object = _class_0;return _class_0 end