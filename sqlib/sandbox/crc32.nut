local HashFunction = import("gla.io.HashFunction")
local Buffer = import("gla.io.Buffer")

local buf = Buffer()
buf.write(1037, "u32")
local value = buf.hash(HashFunction.CRC32, "string")
print("HASH VALUE: " + value)
buf.rseek(0)
value = buf.hash(HashFunction.CRC32, "integer")
print("HASH VALUE: " + value)
value = buf.hash(HashFunction.CRC32, "invalid")

