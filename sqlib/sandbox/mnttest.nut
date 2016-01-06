local PackagePath = import("gla.io.PackagePath")

local io = open(PackagePath("mnt.test.Ping", true), "w")
io.close()

