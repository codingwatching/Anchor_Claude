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
test_link_callback = function()
	return test("Link with callback (object survives)", function()
		an:add(object('shooter'))
		an:add(object('bullet'))
		an.bullet.homing = true
		an.bullet:link(an.shooter, function(self)
			self.homing = false
			return log("Callback ran, homing=" .. tostring(self.homing))
		end)
		log("bullet linked to shooter")
		an.shooter:kill()
		return log("shooter killed, bullet.dead=" .. tostring(an.bullet.dead) .. ", bullet.homing=" .. tostring(an.bullet.homing))
	end)
end
test_after_link_callback = function()
	return test("After link callback (bullet still alive)", function()
		log("bullet exists: " .. tostring(an.bullet ~= nil))
		log("bullet.dead: " .. tostring(an.bullet.dead))
		return log("shooter removed: " .. tostring(an.shooter == nil))
	end)
end
test_link_callback_kills = function()
	return test("Link with callback that kills self", function()
		an:add(object('owner'))
		an:add(object('pet'))
		an.pet.said_goodbye = false
		an.pet:link(an.owner, function(self)
			self.said_goodbye = true
			log("Pet says goodbye")
			return self:kill()
		end)
		log("pet linked to owner")
		an.owner:kill()
		return log("owner killed, pet.dead=" .. tostring(an.pet.dead) .. ", pet.said_goodbye=" .. tostring(an.pet.said_goodbye))
	end)
end
test_after_callback_kill = function()
	return test("After callback kill (both removed)", function()
		log("owner exists: " .. tostring(an.owner))
		return log("pet exists: " .. tostring(an.pet))
	end)
end
test_link_default = function()
	return test("Link without callback (default kill)", function()
		an:add(object('parent_obj'))
		an:add(object('child_obj'))
		an.child_obj:link(an.parent_obj)
		log("child_obj linked to parent_obj (no callback)")
		an.parent_obj:kill()
		return log("parent_obj killed, child_obj.dead=" .. tostring(an.child_obj.dead))
	end)
end
test_after_default_kill = function()
	return test("After default kill (both removed)", function()
		log("parent_obj exists: " .. tostring(an.parent_obj))
		return log("child_obj exists: " .. tostring(an.child_obj))
	end)
end
test_circular_links = function()
	return test("Circular links", function()
		an:add(object('node_a'))
		an:add(object('node_b'))
		an.node_a:link(an.node_b)
		an.node_b:link(an.node_a)
		log("node_a and node_b linked to each other")
		an.node_a:kill()
		return log("node_a killed, node_a.dead=" .. tostring(an.node_a.dead) .. ", node_b.dead=" .. tostring(an.node_b.dead))
	end)
end
test_after_circular = function()
	return test("After circular kill (both removed)", function()
		log("node_a exists: " .. tostring(an.node_a))
		return log("node_b exists: " .. tostring(an.node_b))
	end)
end
test_link_cleanup = function()
	return test("Link cleanup when linker dies", function()
		an:add(object('target'))
		an:add(object('linker'))
		an.linker:link(an.target, function(self)
			return log("This should not run")
		end)
		log("target.linked_from count: " .. tostring(#an.target.linked_from))
		an.linker:kill()
		return log("linker killed (not target)")
	end)
end
test_after_linker_cleanup = function()
	return test("After linker cleanup (linked_from cleaned)", function()
		log("target exists: " .. tostring(an.target ~= nil))
		log("target.linked_from count: " .. tostring(an.target.linked_from and #an.target.linked_from or 0))
		return an.target:kill()
	end)
end
test_T_alias = function()
	return test("T alias (object)", function()
		local o = T('alias_test')
		an:add(o)
		log("T created object: " .. tostring(an.alias_test ~= nil))
		return log("name: " .. tostring(an.alias_test.name))
	end)
end
test_Y_alias = function()
	return test("Y alias (set)", function()
		an.alias_test:Y({
			x = 100,
			y = 200,
			hp = 50
		})
		return log("x=" .. tostring(an.alias_test.x) .. ", y=" .. tostring(an.alias_test.y) .. ", hp=" .. tostring(an.alias_test.hp))
	end)
end
test_U_alias = function()
	return test("U alias (build)", function()
		an.alias_test:U(function(self)
			self.speed = self.x + self.y
			self.ready = true
		end)
		return log("speed=" .. tostring(an.alias_test.speed) .. ", ready=" .. tostring(an.alias_test.ready))
	end)
end
test_A_alias = function()
	return test("A alias (add)", function()
		an.alias_test:A(object('child_a'))
		return log("child added: " .. tostring(an.alias_test.child_a ~= nil))
	end)
end
test_action_aliases = function()
	return test("E, X, L aliases (actions)", function()
		an:add(object('action_alias_test'))
		an.action_alias_test.order = { }
		an.action_alias_test:E(function()
			do
				local _obj_0 = an.action_alias_test.order
				_obj_0[#_obj_0 + 1] = 'E'
			end
			return true
		end)
		an.action_alias_test:X(function()
			do
				local _obj_0 = an.action_alias_test.order
				_obj_0[#_obj_0 + 1] = 'X'
			end
			return true
		end)
		an.action_alias_test:L(function()
			do
				local _obj_0 = an.action_alias_test.order
				_obj_0[#_obj_0 + 1] = 'L'
			end
			return true
		end)
		return log("Actions added via aliases")
	end)
end
test_after_action_aliases_wait = function()
	return test("After action aliases (waiting)", function()
		return log("Letting E, X, L run...")
	end)
end
test_after_action_aliases = function()
	return test("After action aliases (order check)", function()
		log("Order: " .. tostring(table.concat(an.action_alias_test.order, ', ')))
		log("Expected: E, X, L")
		return an.action_alias_test:kill()
	end)
end
test_F_alias = function()
	return test("F alias (flow_to)", function()
		local o = T('flow_test')
		o:Y({
			x = 50
		})
		o:F(an)
		log("flow_to worked: " .. tostring(an.flow_test ~= nil))
		return log("x=" .. tostring(an.flow_test.x))
	end)
end
test_K_alias = function()
	return test("K alias (link)", function()
		an:add(object('link_target'))
		an:add(object('link_source'))
		an.link_source.survived = false
		an.link_source:K(an.link_target, function(self)
			self.survived = true
		end)
		an.link_target:kill()
		return log("link callback ran: " .. tostring(an.link_source.survived))
	end)
end
test_after_K_alias = function()
	return test("After K alias (cleanup)", function()
		log("link_target removed: " .. tostring(an.link_target == nil))
		log("link_source survived: " .. tostring(an.link_source ~= nil))
		an.link_source:kill()
		an.alias_test:kill()
		return an.flow_test:kill()
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
		return test_link_callback()
	elseif frame == 17 then
		test_after_link_callback()
		return test_link_callback_kills()
	elseif frame == 18 then
		test_after_callback_kill()
		return test_link_default()
	elseif frame == 19 then
		test_after_default_kill()
		return test_circular_links()
	elseif frame == 20 then
		test_after_circular()
		return test_link_cleanup()
	elseif frame == 21 then
		return test_after_linker_cleanup()
	elseif frame == 22 then
		test_T_alias()
		test_Y_alias()
		return test_U_alias()
	elseif frame == 23 then
		test_A_alias()
		return test_action_aliases()
	elseif frame == 24 then
		return test_after_action_aliases_wait()
	elseif frame == 25 then
		return test_after_action_aliases()
	elseif frame == 26 then
		return test_F_alias()
	elseif frame == 27 then
		return test_K_alias()
	elseif frame == 28 then
		return test_after_K_alias()
	elseif frame == 29 then
		return test_final()
	end
end)
