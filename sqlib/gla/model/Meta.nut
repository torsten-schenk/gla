local strlib = import("squirrel.stringlib")
local Identifier = import("gla.string.Identifier")

local BaseNode = import("gla.model.Node")
local BaseEdge = import("gla.model.Edge")
local BaseGraph = import("gla.model.Graph")
local BaseModel = import("gla.model.Model")
local BaseParser = import("gla.model.Parser")

local instances = {} //package -> Meta instance

local BaseMeta
local configure = function(package) {
	if(package in instances) 
		return instances[package]
	else {
		local meta = import(package + ".meta")
		assert(meta instanceof BaseMeta)
		assert(meta.package == package)
		instances[package] <- meta
		return meta
	}
}

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
	Initialized,
	SetNodeBaseClass,
	AddNodeClasses,
	SetEdgeBaseClass,
	AddEdgeClasses,
	SetGraphBaseClass,
	AddGraphClasses
	SetModelBaseClass,
	AddAdapters,
	Finished
}

BaseMeta = class {
	package = null

	parsers = null
	importers = null
	converters = null
	exporters = null
	generators = null

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

	stage = Stage.Initialized

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
		local cur = clazz
		while(cur != null) {
			if(cur == BaseNode) {
				advance(Stage.SetNodeBaseClass)
				Node = clazz
				return
			}
			else if(cur == BaseEdge) {
				advance(Stage.SetEdgeBaseClass)
				Edge = clazz
				return
			}
			else if(cur == BaseGraph) {
				advance(Stage.SetGraphBaseClass)
				Graph = clazz
				return
			}
			else if(cur == BaseModel) {
				advance(Stage.SetModelBaseClass)
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
		throw "error setting base class: base class does not inherit any of Node, Edge or Graph"
	}

	function mknode(pathname, slots = null, staticslots = null) {
		advance(Stage.AddNodeClasses)
		return newclass(Node, node, pathname, slots, staticslots, rvnode)
	}

	function mkedge(pathname, slots = null, staticslots = null) {
		advance(Stage.AddEdgeClasses)
		return newclass(Edge, edge, pathname, slots, staticslots, rvedge)
	}

	function mkgraph(pathname, slots = null, staticslots = null) {
		advance(Stage.AddGraphClasses)
		return newclass(Graph, graph, pathname, slots, staticslots, rvgraph)
	}

	function regparser(entity, name = "default") {
		advance(Stage.AddAdapters)
		if(parsers == null)
			parsers = {}
		parsers[name] <- entity
	}

	function regconverter(package, entity, name = "default") {
		advance(Stage.AddAdapters)
		if(converters == null)
			converters = {}
		if(!(package in converters))
			converters[package] <- {}
		converters[package][name] <- entity
	}

	function finish() {
		advance(Stage.Finished)
	}

	function new(...) {
		advance(Stage.Finished)
		local object = Model.instance()
		if("constructor" in object) {
			local args = [ object ]
			args.extend(vargv)
			Model.constructor.acall(args)
		}
		return object
	}

	function get(package) {
		return configure(package)
	}

	function advance(target) {
		if(stage > target)
			throw "invalid operation: expected stage already finished"
		while(stage < target) {
			switch(stage) {
				case Stage.Initialized:
					stage++
					break
				case Stage.SetNodeBaseClass:
					stage++
					break
				case Stage.SetEdgeBaseClass:
					stage++
					break
				case Stage.SetGraphBaseClass:
					stage++
					break
				case Stage.AddNodeClasses:
					stage++
					break
				case Stage.AddEdgeClasses:
					stage++
					break
				case Stage.AddGraphClasses:
					stage++
					break
				case Stage.SetModelBaseClass:
					if(Model == null)
						Model = class extends BaseModel </ meta = this /> {}
					stage++
					break
				case Stage.AddAdapters:
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

	static function parser(package, options = null, type = "default") {
		local meta = configure(package)
		if(meta.parsers == null || !(type in meta.parsers))
			return null
		local Parser = import(package + "." + meta.parsers[type])
		local parser = Parser(options)
		assert(parser instanceof BaseParser)
		return parser
	}
}

return BaseMeta

