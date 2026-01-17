









































array = {  }

















array.all = function(t, f)if not 
t then return nil end;for i = 
1, #t do if not 
f(t[i], i) then return false end end;return 
true end

















array.any = function(t, f)if not 
t then return nil end;for i = 
1, #t do if 
f(t[i], i) then return true end end;return 
false end














array.average = function(t)if #
t == 0 then return nil end;local sum = 
0;for _index_0 = 
1, #t do local v = t[_index_0]sum = sum + v end;return 
sum / #t end



















array.count = function(t, v)if not 
v then return #t end;if 
type(v) == 'function' then local count = 
0;for i = 
1, #t do if 
v(t[i], i) then count = count + 1 end end;return 
count else local count = 

0;for i = 
1, #t do if 
t[i] == v then count = count + 1 end end;return 
count end end















array.delete = function(t, v)local count = 
0;for i = #
t, 1, -1 do if 
t[i] == v then
table.remove(t, i)
count = count + 1 end end;return 
count end





















array.flatten = function(t, level)if level == nil then level = 1000 end;if not 
t then return nil end;local out = 
{  }local stack = 
{  }local levels = 
{  }for i = #
t, 1, -1 do
stack[#stack + 1] = t[i]
levels[#levels + 1] = 0 end;while #
stack > 0 do local v = 
table.remove(stack)local current_level = 
table.remove(levels)if 
type(v) == 'table' and not getmetatable(v) and current_level < level then for i = #
v, 1, -1 do
stack[#stack + 1] = v[i]
levels[#levels + 1] = current_level + 1 end else

out[#out + 1] = v end end;return 
out end
























array.get = function(t, i, j)if not 
i then return nil end;if 
i < 0 then i = #t + i + 1 end;if not 
j then return t[i]end;if 
j < 0 then j = #t + j + 1 end;if 
i == j then return t[i]end;local out = 
{  }
local step;if j > i then step = 1 else step = -1 end;for k = 
i, j, step do
out[#out + 1] = t[k]end;return 
out end

















array.get_circular_buffer_index = function(t, i)if not 
t then return nil end;if #
t == 0 then return nil end;return ((
i - 1) % #t) + 1 end
















array.has = function(t, v)if not (
v and #t >= 1) then return false end;if 
type(v) == 'function' then for i = 
1, #t do if 
v(t[i]) then return true end end else for i = 

1, #t do if 
t[i] == v then return true end end end;return 
false end
















array.index = function(t, v)if not (
v and #t >= 1) then return nil end;if 
type(v) == 'function' then for i = 
1, #t do if 
v(t[i]) then return i end end else for i = 

1, #t do if 
t[i] == v then return i end end end;return 
nil end















array.join = function(t, separator)if separator == nil then separator = ''end;local s = 
''for i = 
1, #t do
s = s .. tostring(t[i])if 
i < #t then s = s .. separator end end;return 
s end
















array.max = function(t, f)if #
t == 0 then return nil end;if 
f then local max_val = 
f(t[1])local max_elem = 
t[1]for i = 
2, #t do local val = 
f(t[i])if 
val > max_val then
max_val = val
max_elem = t[i]end end;return 
max_elem else local max_elem = 

t[1]for i = 
2, #t do if 
t[i] > max_elem then max_elem = t[i]end end;return 
max_elem end end












array.print = function(t)local s = 
table.tostring(t)
print(s)return 
s end






















array.random = function(t, n, rng)if n == nil then n = 1 end;if #
t == 0 then return nil end
rng = rng or an.random;if 
n == 1 then return 
t[rng:int(1, #t)]else local out = 

{  }local selected = 
{  }while #
out < n and #selected < #t do local i = 
rng:int(1, #t)if not 
array.has(selected, i) then
selected[#selected + 1] = i
out[#out + 1] = t[i]end end;return 
out end end















array.remove = function(t, i)return 
table.remove(t, i)end





















array.remove_random = function(t, n, rng)if n == nil then n = 1 end;if #
t == 0 then return nil end
rng = rng or an.random;if 
n == 1 then return 
table.remove(t, rng:int(1, #t))else local out = 

{  }while #
out < n and #t > 0 do
out[#out + 1] = table.remove(t, rng:int(1, #t))end;return 
out end end

















array.reverse = function(t, i, j)if not 
t then return nil end
i = i or 1;if 
i < 0 then i = #t + i + 1 end
j = j or (#t)if 
j < 0 then j = #t + j + 1 end;if 
i == j then return t end;for k = 
0, math.floor((j - i) / 2) do
t[i + k], t[j - k] = t[j - k], t[i + k]end;return 
t end




















array.rotate = function(t, n)if not 
t then return nil end;if not 
n then return t end;if 
n < 0 then n = #t + n end
n = n % #t;if 
n == 0 then return t end
array.reverse(t, 1, #t)
array.reverse(t, 1, n)
array.reverse(t, n + 1, #t)return 
t end

















array.shuffle = function(t, rng)
rng = rng or an.random;for i = #
t, 2, -1 do local j = 
rng:int(1, i)
t[i], t[j] = t[j], t[i]end;return 
t end
















array.sum = function(t, f)local sum = 
0;if 
f then for _index_0 = 
1, #t do local v = t[_index_0]sum = sum + f(v)end else for _index_0 = 

1, #t do local v = t[_index_0]sum = sum + v end end;return 
sum end



















table.copy = function(t)if not (
type(t) == 'table') then return t end;local copy = 
{  }for k, v in 
next, t, nil do
copy[table.copy(k)] = table.copy(v)end;return 
copy end














table.tostring = function(t)
t = t or {  }if not (
type(t) == 'table') then return tostring(t)end;local s = 
'{'for k, v in 
pairs(t) do
local key;if type(k) == 'number' then key = k else key = '"' .. k .. '"'end
s = s .. ('[' .. key .. '] = ' .. table.tostring(v) .. ', ')end;if 
s ~= '{' then
s = s:sub(1, -3)end;return 
s .. '}'end