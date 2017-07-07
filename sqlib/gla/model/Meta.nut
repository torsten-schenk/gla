local strlib = import("squirrel.stringlib")
local Identifier = import("gla.string.Identifier")
local SimpleTable = import("gla.storage.SimpleTable")

local BaseNode = import("gla.model.Node")
local BaseEdge = import("gla.model.Edge")
local BaseGraph = import("gla.model.Graph")
local BaseModel = import("gla.model.Model")
local BaseParser = import("gla.model.Parser")
local BaseConverter = import("gla.model.Converter")
local BaseGenerator = import("gla.model.Generator")

enum RegType {
	Parser, Importer, Converter, Exporter, Generator
}

local registry = SimpleTable(3, 1) // [ type name1 name2 ] -> [ fullentity ]

local instances = {} //package -> Meta instance

local converters = {}

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

local newclass = function(Base, dir, pathname, slots, staticslots, attributes, byclass) {
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
	if(attributes != null)
		clazz.setattributes(null, attributes)
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
	super = null

	parsers = null
	importers = null
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

	constructor(package, super = null) { //TODO implement meta model inheritance or is it a useless, overengineered feature?
		this.package = package
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
/*	function borrowbasenode(other) {
}*/

	function mknode(pathname, slots = null, staticslots = null, attributes = null) {
		advance(Stage.AddNodeClasses)
		return newclass(Node, node, pathname, slots, staticslots, attributes, rvnode)
	}

	function mkedge(pathname, slots = null, staticslots = null, attributes = null) {
		advance(Stage.AddEdgeClasses)
		return newclass(Edge, edge, pathname, slots, staticslots, rvedge)
	}

	function mkgraph(pathname, slots = null, staticslots = null, attributes = null) {
		advance(Stage.AddGraphClasses)
		return newclass(Graph, graph, pathname, slots, staticslots, rvgraph)
	}

/*	function borrownode(other, pathname, mypathname = null) { //other: other meta instance; pathname: pathname in other meta; mypathname: pathname in this meta
	}*/

	function regparser(entity, name = "default") {
		advance(Stage.AddAdapters)
		registry.insert([ RegType.Parser package name ], package + "." + entity)
	}

	function regconverterto(tgtpackage, entity) {
		advance(Stage.AddAdapters)
		registry.insert([ RegType.Converter package tgtpackage ], package + "." + entity)
	}

	function regconverterfrom(srcpackage, entity) {
		advance(Stage.AddAdapters)
		registry.insert([ RegType.Converter srcpackage package ], package + "." + entity)
	}

	function reggenerator(entity, name = "default") {
		advance(Stage.AddAdapters)
		registry.insert([ RegType.Generator package name ], package + "." + entity)
	}

	function finish() {
		advance(Stage.Finished)
		assert(!(package in instances))
		instances[package] <- this
		return this
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

	function parser(type = "default", options = null) {
		if(type == null)
			type = "default"
		local entity = registry.tryValue([ RegType.Parser package type ])
		if(entity == null)
			return null
		local Parser = import(entity)
		local parser = Parser(options)
		assert(parser instanceof BaseParser)
		return parser
	}

	function converterto(tgtpackage, options = null) {
		local entity = registry.tryValue([ RegType.Converter package tgtpackage ])
		if(entity == null)
			return null
		local Converter = import(entity)
		local converter = Converter(options)
		assert(converter instanceof BaseConverter)
		return converter
	}

	function converterfrom(srcpackage, options = null) {
		local entity = registry.tryValue([ RegType.Converter srcpackage package ])
		if(entity == null)
			return null
		local Converter = import(entity)
		local converter = Converter(options)
		assert(converter instanceof BaseConverter)
		return converter
	}

	function generator(type = "default", options = null) {
		if(type == null)
			type = "default"
		local entity = registry.tryValue([ RegType.Generator package type ])
		if(entity == null)
			return null
		local Generator = import(entity)
		local generator = Generator(options)
		assert(generator instanceof BaseGenerator)
		return generator
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
		assert(this.package == package)
	}

	static function get(package) {
		return configure(package)
	}
}

return BaseMeta

