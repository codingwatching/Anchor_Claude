require('game.init')
frame = 0
test_num = 0
log = function(msg)
	return print("  " .. tostring(msg))
end
test = function(name, fn)
	test_num = test_num + 1
	print("\n=== Test " .. tostring(test_num) .. ": " .. tostring(name) .. " ===")
	return fn()
end
_anon_func_0 = function(list)
	local _accum_0 = { }
	local _len_0 = 1
	for _index_0 = 1, #list do
		local obj = list[_index_0]
		_accum_0[_len_0] = obj.name
		_len_0 = _len_0 + 1
	end
	return _accum_0
end
names = function(list)
	return table.concat(_anon_func_0(list), ', ')
end
test_complex_tree = function()
	return test("Complex tree (4 levels deep)", function()
		an:add(object('a'))
		an.a:add(object('b'))
		an.a.b:add(object('c'))
		an.a.b.c:add(object('d'))
		an.a:add(object('e'))
		an:add(object('f'))
		an.f:add(object('g'))
		an.f:add(object('h'))
		an.f.h:add(object('i'))
		an.f.h:add(object('j'))
		an:add(object('k'))
		log("All: " .. tostring(names(an:all())))
		log("Expected: a, b, c, d, e, f, g, h, i, j, k")
		return log("Count: " .. tostring(#an:all()))
	end)
end
test_bidirectional = function()
	return test("Bidirectional named links", function()
		log("an.a exists: " .. tostring(an.a ~= nil))
		log("an.a.an == an: " .. tostring(an.a.an == an))
		log("an.f.h.f == an.f: " .. tostring(an.f.h.f == an.f))
		return log("an.a.b.c.d.c == an.a.b.c: " .. tostring(an.a.b.c.d.c == an.a.b.c))
	end)
end
test_tags = function()
	return test("Tags and is() method", function()
		an.a:tag('enemy', 'flying')
		an.a.b:tag('enemy')
		an.f:tag('friendly')
		an.f.h:tag('enemy', 'boss')
		log("Enemies: " .. tostring(names(an:all('enemy'))))
		log("Flying: " .. tostring(names(an:all('flying'))))
		log("Bosses: " .. tostring(names(an:all('boss'))))
		log("a\\is 'enemy': " .. tostring(an.a:is('enemy')))
		return log("a\\is 'a': " .. tostring(an.a:is('a')))
	end)
end
test_kill_middle = function()
	return test("Kill middle of tree (b branch)", function()
		log("Before: " .. tostring(names(an:all())))
		an.a.b:kill()
		log("After kill (same frame): b.dead=" .. tostring(an.a.b.dead) .. ", c.dead=" .. tostring(an.a.b.c.dead))
		return log("a.dead: " .. tostring(an.a.dead) .. ", e.dead: " .. tostring(an.a.e.dead))
	end)
end
test_after_middle_cleanup = function()
	return test("After cleanup (middle branch removed)", function()
		log("All: " .. tostring(names(an:all())))
		return log("an.a.b: " .. tostring(an.a.b))
	end)
end
test_replacement = function()
	return test("Named child replacement", function()
		local old_g = an.f.g
		an.f:add(object('g'))
		log("Old g dead: " .. tostring(old_g.dead))
		return log("New g is different: " .. tostring(an.f.g ~= old_g))
	end)
end
test_kill_by_tag = function()
	return test("Kill by tag 'enemy'", function()
		log("Before: " .. tostring(names(an:all())))
		an:kill('enemy')
		return log("a.dead: " .. tostring(an.a.dead) .. ", f.dead: " .. tostring(an.f.dead) .. ", k.dead: " .. tostring(an.k.dead))
	end)
end
test_after_tag_kill = function()
	return test("After tag kill cleanup", function()
		return log("All: " .. tostring(names(an:all())))
	end)
end
test_oneshot_action = function()
	return test("One-shot action (returns true)", function()
		an:add(object('m'))
		local counter = 0
		an.m:action(function()
			counter = counter + 1
			log("One-shot ran, counter=" .. tostring(counter))
			return true
		end)
		return log("Action added, will run next frame")
	end)
end
test_after_oneshot = function()
	return test("After one-shot (should not run again)", function()
		return log("If counter is still 1, one-shot was removed")
	end)
end
test_named_action = function()
	return test("Named action", function()
		an.m:action('counter', function()
			an.m.count = (an.m.count or 0) + 1
			return log("Named action ran, count=" .. tostring(an.m.count))
		end)
		return log("an.m.counter exists: " .. tostring(an.m.counter ~= nil))
	end)
end
test_named_action_runs = function()
	return test("Named action runs each frame", function()
		return log("count=" .. tostring(an.m.count))
	end)
end
test_replace_action = function()
	return test("Replace named action", function()
		local old_action = an.m.counter
		an.m:action('counter', function()
			an.m.count2 = (an.m.count2 or 0) + 1
			return log("Replaced action ran, count2=" .. tostring(an.m.count2))
		end)
		return log("Action replaced: " .. tostring(an.m.counter ~= old_action))
	end)
end
test_replaced_runs = function()
	return test("Replaced action runs", function()
		return log("count2=" .. tostring(an.m.count2) .. ", count=" .. tostring(an.m.count) .. " (should stop incrementing)")
	end)
end
test_early_late = function()
	return test("Early and late actions", function()
		an:add(object('p'))
		an.p.order = { }
		an.p:early_action(function()
			local _obj_0 = an.p.order
			_obj_0[#_obj_0 + 1] = 'early'
		end)
		an.p:action(function()
			local _obj_0 = an.p.order
			_obj_0[#_obj_0 + 1] = 'main'
		end)
		an.p:late_action(function()
			local _obj_0 = an.p.order
			_obj_0[#_obj_0 + 1] = 'late'
		end)
		return log("Actions added, will run next frame")
	end)
end
test_order = function()
	return test("Action execution order", function()
		log("Order: " .. tostring(table.concat(an.p.order, ', ')))
		return log("Expected: early, main, late")
	end)
end
test_named_early_late = function()
	return test("Named early/late actions", function()
		an.p:early_action('my_early', function()
			an.p.early_count = (an.p.early_count or 0) + 1
		end)
		an.p:late_action('my_late', function()
			an.p.late_count = (an.p.late_count or 0) + 1
		end)
		log("an.p.my_early exists: " .. tostring(an.p.my_early ~= nil))
		return log("an.p.my_late exists: " .. tostring(an.p.my_late ~= nil))
	end)
end
test_named_early_late_run = function()
	return test("Named early/late run each frame", function()
		return log("early_count=" .. tostring(an.p.early_count) .. ", late_count=" .. tostring(an.p.late_count))
	end)
end
test_oneshot_early_late = function()
	return test("One-shot early/late actions", function()
		an.p:early_action(function()
			log("One-shot early ran")
			return true
		end)
		an.p:late_action(function()
			log("One-shot late ran")
			return true
		end)
		return log("One-shot early/late added")
	end)
end
test_after_oneshot_early_late = function()
	return test("After one-shot early/late", function()
		log("One-shots should have run once and been removed")
		return log("early_count=" .. tostring(an.p.early_count) .. ", late_count=" .. tostring(an.p.late_count))
	end)
end
test_final = function()
	return test("Final state", function()
		log("All: " .. tostring(names(an:all())))
		return print("\n=== All tests complete ===")
	end)
end
return an:action(function()
	frame = frame + 1
	if frame == 1 then
		return test_complex_tree()
	elseif frame == 2 then
		test_bidirectional()
		return test_tags()
	elseif frame == 3 then
		return test_kill_middle()
	elseif frame == 4 then
		test_after_middle_cleanup()
		return test_replacement()
	elseif frame == 5 then
		return test_kill_by_tag()
	elseif frame == 6 then
		test_after_tag_kill()
		return test_oneshot_action()
	elseif frame == 7 then
		test_after_oneshot()
		return test_named_action()
	elseif frame == 8 then
		return test_named_action_runs()
	elseif frame == 9 then
		return test_replace_action()
	elseif frame == 10 then
		return test_replaced_runs()
	elseif frame == 11 then
		return test_early_late()
	elseif frame == 12 then
		return test_named_early_late()
	elseif frame == 13 then
		test_order()
		return test_named_early_late_run()
	elseif frame == 14 then
		return test_oneshot_early_late()
	elseif frame == 15 then
		return test_after_oneshot_early_late()
	elseif frame == 16 then
		return test_final()
	end
end)
