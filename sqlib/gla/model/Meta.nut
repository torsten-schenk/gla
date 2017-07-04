local strlib = import("squirrel.stringlib")
local Identifier = import("gla.string.Identifier")

local BaseNode = import("gla.model.Node")
local BaseEdge = import("gla.model.Edge")
local BaseGraph = import("gla.model.Graph")
local BaseModel = import("gla.model.Model")

local newclass = function(Base, dir, pathname, slots, staticslots, byclass) {
	local name 
	local err
	local path = strlib.split(pathname, ".")
	local cur = dir
	local cursuper = Base
	for(local i = 0; i < path.len() - 1; i++) {
		name = Identifier(path[i], Identifier.LowerUnderscore)
		err = name.validate()
		if(err != null)
			throw "invalid class name '" + name + "': " + err
		local supername = name.convert(Identifier.CapitalCamel)

		local super

		if(supername.tostring() in cur)
			cursuper = cur[supername.tostring()]
		else {
			cursuper = class extends cursuper {}
			cur[supername.tostring()] <- cursuper
		}

		if(!(name.tostring() in cur))
			cur[name.tostring()] <- {}
		cur = cur[name.tostring()]
	}

	name = Identifier(path[path.len() - 1], Identifier.CapitalCamel)
	err = name.validate()
	if(err != null)
		throw "invalid class name '" + name + "': " + err
	local clazz = class extends cursuper </ path = path, pathname = pathname /> {}
	if(slots != null)
		foreach(i, v in slots)
			clazz.newmember(i, v)
	if(staticslots != null)
		foreach(i, v in staticslots)
			clazz.newmember(i, v, null, true)
	cur[name.tostring()] <- clazz
	byclass[clazz] <- pathname
	return clazz
}

enum Stage {
	SetBaseClasses,
	AddClasses,
	SetModelClass,
	Finished
}

return class {
	package = null

	Node = null
	Edge = null
	Graph = null
	Model = null

	node = null
	edge = null
	graph = null

	rvnode = null
	rvedge = null
	rvgraph = null

	stage = Stage.SetBaseClasses

	constructor() {
		Node = BaseNode
		Edge = BaseEdge
		Graph = BaseGraph
		node = {}
		edge = {}
		graph = {}
		rvnode = {}
		rvedge = {}
		rvgraph = {}
	}

	function setbase(clazz) {
		advance(Stage.SetBaseClasses)

		local cur = clazz
		while(cur != null) {
			if(cur == BaseNode) {
				Node = clazz
				return
			}
			else if(cur == BaseEdge) {
				Edge = clazz
				return
			}
			else if(cur == BaseGraph) {
				Graph = clazz
				return
			}
			else
				cur = cur.getbase()
		}
		throw "error setting base class: base class does not inherit any of Node, Edge or Graph"
	}

	function mkgraph(pathname, slots = null, staticslots = null) {
		advance(Stage.AddClasses)
		return newclass(Graph, graph, pathname, slots, staticslots, rvgraph)
	}

	function mkedge(pathname, slots = null, staticslots = null) {
		advance(Stage.AddClasses)
		return newclass(Edge, edge, pathname, slots, staticslots, rvedge)
	}

	function mknode(pathname, slots = null, staticslots = null) {
		advance(Stage.AddClasses)
		return newclass(Node, node, pathname, slots, staticslots, rvnode)
	}

	function setmodel(clazz) {
		advance(Stage.SetModelClass)

		local cur = clazz
		while(cur != null) {
			if(cur == BaseModel) {
				if(clazz.getattributes(null) == null)
					clazz.setattributes(null, { meta = this })
				else if("meta" in clazz.getattributes(null))
					throw "model class already belongs to a different meta model"
				else
					clazz.getattributes(null).meta <- this
				Model = clazz
				return
			}
			else
				cur = cur.getbase()
		}
		throw "error setting model class: class does not inherit Model"
	}

	function finish() {
		advance(Stage.Finished)
	}

	function advance(target) {
		if(stage > target)
			throw "invalid operation: expected stage already finished"
		while(stage < target) {
			switch(stage) {
				case Stage.SetBaseClasses:
					stage++
					break
				case Stage.AddClasses:
					stage++
					break
				case Stage.SetModelClass:
					if(Model == null)
						Model = class extends BaseModel </ meta = this /> {}
					stage++
					break
				default:
					assert(false)
			}
		}
	}

	function _export(package, entity) {
		this.package = package
	}
}

