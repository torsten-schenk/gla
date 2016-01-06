local Printer = import("gla.textgen.Printer")
local IOSink = import("gla.textgen.IOSink")
local DumpSink = import("gla.textgen.DumpSink")
local EntityIO = import("gla.util.EntityIO")

//local io = EntityIO("<io>gla.stdio.stdout")
local io = EntityIO("<txt>tmp.asd")

local p = Printer()
local p2 = Printer()

p.pni("Hello world!")
p.embed(p2)
p.upn("And from first again")
p2.pn("From second")

p.commit(DumpSink())

p.commit(IOSink(io))

