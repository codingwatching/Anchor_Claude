

























require('anchor.object')
require('anchor.layer')
require('anchor.image')
require('anchor.font')
require('anchor.timer')
require('anchor.math')
require('anchor.collider')
require('anchor.spring')
require('anchor.camera')
require('anchor.shake')
require('anchor.random')
require('anchor.color')
require('anchor.array')return function(config)if 


config == nil then config = {  }end;if 

config.width and config.height then
engine_set_game_size(config.width, config.height)end;if 
config.title then
engine_set_title(config.title)end;if 
config.scale then
engine_set_scale(config.scale)end;if 
config.vsync ~= nil then
engine_set_vsync(config.vsync)end;if 
config.fullscreen ~= nil then
engine_set_fullscreen(config.fullscreen)end;if 
config.resizable ~= nil then
engine_set_resizable(config.resizable)end;if 
config.filter then
set_filter_mode(config.filter)end


engine_init()
















an = object('an')
an.layers = {  }
an.images = {  }
an.fonts = {  }
an.shaders = {  }
an.sounds = {  }
an.tracks = {  }
an:add(random())


an.width = engine_get_width()
an.height = engine_get_height()
an.dt = engine_get_dt()
an.platform = engine_get_platform()














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















an.sound = function(self, name, path)
self.sounds[name] = sound_load(path)return 
self.sounds[name]end














an.sound_play = function(self, name, volume, pitch)if volume == nil then volume = 1 end;if pitch == nil then pitch = 1 end;return 
sound_play(self.sounds[name], volume, pitch)end







an.sound_set_volume = function(self, volume)return 
sound_set_volume(volume)end















an.music = function(self, name, path)
self.tracks[name] = music_load(path)return 
self.tracks[name]end














an.music_play = function(self, name, loop, channel)if loop == nil then loop = false end;if channel == nil then channel = 0 end;if 

self.crossfade_state and (channel == self.crossfade_state.from_channel or channel == self.crossfade_state.to_channel) then
music_set_volume(self.crossfade_state.original_from_volume, self.crossfade_state.from_channel)
music_set_volume(self.crossfade_state.original_to_volume, self.crossfade_state.to_channel)
self.crossfade_state = nil end;return 
music_play(self.tracks[name], loop, channel)end









an.music_stop = function(self, channel)if channel == nil then channel = -1 end
music_stop(channel)if 

self.crossfade_state then
music_set_volume(self.crossfade_state.original_from_volume, self.crossfade_state.from_channel)
music_set_volume(self.crossfade_state.original_to_volume, self.crossfade_state.to_channel)
self.crossfade_state = nil end;if 

channel == -1 then
self.playlist_channel = 0 end end













an.music_set_volume = function(self, volume, channel)if channel == nil then channel = -1 end;return 
music_set_volume(volume, channel)end



















an.music_crossfade = function(self, name, duration, loop)if loop == nil then loop = false end;local from_channel = 

self.playlist_channel;local to_channel = 
1 - self.playlist_channel;local original_from_volume = 


music_get_volume(from_channel)local original_to_volume = 
music_get_volume(to_channel)


music_set_volume(0, to_channel)
music_play(self.tracks[name], loop, to_channel)



self.crossfade_state = { duration = duration, time = 
0, from_channel = 
from_channel, to_channel = 
to_channel, original_from_volume = 
original_from_volume, original_to_volume = 
original_to_volume }end



an.playlist = {  }
an.playlist_index = 1
an.playlist_shuffled = {  }
an.playlist_shuffle_enabled = false
an.playlist_crossfade_duration = 0
an.playlist_channel = 0
an.playlist_just_advanced = false










an.playlist_set = function(self, tracks)
self.playlist = tracks
self.playlist_index = 1
self.playlist_shuffled = {  }if 
self.playlist_shuffle_enabled then return self:playlist_generate_shuffle()end end







an.playlist_play = function(self)if #
self.playlist == 0 then return end;local track = 
self:playlist_current_track()if 
self.playlist_crossfade_duration > 0 then
self:music_crossfade(track, self.playlist_crossfade_duration)else

self:music_play(track, false, self.playlist_channel)end
self.playlist_just_advanced = true end







an.playlist_stop = function(self)
self:music_stop()
self.playlist_channel = 0 end







an.playlist_next = function(self)if #
self.playlist == 0 then return end
self.playlist_index = (self.playlist_index % #self.playlist) + 1;return 
self:playlist_play()end







an.playlist_prev = function(self)if #
self.playlist == 0 then return end
self.playlist_index = ((self.playlist_index - 2) % #self.playlist) + 1;return 
self:playlist_play()end












an.playlist_shuffle = function(self, enabled)
self.playlist_shuffle_enabled = enabled;if 
enabled then return 
self:playlist_generate_shuffle()else

self.playlist_shuffled = {  }end end








an.playlist_set_crossfade = function(self, duration)
self.playlist_crossfade_duration = duration end


an.playlist_generate_shuffle = function(self)
self.playlist_shuffled = {  }
local indices;do local _accum_0 = {  }local _len_0 = 1;for i = 1, #self.playlist do _accum_0[_len_0] = i;_len_0 = _len_0 + 1 end;indices = _accum_0 end;while #
indices > 0 do local index = 
self.random:int(1, #indices)do local _obj_0 = 
self.playlist_shuffled;_obj_0[#_obj_0 + 1] = indices[index]end
table.remove(indices, index)end end


an.playlist_current_track = function(self)if 
self.playlist_shuffle_enabled and #self.playlist_shuffled > 0 then return 
self.playlist[self.playlist_shuffled[self.playlist_index]]else return 

self.playlist[self.playlist_index]end end


an:early_action('crossfade', function(self, dt)if not 
self.crossfade_state then return end;local crossfade = 
self.crossfade_state
crossfade.time = crossfade.time + dt;if 

crossfade.time >= crossfade.duration then

music_set_volume(1, crossfade.to_channel)
music_stop(crossfade.from_channel)

music_set_volume(crossfade.original_from_volume, crossfade.from_channel)

self.playlist_channel = crossfade.to_channel
self.crossfade_state = nil else local progress = 


crossfade.time / crossfade.duration
music_set_volume(1 - progress, crossfade.from_channel)return 
music_set_volume(progress, crossfade.to_channel)end end)


an:early_action('playlist_auto_advance', function(self, dt)if #
self.playlist == 0 then return end;if 

self.playlist_just_advanced then
self.playlist_just_advanced = false
return end;if 

music_at_end(self.playlist_channel) and not music_is_playing(self.playlist_channel) then

self.playlist_index = (self.playlist_index % #self.playlist) + 1;if 

self.playlist_index == 1 and self.playlist_shuffle_enabled then
self:playlist_generate_shuffle()end;return 
self:playlist_play()end end)


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




















an.bind = function(self, action, control)return 
input_bind(action, control)end







an.unbind = function(self, action, control)return 
input_unbind(action, control)end







an.unbind_all = function(self, action)return 
input_unbind_all(action)end









an.bind_all = function(self)return 
input_bind_all()end









an.bind_chord = function(self, name, actions)return 
input_bind_chord(name, actions)end











an.bind_sequence = function(self, name, sequence)return 
input_bind_sequence(name, sequence)end









an.bind_hold = function(self, name, duration, source_action)return 
input_bind_hold(name, duration, source_action)end








an.is_pressed = function(self, action)return 
is_pressed(action)end








an.is_down = function(self, action)return 
is_down(action)end








an.is_released = function(self, action)return 
is_released(action)end








an.get_axis = function(self, negative, positive)return 
input_get_axis(negative, positive)end











an.get_vector = function(self, left, right, up, down)return 
input_get_vector(left, right, up, down)end










an.get_hold_duration = function(self, name)return 
input_get_hold_duration(name)end








an.any_pressed = function(self)return 
input_any_pressed()end









an.get_pressed_action = function(self)return 
input_get_pressed_action()end








an.key_is_down = function(self, key)return 
key_is_down(key)end








an.key_is_pressed = function(self, key)return 
key_is_pressed(key)end








an.key_is_released = function(self, key)return 
key_is_released(key)end







an.mouse_position = function(self)return 
mouse_position()end







an.mouse_delta = function(self)return 
mouse_delta()end








an.mouse_wheel = function(self)return 
mouse_wheel()end










an.mouse_is_down = function(self, button)return 
mouse_is_down(button)end








an.mouse_is_pressed = function(self, button)return 
mouse_is_pressed(button)end








an.mouse_is_released = function(self, button)return 
mouse_is_released(button)end







an.mouse_set_visible = function(self, visible)return 
mouse_set_visible(visible)end







an.mouse_set_grabbed = function(self, grabbed)return 
mouse_set_grabbed(grabbed)end








an.gamepad_is_connected = function(self)return 
gamepad_is_connected()end










an.gamepad_get_axis = function(self, axis)return 
gamepad_get_axis(axis)end










an.get_last_input_type = function(self)return 
input_get_last_type()end








an.start_capture = function(self)return 
input_start_capture()end










an.get_captured = function(self)return 
input_get_captured()end







an.stop_capture = function(self)return 
input_stop_capture()end







an.set_deadzone = function(self, deadzone)return 
input_set_deadzone(deadzone)end




















update = function(dt)

an.frame = engine_get_frame()
an.step = engine_get_step()
an.time = engine_get_time()
an.window_width, an.window_height = engine_get_window_size()
an.scale = engine_get_scale()
an.fullscreen = engine_is_fullscreen()
an.fps = engine_get_fps()
an.draw_calls = engine_get_draw_calls()for name, layer in 


pairs(an.layers) do if 
layer.camera then
layer.camera:attach(layer, layer.parallax_x, layer.parallax_y)end end;local all_objects = { 

an }local _list_0 = 
an:all()for _index_0 = 1, #_list_0 do local obj = _list_0[_index_0]all_objects[#all_objects + 1] = obj end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_early_update(dt)end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_update(dt)end;for _index_0 = 
1, #all_objects do local obj = all_objects[_index_0]obj:_late_update(dt)end
an:cleanup()for name, layer in 


pairs(an.layers) do if 
layer.camera then
layer.camera:detach(layer)end end end end