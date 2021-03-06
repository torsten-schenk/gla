local BaseNode = import("gla.model.Node")
local BaseEdge = import("gla.model.Edge")
local BaseGraph = import("gla.model.Graph")
local BaseModel = import("gla.model.Model")
local SimpleTable = import("gla.storage.SimpleTable")

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
	//static meta: will be created in _inherited

	directory = null //managed and created by subclass; expected to be instance of gla.util.Directory

	nodes = null
	graphs = null
	
	rvnodes = null
	rvgraphs = null

	function new(Class, ...) {
		local object
		local args
		if(Class in meta.rvnode) {
			if(rvnodes == null)
				rvnodes = {}
			if(nodes == null)
				nodes = []
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
			object.source = source
			object.target = target
			graph.outgoing.insert([ source target Class object ])
			graph.incoming.insert([ target source Class object ])
			graph.rvedges[object] <- graph.edges.len()
			graph.edges.push(object)
		}
		else if(Class in meta.rvgraph) {
			if(rvgraphs == null)
				rvgraphs = {}
			if(graphs == null)
				graphs = []
			object = mkinstance(Class, vargv)
			object.outgoing = SimpleTable(4, 0)
			object.incoming = SimpleTable(4, 0)
			object.edges = []
			object.rvedges = {}
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
		if(directory != null) {
			print("")
			print("directory:")
			directory.dump()
		}
		print("")
		print("nodes:")
		foreach(i, v in nodes)
			print("  [" + i + " " + meta.rvnode[v.getclass()] + "] " + v)
		print("---------------")
	}
}

