require('game.object')
an = object('an')
update = function(dt)
	local all_objects = {
		an
	}
	local _list_0 = an:all()
	for _index_0 = 1, #_list_0 do
		local obj = _list_0[_index_0]
		all_objects[#all_objects + 1] = obj
	end
	for _index_0 = 1, #all_objects do
		local obj = all_objects[_index_0]
		obj:_early_update(dt)
	end
	for _index_0 = 1, #all_objects do
		local obj = all_objects[_index_0]
		obj:_update(dt)
	end
	for _index_0 = 1, #all_objects do
		local obj = all_objects[_index_0]
		obj:_late_update(dt)
	end
	return an:cleanup()
end
