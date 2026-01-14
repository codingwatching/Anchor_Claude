do

local _class_0;local _base_0 = { rectangle = function(self, x, y, w, h, color)return 




layer_rectangle(self.handle, x, y, w, h, color)end, circle = function(self, x, y, radius, color)return 


layer_circle(self.handle, x, y, radius, color)end, image = function(self, image, x, y, color, flash)return 


layer_draw_texture(self.handle, image.handle, x, y, color or 0xFFFFFFFF, flash or 0)end, text = function(self, text, font, x, y, color)


local font_name;if type(font) == 'string' then font_name = font else font_name = font.name end;return 
layer_draw_text(self.handle, text, font_name, x, y, color)end, push = function(self, x, y, r, sx, sy)return 


layer_push(self.handle, x, y, r, sx, sy)end, pop = function(self)return 


layer_pop(self.handle)end, set_blend_mode = function(self, mode)return 


layer_set_blend_mode(self.handle, mode)end, draw = function(self, x, y)return 


layer_draw(self.handle, x or 0, y or 0)end }if _base_0.__index == nil then _base_0.__index = _base_0 end;_class_0 = setmetatable({ __init = function(self, name)self.name = name;self.handle = layer_create(self.name)end, __base = _base_0, __name = "layer" }, { __index = _base_0, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;layer = _class_0;return _class_0 end