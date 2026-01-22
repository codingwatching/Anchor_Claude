do


















local _class_0;local _base_0 = {  }if _base_0.__index == nil then _base_0.__index = _base_0 end;_class_0 = setmetatable({ __init = function(self, handle)










self.handle = handle
self.frame_width = spritesheet_get_frame_width(self.handle)
self.frame_height = spritesheet_get_frame_height(self.handle)
self.frames = spritesheet_get_total_frames(self.handle)end, __base = _base_0, __name = "spritesheet" }, { __index = _base_0, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;spritesheet = _class_0;return _class_0 end