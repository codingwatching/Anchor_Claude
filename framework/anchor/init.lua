














require('anchor.object')
require('anchor.layer')
require('anchor.image')
require('anchor.font')
require('anchor.timer')
require('anchor.math')
require('anchor.collider')













an = object('an')
an.layers = {  }
an.images = {  }
an.fonts = {  }
an.shaders = {  }














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















an.shader = function(self, name, path)
self.shaders[name] = shader_load_file(path)return 
self.shaders[name]end














an.shader_string = function(self, name, source)
self.shaders[name] = shader_load_string(source)return 
self.shaders[name]end


an.colliders = {  }
an.collision_pairs = {  }
an.sensor_pairs = {  }
an.hit_pairs = {  }









an.physics_init = function(self)return 
physics_init()end








an.physics_set_gravity = function(self, gx, gy)return 
physics_set_gravity(gx, gy)end







an.physics_set_meter_scale = function(self, scale)return 
physics_set_meter_scale(scale)end








an.physics_set_enabled = function(self, enabled)return 
physics_set_enabled(enabled)end











an.physics_tag = function(self, name)return 
physics_register_tag(name)end










an.physics_collision = function(self, tag_a, tag_b)
physics_enable_collision(tag_a, tag_b)local _obj_0 = 
self.collision_pairs;_obj_0[#_obj_0 + 1] = { a = tag_a, b = tag_b }end










an.physics_sensor = function(self, tag_a, tag_b)
physics_enable_sensor(tag_a, tag_b)local _obj_0 = 
self.sensor_pairs;_obj_0[#_obj_0 + 1] = { a = tag_a, b = tag_b }end










an.physics_hit = function(self, tag_a, tag_b)
physics_enable_hit(tag_a, tag_b)local _obj_0 = 
self.hit_pairs;_obj_0[#_obj_0 + 1] = { a = tag_a, b = tag_b }end













an.collision_begin_events = function(self, tag_a, tag_b)local result = 
{  }local _list_0 = 
physics_get_collision_begin(tag_a, tag_b)for _index_0 = 1, #_list_0 do local event = _list_0[_index_0]local id_a = 
physics_get_user_data(event.body_a)local id_b = 
physics_get_user_data(event.body_b)local collider_a = 
self.colliders[id_a]local collider_b = 
self.colliders[id_b]if 
collider_a and collider_b then if 

collider_a.tag == tag_a then

result[#result + 1] = { a = collider_a.parent, b = 
collider_b.parent, shape_a = 
event.shape_a, shape_b = 
event.shape_b, point_x = 
event.point_x, point_y = 
event.point_y, normal_x = 
event.normal_x, normal_y = 
event.normal_y }else



result[#result + 1] = { a = collider_b.parent, b = 
collider_a.parent, shape_a = 
event.shape_b, shape_b = 
event.shape_a, point_x = 
event.point_x, point_y = 
event.point_y, normal_x = -
event.normal_x, normal_y = -
event.normal_y }end end end;return 

result end











an.collision_end_events = function(self, tag_a, tag_b)local result = 
{  }local _list_0 = 
physics_get_collision_end(tag_a, tag_b)for _index_0 = 1, #_list_0 do local event = _list_0[_index_0]local id_a = 
physics_get_user_data(event.body_a)local id_b = 
physics_get_user_data(event.body_b)local collider_a = 
self.colliders[id_a]local collider_b = 
self.colliders[id_b]if 
collider_a and collider_b then if 

collider_a.tag == tag_a then

result[#result + 1] = { a = collider_a.parent, b = 
collider_b.parent, shape_a = 
event.shape_a, shape_b = 
event.shape_b }else



result[#result + 1] = { a = collider_b.parent, b = 
collider_a.parent, shape_a = 
event.shape_b, shape_b = 
event.shape_a }end end end;return 

result end












an.sensor_begin_events = function(self, tag_a, tag_b)local result = 
{  }local _list_0 = 
physics_get_sensor_begin(tag_a, tag_b)for _index_0 = 1, #_list_0 do local event = _list_0[_index_0]local id_a = 
physics_get_user_data(event.sensor_body)local id_b = 
physics_get_user_data(event.visitor_body)local collider_a = 
self.colliders[id_a]local collider_b = 
self.colliders[id_b]if 
collider_a and collider_b then if 

collider_a.tag == tag_a then

result[#result + 1] = { a = collider_a.parent, b = 
collider_b.parent, shape_a = 
event.sensor_shape, shape_b = 
event.visitor_shape }else



result[#result + 1] = { a = collider_b.parent, b = 
collider_a.parent, shape_a = 
event.visitor_shape, shape_b = 
event.sensor_shape }end end end;return 

result end











an.sensor_end_events = function(self, tag_a, tag_b)local result = 
{  }local _list_0 = 
physics_get_sensor_end(tag_a, tag_b)for _index_0 = 1, #_list_0 do local event = _list_0[_index_0]local id_a = 
physics_get_user_data(event.sensor_body)local id_b = 
physics_get_user_data(event.visitor_body)local collider_a = 
self.colliders[id_a]local collider_b = 
self.colliders[id_b]if 
collider_a and collider_b then if 

collider_a.tag == tag_a then

result[#result + 1] = { a = collider_a.parent, b = 
collider_b.parent, shape_a = 
event.sensor_shape, shape_b = 
event.visitor_shape }else



result[#result + 1] = { a = collider_b.parent, b = 
collider_a.parent, shape_a = 
event.visitor_shape, shape_b = 
event.sensor_shape }end end end;return 

result end














an.hit_events = function(self, tag_a, tag_b)local result = 
{  }local _list_0 = 
physics_get_hit(tag_a, tag_b)for _index_0 = 1, #_list_0 do local event = _list_0[_index_0]local id_a = 
physics_get_user_data(event.body_a)local id_b = 
physics_get_user_data(event.body_b)local collider_a = 
self.colliders[id_a]local collider_b = 
self.colliders[id_b]if 
collider_a and collider_b then if 

collider_a.tag == tag_a then

result[#result + 1] = { a = collider_a.parent, b = 
collider_b.parent, shape_a = 
event.shape_a, shape_b = 
event.shape_b, point_x = 
event.point_x, point_y = 
event.point_y, normal_x = 
event.normal_x, normal_y = 
event.normal_y, approach_speed = 
event.approach_speed }else



result[#result + 1] = { a = collider_b.parent, b = 
collider_a.parent, shape_a = 
event.shape_b, shape_b = 
event.shape_a, point_x = 
event.point_x, point_y = 
event.point_y, normal_x = -
event.normal_x, normal_y = -
event.normal_y, approach_speed = 
event.approach_speed }end end end;return 

result end












an.query_point = function(self, x, y, tags)if 
type(tags) == 'string' then tags = { tags }end;local result = 
{  }local _list_0 = 
physics_query_point(x, y, tags)for _index_0 = 1, #_list_0 do local body = _list_0[_index_0]local id = 
physics_get_user_data(body)local collider = 
self.colliders[id]if 
collider then
result[#result + 1] = collider.parent end end;return 
result end










an.query_circle = function(self, x, y, radius, tags)if 
type(tags) == 'string' then tags = { tags }end;local result = 
{  }local _list_0 = 
physics_query_circle(x, y, radius, tags)for _index_0 = 1, #_list_0 do local body = _list_0[_index_0]local id = 
physics_get_user_data(body)local collider = 
self.colliders[id]if 
collider then
result[#result + 1] = collider.parent end end;return 
result end










an.query_aabb = function(self, x, y, width, height, tags)if 
type(tags) == 'string' then tags = { tags }end;local result = 
{  }local _list_0 = 
physics_query_aabb(x, y, width, height, tags)for _index_0 = 1, #_list_0 do local body = _list_0[_index_0]local id = 
physics_get_user_data(body)local collider = 
self.colliders[id]if 
collider then
result[#result + 1] = collider.parent end end;return 
result end










an.query_box = function(self, x, y, width, height, angle, tags)if 
type(tags) == 'string' then tags = { tags }end;local result = 
{  }local _list_0 = 
physics_query_box(x, y, width, height, angle, tags)for _index_0 = 1, #_list_0 do local body = _list_0[_index_0]local id = 
physics_get_user_data(body)local collider = 
self.colliders[id]if 
collider then
result[#result + 1] = collider.parent end end;return 
result end










an.query_capsule = function(self, x1, y1, x2, y2, radius, tags)if 
type(tags) == 'string' then tags = { tags }end;local result = 
{  }local _list_0 = 
physics_query_capsule(x1, y1, x2, y2, radius, tags)for _index_0 = 1, #_list_0 do local body = _list_0[_index_0]local id = 
physics_get_user_data(body)local collider = 
self.colliders[id]if 
collider then
result[#result + 1] = collider.parent end end;return 
result end











an.query_polygon = function(self, x, y, vertices, tags)if 
type(tags) == 'string' then tags = { tags }end;local result = 
{  }local _list_0 = 
physics_query_polygon(x, y, vertices, tags)for _index_0 = 1, #_list_0 do local body = _list_0[_index_0]local id = 
physics_get_user_data(body)local collider = 
self.colliders[id]if 
collider then
result[#result + 1] = collider.parent end end;return 
result end












an.raycast = function(self, x1, y1, x2, y2, tags)if 
type(tags) == 'string' then tags = { tags }end;local hit = 
physics_raycast(x1, y1, x2, y2, tags)if 
hit then local id = 
physics_get_user_data(hit.body)local collider = 
self.colliders[id]if 
collider then return { object = 

collider.parent, shape = 
hit.shape, point_x = 
hit.point_x, point_y = 
hit.point_y, normal_x = 
hit.normal_x, normal_y = 
hit.normal_y, fraction = 
hit.fraction }end end;return 

nil end










an.raycast_all = function(self, x1, y1, x2, y2, tags)if 
type(tags) == 'string' then tags = { tags }end;local result = 
{  }local _list_0 = 
physics_raycast_all(x1, y1, x2, y2, tags)for _index_0 = 1, #_list_0 do local hit = _list_0[_index_0]local id = 
physics_get_user_data(hit.body)local collider = 
self.colliders[id]if 
collider then

result[#result + 1] = { object = collider.parent, shape = 
hit.shape, point_x = 
hit.point_x, point_y = 
hit.point_y, normal_x = 
hit.normal_x, normal_y = 
hit.normal_y, fraction = 
hit.fraction }end end;return 

result end


















update = function(dt)local all_objects = { 
an }local _list_0 = 
an:all()for _index_0 = 1, #_list_0 do local obj = _list_0[_index_0]all_objects[#all_objects + 1] = obj end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_early_update(dt)end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_update(dt)end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_late_update(dt)end;return 
an:cleanup()end