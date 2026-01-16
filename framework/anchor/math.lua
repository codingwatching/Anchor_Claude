
















PI = math.pi
PI2 = math.pi / 2
LN2 = math.log(2)
LN210 = 10 * math.log(2)


overshoot = 1.70158


amplitude = 1
period = 0.0003
















math.lerp = function(t, source, destination)return 
source * (1 - t) + destination * t end





















math.lerp_dt = function(p, t, dt, source, destination)return 
math.lerp(1 - (1 - p) ^ (dt / t), source, destination)end



















math.loop = function(t, length)return 
math.clamp(t - math.floor(t / length) * length, 0, length)end



















math.lerp_angle = function(t, source, destination)local dt = 
math.loop(destination - source, 2 * math.pi)if 
dt > math.pi then dt = dt - 2 * math.pi end;return 
source + dt * math.clamp(t, 0, 1)end




















math.lerp_angle_dt = function(p, t, dt, source, destination)return 
math.lerp_angle(1 - (1 - p) ^ (dt / t), source, destination)end
















math.clamp = function(value, min, max)if 
value < min then return min elseif 
value > max then return max else return 
value end end


math.linear = function(t)return t end


math.sine_in = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else return 
1 - math.cos(t * PI2)end end

math.sine_out = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else return 
math.sin(t * PI2)end end

math.sine_in_out = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else return -
0.5 * (math.cos(t * PI) - 1)end end

math.sine_out_in = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 elseif 
t < 0.5 then return 0.5 * math.sin(t * 2 * PI2)else return -
0.5 * math.cos((t * 2 - 1) * PI2) + 1 end end


math.quad_in = function(t)return t * t end

math.quad_out = function(t)return -t * (t - 2)end

math.quad_in_out = function(t)if 
t < 0.5 then return 
2 * t * t else

t = t - 1;return -
2 * t * t + 1 end end

math.quad_out_in = function(t)if 
t < 0.5 then
t = t * 2;return -
0.5 * t * (t - 2)else

t = t * 2 - 1;return 
0.5 * t * t + 0.5 end end


math.cubic_in = function(t)return t * t * t end

math.cubic_out = function(t)
t = t - 1;return 
t * t * t + 1 end

math.cubic_in_out = function(t)
t = t * 2;if 
t < 1 then return 
0.5 * t * t * t else

t = t - 2;return 
0.5 * (t * t * t + 2)end end

math.cubic_out_in = function(t)
t = t * 2 - 1;return 
0.5 * (t * t * t + 1)end


math.quart_in = function(t)return t * t * t * t end

math.quart_out = function(t)
t = t - 1
t = t * t;return 
1 - t * t end

math.quart_in_out = function(t)
t = t * 2;if 
t < 1 then return 
0.5 * t * t * t * t else

t = t - 2
t = t * t;return -
0.5 * (t * t - 2)end end

math.quart_out_in = function(t)if 
t < 0.5 then
t = t * 2 - 1
t = t * t;return -
0.5 * t * t + 0.5 else

t = t * 2 - 1
t = t * t;return 
0.5 * t * t + 0.5 end end


math.quint_in = function(t)return t * t * t * t * t end

math.quint_out = function(t)
t = t - 1;return 
t * t * t * t * t + 1 end

math.quint_in_out = function(t)
t = t * 2;if 
t < 1 then return 
0.5 * t * t * t * t * t else

t = t - 2;return 
0.5 * t * t * t * t * t + 1 end end

math.quint_out_in = function(t)
t = t * 2 - 1;return 
0.5 * (t * t * t * t * t + 1)end


math.expo_in = function(t)if 
t == 0 then return 0 else return 
math.exp(LN210 * (t - 1))end end

math.expo_out = function(t)if 
t == 1 then return 1 else return 
1 - math.exp(-LN210 * t)end end

math.expo_in_out = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else

t = t * 2;if 
t < 1 then return 0.5 * math.exp(LN210 * (t - 1))else return 
0.5 * (2 - math.exp(-LN210 * (t - 1)))end end end

math.expo_out_in = function(t)if 
t < 0.5 then return 0.5 * (1 - math.exp(-20 * LN2 * t))elseif 
t == 0.5 then return 0.5 else return 
0.5 * (math.exp(20 * LN2 * (t - 1)) + 1)end end


math.circ_in = function(t)if 
t < -1 or t > 1 then return 0 else return 
1 - math.sqrt(1 - t * t)end end

math.circ_out = function(t)if 
t < 0 or t > 2 then return 0 else return 
math.sqrt(t * (2 - t))end end

math.circ_in_out = function(t)if 
t < -0.5 or t > 1.5 then return 0.5 else

t = t * 2;if 
t < 1 then return -0.5 * (math.sqrt(1 - t * t) - 1)else

t = t - 2;return 
0.5 * (math.sqrt(1 - t * t) + 1)end end end

math.circ_out_in = function(t)if 
t < 0 then return 0 elseif 
t > 1 then return 1 elseif 
t < 0.5 then
t = t * 2 - 1;return 
0.5 * math.sqrt(1 - t * t)else

t = t * 2 - 1;return -
0.5 * ((math.sqrt(1 - t * t) - 1) - 1)end end


math.bounce_in = function(t)
t = 1 - t;if 
t < 1 / 2.75 then return 1 - 7.5625 * t * t elseif 
t < 2 / 2.75 then
t = t - 1.5 / 2.75;return 
1 - (7.5625 * t * t + 0.75)elseif 
t < 2.5 / 2.75 then
t = t - 2.25 / 2.75;return 
1 - (7.5625 * t * t + 0.9375)else

t = t - 2.625 / 2.75;return 
1 - (7.5625 * t * t + 0.984375)end end

math.bounce_out = function(t)if 
t < 1 / 2.75 then return 7.5625 * t * t elseif 
t < 2 / 2.75 then
t = t - 1.5 / 2.75;return 
7.5625 * t * t + 0.75 elseif 
t < 2.5 / 2.75 then
t = t - 2.25 / 2.75;return 
7.5625 * t * t + 0.9375 else

t = t - 2.625 / 2.75;return 
7.5625 * t * t + 0.984375 end end

math.bounce_in_out = function(t)if 
t < 0.5 then
t = 1 - t * 2;if 
t < 1 / 2.75 then return (1 - 7.5625 * t * t) * 0.5 elseif 
t < 2 / 2.75 then
t = t - 1.5 / 2.75;return (
1 - (7.5625 * t * t + 0.75)) * 0.5 elseif 
t < 2.5 / 2.75 then
t = t - 2.25 / 2.75;return (
1 - (7.5625 * t * t + 0.9375)) * 0.5 else

t = t - 2.625 / 2.75;return (
1 - (7.5625 * t * t + 0.984375)) * 0.5 end else

t = t * 2 - 1;if 
t < 1 / 2.75 then return 7.5625 * t * t * 0.5 + 0.5 elseif 
t < 2 / 2.75 then
t = t - 1.5 / 2.75;return (
7.5625 * t * t + 0.75) * 0.5 + 0.5 elseif 
t < 2.5 / 2.75 then
t = t - 2.25 / 2.75;return (
7.5625 * t * t + 0.9375) * 0.5 + 0.5 else

t = t - 2.625 / 2.75;return (
7.5625 * t * t + 0.984375) * 0.5 + 0.5 end end end

math.bounce_out_in = function(t)if 
t < 0.5 then
t = t * 2;if 
t < 1 / 2.75 then return 7.5625 * t * t * 0.5 elseif 
t < 2 / 2.75 then
t = t - 1.5 / 2.75;return (
7.5625 * t * t + 0.75) * 0.5 elseif 
t < 2.5 / 2.75 then
t = t - 2.25 / 2.75;return (
7.5625 * t * t + 0.9375) * 0.5 else

t = t - 2.625 / 2.75;return (
7.5625 * t * t + 0.984375) * 0.5 end else

t = 1 - (t * 2 - 1)if 
t < 1 / 2.75 then return 0.5 - 7.5625 * t * t * 0.5 + 0.5 elseif 
t < 2 / 2.75 then
t = t - 1.5 / 2.75;return 
0.5 - (7.5625 * t * t + 0.75) * 0.5 + 0.5 elseif 
t < 2.5 / 2.75 then
t = t - 2.25 / 2.75;return 
0.5 - (7.5625 * t * t + 0.9375) * 0.5 + 0.5 else

t = t - 2.625 / 2.75;return 
0.5 - (7.5625 * t * t + 0.984375) * 0.5 + 0.5 end end end


math.back_in = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else return 
t * t * ((overshoot + 1) * t - overshoot)end end

math.back_out = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else

t = t - 1;return 
t * t * ((overshoot + 1) * t + overshoot) + 1 end end

math.back_in_out = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else

t = t * 2;if 
t < 1 then return 0.5 * (t * t * ((overshoot * 1.525 + 1) * t - overshoot * 1.525))else

t = t - 2;return 
0.5 * (t * t * ((overshoot * 1.525 + 1) * t + overshoot * 1.525) + 2)end end end

math.back_out_in = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 elseif 
t < 0.5 then
t = t * 2 - 1;return 
0.5 * (t * t * ((overshoot + 1) * t + overshoot) + 1)else

t = t * 2 - 1;return 
0.5 * t * t * ((overshoot + 1) * t - overshoot) + 0.5 end end


math.elastic_in = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else

t = t - 1;return - (
amplitude * math.exp(LN210 * t) * math.sin((t * 0.001 - period / 4) * (2 * PI) / period))end end

math.elastic_out = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else return 
math.exp(-LN210 * t) * math.sin((t * 0.001 - period / 4) * (2 * PI) / period) + 1 end end

math.elastic_in_out = function(t)if 
t == 0 then return 0 elseif 
t == 1 then return 1 else

t = t * 2;if 
t < 1 then
t = t - 1;return -
0.5 * (amplitude * math.exp(LN210 * t) * math.sin((t * 0.001 - period / 4) * (2 * PI) / period))else

t = t - 1;return 
amplitude * math.exp(-LN210 * t) * math.sin((t * 0.001 - period / 4) * (2 * PI) / period) * 0.5 + 1 end end end

math.elastic_out_in = function(t)if 
t < 0.5 then
t = t * 2;if 
t == 0 then return 0 else return (
amplitude / 2) * math.exp(-LN210 * t) * math.sin((t * 0.001 - period / 4) * (2 * PI) / period) + 0.5 end else if 

t == 0.5 then return 0.5 elseif 
t == 1 then return 1 else

t = t * 2 - 1
t = t - 1;return - ((
amplitude / 2) * math.exp(LN210 * t) * math.sin((t * 0.001 - period / 4) * (2 * PI) / period)) + 0.5 end end end