

require('anchor')

an:layer('game')
an:font('main', 'assets/LanaPixel.ttf', 11)
an:image('smile', 'assets/slight_smile.png')


test_obj = object('test')
test_obj:add(timer())
an:add(test_obj)


test_obj.timer:after(0.5, function()return print('[0.5s] anonymous after fired')end)


test_obj.timer:after(1, 'named_after', function()return print('[1.0s] named after fired')end)


test_obj.timer:every(0.3, (function()return print('[every 0.3s] tick')end), 3, function()return print('[every 0.3s] done after 3 times')end)


test_obj.timer:every(0.2, 'to_cancel', function()return print('[every 0.2s] this should only print twice')end)


test_obj.timer:after(0.5, 'do_cancel', function()
print('[0.5s] cancelling to_cancel timer')return 
test_obj.timer:cancel('to_cancel')end)


test_obj.timer:after(2, 'replaceable', function()return print('[2.0s] FAIL - first timer should have been replaced')end)
test_obj.timer:after(2, 'replaceable', function()return print('[2.0s] replaced timer fired (expected)')end)


during_count = 0
test_obj.timer:during(0.5, (function(dt, progress)during_count = during_count + 1 end), function()return print("[during 0.5s] done, ran " .. tostring(during_count) .. " times")end)


tween_obj = { x = 0, y = 0 }
test_obj.timer:tween(1, tween_obj, { x = 100, y = 50 }, math.cubic_out, function()return print("[tween 1s] done, x=" .. tostring(tween_obj.x) .. ", y=" .. tostring(tween_obj.y))end)


tween_obj2 = { scale = 1 }
test_obj.timer:tween(0.5, 'scale_tween', tween_obj2, { scale = 2 }, math.quad_in_out, function()return print("[tween 0.5s] scale done, scale=" .. tostring(tween_obj2.scale))end)


test_obj.hp = 100
test_obj.timer:watch('hp', function(current, previous)return print("[watch] hp changed: " .. tostring(previous) .. " -> " .. tostring(current))end)
test_obj.timer:after(0.3, function()test_obj.hp = 80 end)
test_obj.timer:after(0.6, function()test_obj.hp = 50 end)


test_obj.danger = false
test_obj.timer:when((function()return test_obj.danger end), function()return print("[when] danger became true!")end)
test_obj.timer:after(0.4, function()test_obj.danger = true end)


every_step_count = 0
test_obj.timer:every_step(0.1, 0.3, 5, function()
every_step_count = every_step_count + 1;return 
print("[every_step] tick " .. tostring(every_step_count))end)


test_obj.timer:every(10, 'trigger_test', function()return print("[trigger] fired!")end)
test_obj.timer:after(0.2, function()
print("[0.2s] triggering trigger_test")return 
test_obj.timer:trigger('trigger_test')end)


test_obj.timer:after(1.5, 'time_check', function()return print("[1.5s] time_check fired")end)
test_obj.timer:after(0.7, function()local remaining = 
test_obj.timer:get_time_left('time_check')return 
print("[0.7s] time_check has " .. tostring(remaining) .. "s left")end)

an.angle = 0;return 

an:action(function(self, dt)
self.angle = self.angle + (dt * 2)local game = 
an.layers.game
game:rectangle(80, 80, 50, 50, rgba(255, 0, 0, 255))
game:circle(400, 80, 25, rgba(0, 255, 0, 255))
game:push(240, 135, self.angle, 0.1, 0.1)
game:image(an.images.smile, 0, 0)
game:pop()
game:text("Hello!", an.fonts.main, 240, 220, rgba(255, 255, 255, 255))return 
game:draw()end)