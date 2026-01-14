do

local _class_0;local _base_0 = {  }if _base_0.__index == nil then _base_0.__index = _base_0 end;_class_0 = setmetatable({ __init = function(self, handle)
self.handle = handle
self.width = texture_get_width(self.handle)
self.height = texture_get_height(self.handle)end, __base = _base_0, __name = "image" }, { __index = _base_0, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;image = _class_0;return _class_0 end