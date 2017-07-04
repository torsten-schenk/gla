local Directory = import("gla.util.Directory")
local BaseNode = import("gla.model.Node")
local BaseEdge = import("gla.model.Edge")
local BaseGraph = import("gla.model.Graph")
local BaseModel = import("gla.model.Model")

local mkinstance = function(Class, vargs, argoff = 0) {
	local object = Class.instance()
	if("constructor" in Class) {
		local args = array(1 + vargs.len() - argoff)
		args[0] = object
		for(local i = argoff; i < vargs.len(); i++)
			args[i - argoff + 1] = vargs[i]
		Class.constructor.acall(args)
	}
	return object
}

return class {
	nodes = null
	directory = null
	meta = null

	constructor(diropts = null) {
		nodes = []
		directory = Directory(diropts)
		meta = this.getclass().getattributes(null).meta
	}

	function new(Class, ...) {
		local object
		local args
		if(Class in meta.rvnode) {
			object = mkinstance(Class, vargv)
			nodes.push(object)
		}
		else if(Class in meta.rvedge) {
			local graph = vargv[0]
			local source = vargv[1]
			local target = vargv[2]
			object = mkinstance(Class, vargv, 3)
		}
		else if(Class in meta.rvgraph) {
		}
		else
			throw "invalid class: given class is neither a registered node, edge nor graph"
		return object
	}

	function dump() {
		print("---- Model ----")
		print("package: " + meta.package)
		print("")
		print("directory:")
		directory.dump()
		print("")
		print("nodes:")
		foreach(i, v in nodes) {
			print("  [" + i + " " + meta.rvnode[v.getclass()] + "] " + v)
		}
		print("---------------")
	}
}

