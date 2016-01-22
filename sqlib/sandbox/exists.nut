local PackagePath = import("gla.io.PackagePath")

local path = PackagePath("sandbox")
path.select("exists", "nut")
print("Path: " + path.tostring())
print("This should be true: " + path.exists())
path.unselect()
path.select("notexists", "nex")
print("This should be false: " + path.exists())
