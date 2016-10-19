//uses a <graphml> entity as input and generates a walker definition <nut> entity from it

local Loader = import("gla.graphml.Loader")
local PackagePath = import("gla.io.PackagePath")

local NodeParser = class {
	function parsed(node, graph) {
		rt.dump.value(node)
	}
}

local EdgeParser = class {
	function parsed(edge) {
	}
}

if(rt.arg.len() != 2)
	throw "Invalid argument count"

local path = PackagePath(rt.arg[0], true)
path.extdefault("graphml")

Loader.simpleLoad(path, NodeParser, EdgeParser, null, false)

