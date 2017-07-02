local GraphmlLoader = import("gla.graphml.Loader")
local PackagePath = import("gla.io.PackagePath")

if(rt.arg.len() < 1 || rt.arg.len() > 2)
	throw "Usage: <graphml-entity> [e][n]"

local path = PackagePath(rt.arg[0], true)
local dumpedata = true
local dumpndata = true
if(rt.arg.len() >= 2) {
	dumpndata = rt.arg[1].find("n") != null
	dumpedata = rt.arg[1].find("e") != null
}


local NodeParser = class {
	function parsed(ndata, gdata) {
		if(dumpndata)
			rt.dump.value(ndata)
	}
}

local EdgeParser = class {
	function parsed(edata) {
		if(dumpedata)
			rt.dump.value(edata)
	}
}

GraphmlLoader.simpleLoad(path, NodeParser, EdgeParser, {}, false)

