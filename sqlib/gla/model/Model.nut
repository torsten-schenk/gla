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
	graphs = null
	
	rvnodes = null
	rvgraphs = null

	directory = null
	meta = null

	//only called when model is created via Model().
	//i.e.: constructor will not be called when model is deserialized.
	//TODO serialization
	constructor(diropts = null) {
		nodes = []
		graphs = []
		rvnodes = {}
		rvgraphs = {}
		directory = Directory(diropts)
		meta = this.getclass().getattributes(null).meta
	}

	function new(Class, ...) {
		local object
		local args
		if(Class in meta.rvnode) {
			object = mkinstance(Class, vargv)
			rvnodes[object] <- nodes.len()
			nodes.push(object)
		}
		else if(Class in meta.rvedge) {
			local graph = vargv[0]
			local source = vargv[1]
			local target = vargv[2]
			if(!(source in rvnodes))
				throw "source node does not belong to model"
			else if(!(target in rvnodes))
				throw "target node does not belong to model"
			else if(!(graph in rvgraphs))
				throw "graph doesn not belong to model"
			graph.checklink(Class, source, target)
			object = mkinstance(Class, vargv, 3)
			graph.outgoing.insert([ source target Class object ])
			graph.outgoing.insert([ target source Class object ])
			//TODO also keep track of all edges using 'edges' and 'rvedges'?
		}
		else if(Class in meta.rvgraph) {
			object = mkinstance(Class, vargv)
			rvgraphs[object] <- graphs.len()
			graphs.push(object)
		}
		else
			throw "invalid class: given class is neither a registered node, edge nor graph"
		return object
	}

	function owns(object) {
		return (object in rvnodes) || (object in rvgraphs) //TODO also edges?
	}

	function dump() {
		print("---- Model ----")
		print("package: " + meta.package)
		print("")
		print("directory:")
		directory.dump()
		print("")
		print("nodes:")
		foreach(i, v in nodes)
			print("  [" + i + " " + meta.rvnode[v.getclass()] + "] " + v)
		print("---------------")
	}
}

