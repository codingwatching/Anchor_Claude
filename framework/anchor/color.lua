


















































_anon_func_0 = function(b, g)if g < b then return 6 else return 0 end end;rgb_to_hsl = function(r, g, b)r, g, b = r / 255, g / 255, b / 255;local max = math.max(r, g, b)local min = math.min(r, g, b)local l = (max + min) / 2;if max == min then return 0, 0, l end;local d = max - min;local s;if l > 0.5 then s = d / (2 - max - min)else s = d / (max + min)end;local h;if max == r then h = ((g - b) / d + (_anon_func_0(b, g))) / 6 elseif 
max == g then
h = ((b - r) / d + 2) / 6 else

h = ((r - g) / d + 4) / 6 end;return 

h * 360, s, l end


hsl_to_rgb = function(h, s, l)if 
s == 0 then local v = 
math.floor(l * 255 + 0.5)return 
v, v, v end

h = h / 360
local q;if l < 0.5 then q = l * (1 + s)else q = l + s - l * s end;local p = 
2 * l - q

local hue_to_rgb;hue_to_rgb = function(t)if 
t < 0 then t = t + 1 end;if 
t > 1 then t = t - 1 end;if 
t < 1 / 6 then return 
p + (q - p) * 6 * t elseif 
t < 1 / 2 then return 
q elseif 
t < 2 / 3 then return 
p + (q - p) * (2 / 3 - t) * 6 else return 

p end end;local r = 

math.floor(hue_to_rgb(h + 1 / 3) * 255 + 0.5)local g = 
math.floor(hue_to_rgb(h) * 255 + 0.5)local b = 
math.floor(hue_to_rgb(h - 1 / 3) * 255 + 0.5)return 
r, g, b end;do

local _class_0;local _base_0 = { sync_hsl = function(self)local h,s,l = 






















rgb_to_hsl(self.data.r, self.data.g, self.data.b)
self.data.h = h
self.data.s = s
self.data.l = l end, sync_rgb = function(self)local r,g,b = 



hsl_to_rgb(self.data.h, self.data.s, self.data.l)
self.data.r = r
self.data.g = g
self.data.b = b end, __index = function(self, key)if 




'r' == key then return self.data.r elseif 
'g' == key then return self.data.g elseif 
'b' == key then return self.data.b elseif 
'a' == key then return self.data.a elseif 
'h' == key then return self.data.h elseif 
's' == key then return self.data.s elseif 
'l' == key then return self.data.l else return 

rawget(color.__base, key)end end, __newindex = function(self, key, value)if 




'r' == key then
self.data.r = value;return 
self:sync_hsl()elseif 
'g' == key then
self.data.g = value;return 
self:sync_hsl()elseif 
'b' == key then
self.data.b = value;return 
self:sync_hsl()elseif 
'a' == key then
self.data.a = value elseif 
'h' == key then
self.data.h = value % 360;return 
self:sync_rgb()elseif 
's' == key then
self.data.s = math.max(0, math.min(1, value))return 
self:sync_rgb()elseif 
'l' == key then
self.data.l = math.max(0, math.min(1, value))return 
self:sync_rgb()else return 

rawset(self, key, value)end end, __call = function(self)return 











rgba(math.floor(self.data.r + 0.5), math.floor(self.data.g + 0.5), math.floor(self.data.b + 0.5), math.floor(self.data.a + 0.5))end, __mul = function(self, other)if 





















type(other) == 'number' then
self.data.r = math.max(0, math.min(255, self.data.r * other))
self.data.g = math.max(0, math.min(255, self.data.g * other))
self.data.b = math.max(0, math.min(255, self.data.b * other))else

self.data.r = math.max(0, math.min(255, self.data.r * other.r / 255))
self.data.g = math.max(0, math.min(255, self.data.g * other.g / 255))
self.data.b = math.max(0, math.min(255, self.data.b * other.b / 255))end
self:sync_hsl()return 
self end, __div = function(self, other)if 



















type(other) == 'number' then
self.data.r = math.max(0, math.min(255, self.data.r / other))
self.data.g = math.max(0, math.min(255, self.data.g / other))
self.data.b = math.max(0, math.min(255, self.data.b / other))
self:sync_hsl()end;return 
self end, __add = function(self, other)if 





















type(other) == 'number' then
self.data.r = math.max(0, math.min(255, self.data.r + other))
self.data.g = math.max(0, math.min(255, self.data.g + other))
self.data.b = math.max(0, math.min(255, self.data.b + other))else

self.data.r = math.max(0, math.min(255, self.data.r + other.r))
self.data.g = math.max(0, math.min(255, self.data.g + other.g))
self.data.b = math.max(0, math.min(255, self.data.b + other.b))end
self:sync_hsl()return 
self end, __sub = function(self, other)if 





















type(other) == 'number' then
self.data.r = math.max(0, math.min(255, self.data.r - other))
self.data.g = math.max(0, math.min(255, self.data.g - other))
self.data.b = math.max(0, math.min(255, self.data.b - other))else

self.data.r = math.max(0, math.min(255, self.data.r - other.r))
self.data.g = math.max(0, math.min(255, self.data.g - other.g))
self.data.b = math.max(0, math.min(255, self.data.b - other.b))end
self:sync_hsl()return 
self end, clone = function(self)return 











color(self.data.r, self.data.g, self.data.b, self.data.a)end, invert = function(self)
















self.data.r = 255 - self.data.r
self.data.g = 255 - self.data.g
self.data.b = 255 - self.data.b
self:sync_hsl()return 
self end, mix = function(self, other, t)if 





















t == nil then t = 0.5 end
self.data.r = self.data.r + (other.r - self.data.r) * t
self.data.g = self.data.g + (other.g - self.data.g) * t
self.data.b = self.data.b + (other.b - self.data.b) * t
self.data.a = self.data.a + (other.a - self.data.a) * t
self:sync_hsl()return 
self end }if _base_0.__index == nil then _base_0.__index = _base_0 end;_class_0 = setmetatable({ __init = function(self, r, g, b, a)if r == nil then r = 255 end;if g == nil then g = 255 end;if b == nil then b = 255 end;if a == nil then a = 255 end;rawset(self, 'data', { r = r, g = g, b = b, a = a, h = 0, s = 0, l = 0 })return self:sync_hsl()end, __base = _base_0, __name = "color" }, { __index = _base_0, __call = function(cls, ...)local _self_0 = setmetatable({  }, _base_0)cls.__init(_self_0, ...)return _self_0 end })_base_0.__class = _class_0;color = _class_0;return _class_0 end