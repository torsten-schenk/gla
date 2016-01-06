local PackagePath = import("gla.io.PackagePath")

local a = PackagePath("tmp.bla")
local b = clone(a)
b.ascend()
b.descend("blubb")
print("A: " + a.tostring() + " B: " + b.tostring())


local path = PackagePath("tmp.blablubb")
print("PATH: " + path.tostring())
path.touch()
path.select("Hello", "nut")

path.touch()

local io = path.open("w")
io.write("Hello world!", "string")
io.close()

path = PackagePath("<nut>tmp.Hello", true)
print("PATH: " + path.tostring())

