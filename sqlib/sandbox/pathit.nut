local PackagePath = import("gla.io.PackagePath")

local p = PackagePath()
for(p.down(); p.isValid(); p.next())
	print(p.tostring())
p.up()
for(p.select(); p.isValid(); p.next())
	print(p.tostring())
p.unselect()

p = PackagePath("sandbox.hello", true)
local e = PackagePath(p, true)
run(e)

