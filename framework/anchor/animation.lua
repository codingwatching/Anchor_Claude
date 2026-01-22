do


























local _class_0;local _base_0 = { update = function(self, dt)if not 



























self.playing then return end;if 
self.dead then return end

self.timer = self.timer + dt;local current_delay = 
self:_get_delay(self.frame)while 

self.timer >= current_delay and self.playing and not self.dead do
self.timer = self.timer - current_delay
self:_advance_frame()
current_delay = self:_get_delay(self.frame)end end, play = function(self)





self.playing = true end, stop = function(self)





self.playing = false end, reset = function(self)





self.frame = 1
self.timer = 0
self.direction = 1
self.dead = false
self.playing = true;return 
self:_fire_action(1)end, set_frame = function(self, frame)








self.frame = math.clamp(frame, 1, self.spritesheet.frames)
self.timer = 0;return 
self:_fire_action(self.frame)end, _get_delay = function(self, frame)if 



type(self.delay) == 'table' then return 
self.delay[frame] or self.delay[1] or 0.1 else return 

self.delay end end, _fire_action = function(self, frame)if 



self.actions[frame] then if 
self.parent then return 
self.actions[frame](self.parent)else return 

self.actions[frame](self)end end end, _advance_frame = function(self)local next_frame = 



self.frame + self.direction;if 

self.loop_mode == 'once' then if 
next_frame > self.spritesheet.frames then
self.dead = true
self.playing = false
self:_fire_action(0)
return end
self.frame = next_frame;return 
self:_fire_action(self.frame)elseif 

self.loop_mode == 'loop' then if 
next_frame > self.spritesheet.frames then
self.frame = 1
self:_fire_action(0)return 
self:_fire_action(self.frame)else

self.frame = next_frame;return 
self:_fire_action(self.frame)end elseif 

self.loop_mode == 'bounce' then if 
next_frame > self.spritesheet.frames then
self.direction = -1
self.frame = self.spritesheet.frames - 1
self:_fire_action(0)if 
self.frame >= 1 then return self:_fire_action(self.frame)end elseif 
next_frame < 1 then
self.direction = 1
self.frame = 2
self:_fire_action(0)if 
self.frame <= self.spritesheet.frames then return self:_fire_action(self.frame)end else

self.frame = next_frame;return 
self:_fire_action(self.frame)end end end }if _base_0.__index == nil then _base_0.__index = _base_0 end;_class_0 = setmetatable({ __init = function(self, spritesheet, delay, loop_mode, actions)if delay == nil then delay = 0.1 end;if loop_mode == nil then loop_mode = 'loop'end;if actions == nil then actions = {  }end;self.spritesheet = spritesheet;self.delay = delay;self.loop_mode = loop_mode;self.actions = actions;self.frame = 1;self.timer = 0;self.direction = 1;self.playing = true;self.dead = false;return self:_fire_action(1)end, __base = _base_0, __name = "animation" }, { __index = _base_0, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;animation = _class_0;return _class_0 end