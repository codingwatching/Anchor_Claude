














require('anchor.object')
require('anchor.layer')
require('anchor.image')
require('anchor.font')
require('anchor.timer')
require('anchor.math')













an = object('an')
an.layers = {  }
an.images = {  }
an.fonts = {  }














an.layer = function(self, name)
self.layers[name] = layer(name)return 
self.layers[name]end















an.image = function(self, name, path)local handle = 
texture_load(path)
self.images[name] = image(handle)return 
self.images[name]end















an.font = function(self, name, path, size)
self.fonts[name] = font(name, path, size)return 
self.fonts[name]end


















update = function(dt)local all_objects = { 
an }local _list_0 = 
an:all()for _index_0 = 1, #_list_0 do local obj = _list_0[_index_0]all_objects[#all_objects + 1] = obj end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_early_update(dt)end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_update(dt)end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_late_update(dt)end;return 
an:cleanup()end