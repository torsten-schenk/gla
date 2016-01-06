local A = class {
	value = 5
}

local B = class extends A {
}

local C = class extends A {
}

C.newmember("constructor", function(arg) {
	print("CTOR " + arg)
})

local t = {
	value = 5
}

local u = {
}

u.setdelegate(t)
local a = C(8)
print(a.rawget("value"))
print(u.value)

