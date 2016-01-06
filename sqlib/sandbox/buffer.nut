local Buffer = import("gla.io.Buffer")

local b = Buffer()
b.write(27, "u32")
b.write(66, "u32")
b.write(3.14159265359, "f64")

class Asd {
	v1 = 91
	v2 = 92
	v3 = 93

//	_serialize = 5
	function _serialize(io) {
		io.write(v1, "u32")
		io.write(v2, "u32")
		io.write(v3, "u32")
	}

	function _deserialize(io) {
		v1 = io.read("u32")
		v2 = io.read("u32")
		v3 = io.read("u32")
	}
}

local asd = Asd()
b.write(asd, "serializable")
b.write("Hello!", "string")

local b2 = Buffer()
b2.write(103, "u32")
print("BUFFER SIZE: " + b2.size())
//b2.read(b, "io")
b.write(b2, "io")

print("READ: " + b.read("u32") + " " + b.read("u32") + " " + b.read("f64") + " " + b.read("u32") + " " + b.read("u32") + " " + b.read("u32") + " " + b.read("string", 6) + " " + b.read("u32"))

b.write(57, "u32")
b.write(58, "u32")
b.write(59, "u32")
local asd2 = b.read(Asd, "serializable")
print("DESERIALIZED: " + asd2.v1 + " " + asd2.v2 + " " + asd2.v3)
print("BUFFER SIZE: " + b.size())
b.wseek(0)
b.write(67, "u32")
b.wseek(8)
b.write(69, "u32")
b.rseek(0)
local asd2 = b.read(Asd, "serializable")
print("DESERIALIZED: " + asd2.v1 + " " + asd2.v2 + " " + asd2.v3)
