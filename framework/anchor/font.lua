do

local _class_0;local _base_0 = { text_width = function(self, text)return 





font_get_text_width(self.name, text)end, char_width = function(self, codepoint)return 


font_get_char_width(self.name, codepoint)end, glyph_metrics = function(self, codepoint)return 


font_get_glyph_metrics(self.name, codepoint)end }if _base_0.__index == nil then _base_0.__index = _base_0 end;_class_0 = setmetatable({ __init = function(self, name, path, size)self.name = name;self.size = size;font_load(self.name, path, self.size)self.height = font_get_height(self.name)end, __base = _base_0, __name = "font" }, { __index = _base_0, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;font = _class_0;return _class_0 end