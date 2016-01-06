local walkerdef = import("gla.bindb.model._import_xml")
local XmlWalker = import("gla.xml.model.Walker")
local Binder = import("gla.goprr.Binder")
local stringlib = import("squirrel.stringlib")
local meta = import("gla.bindb.model.meta")
local xmlmeta = import("gla.xml.model.meta")
local strlib = import("squirrel.stringlib")

local RegisterEntry = class {
	metaobject = null
	object = null //Struct, Class or Group
	assigns = null	

	constructor(metaobject, object) {
		this.metaobject = metaobject
		this.object = object
	}
}

local Resolver = class {
	namespace = null
	name = null

	function resolve(context) {
		local searchns = topath(namespace)
		local searchname = topath(name)
		local register = context.register
		local creator = context.creator
		local fullname = name
		local ismain = false
		local isforeign = false

		if(searchname.db != null) { //absolute path given, don't search namespace
			if(searchname.db != searchns.db) {
				if(searchname.db == "")
					ismain = true
				else
					isforeign = true
			}
			fullname = name
		}
		else if(!(fullname in register)) { //relative path, no primitive
			for(local i = searchns.path.len(); i >= 0; i--) {
				fullname = searchns.db + ":"
				for(local k = 0; k < i; k++) {
					if(k > 0)
						fullname += "."
					fullname += searchns.path[k]
				}
				if(i > 0)
					fullname += "."
				fullname += name
				if(fullname in register)
					break
			}
			if(!(fullname in register) && searchns.db != "") {
				ismain = true
				fullname = ":" + name
			}
		}
		if(!(fullname in register))
			throw "Error resolving '" + name + "' within namespace '" + namespace + "': not found"
		_bind(context, register[fullname], ismain, isforeign)
	}

	function topath(string) {
		local split = strlib.split(string, ":")
		local result = {}
		result.path <- strlib.split(split[split.len() - 1], ".")
		if(string[0] == ':')
			result.db <- ""
		else if(split.len() == 1)
			result.db <- null
		else
			result.db <- split[0]
		return result
	}
}

local InheritanceResolver = class extends Resolver {
	metaobject = null
	source = null

	constructor(metaobject, source, namespace, name) {
		this.metaobject = metaobject
		this.source = source
		this.namespace = namespace
		this.name = name
	}

	function _bind(context, entry, ismain, isforeign) {
		assert(entry.metaobject == metaobject)
		Binder.ImmediateNoRole(context.creator, context.graph_inheritance, source, meta.role.Source, meta.relationship.inherits, meta.role.Target, entry.object)
	}
}

local AssignmentResolver = class extends Resolver {
	object = null

	constructor(object, namespace, name) {
		this.object = object
		this.namespace = namespace
		this.name = name
	}

	function _bind(context, entry, ismain, isforeign) {
		local creator = context.creator
		local graph = context.graph_types
		if(isforeign)
			throw "Foreign database type assignment not allowed"
		else if(ismain && entry.metaobject != meta.object.Struct)
			throw "Main database type assignment not allowed for non-struct"
		if(entry.assigns == null) {
			entry.assigns = creator.new(meta.relationship.assigns, graph)
			creator.bind(entry.assigns, meta.role.Source, 0, entry.object, graph)
		}
		creator.bind(entry.assigns, meta.role.Target, 0, object, graph)
	}
}

function processNamespace(node, metaobject, object) {
	local context = node.context
	local creator = context.creator
	local register = context.register
	local name = attribute("name")
	local entry = RegisterEntry(metaobject, object)
	local fullname
	creator.set(context.composed, "name", name)
	if("binder_ns_nests" in context) {
		fullname = context.namespace + "." + name
		context.binder_ns_nests.bind(object)
	}
	else {
		fullname = context.namespace + name
		context.binder_ns_owns.bind(object)
	}
	if(fullname in register)
		throw "Multiple declarations of '" + fullname + "'"
	register[fullname] <- entry
	context.namespace <- fullname
}

function processInheritance(node, metaobject, object) {
	local context = node.context
	local creator = context.creator
	local super = node.attribute("extends")
	if(super == null)
		return
	context.resolvers.push(InheritanceResolver(metaobject, object, context.namespace, super))
}

function processType(node, object) {
	local context = node.context
	local creator = context.creator
	local type = node.attribute("type")
	context.resolvers.push(AssignmentResolver(object, context.namespace, type))
}

return class extends XmlWalker </ nodes = walkerdef.nodes, edges = walkerdef.edges /> {
	constructor(reader, creator) {
		creator.initMeta(meta)
		XmlWalker.constructor.call(this, reader, { creator = creator })
	}

	Root = class extends XmlWalker.Element {
		function enter() {
			local creator = context.creator
			context.register <- {} //Struct, Class and Group; key: fullname (path separated by '.'), value: RegisterEntry
			context.resolvers <- []
			context.graph_ns <- creator.new(meta.graph.Namespace)
			context.graph_inheritance <- creator.new(meta.graph.Inheritance)
			context.graph_types <- creator.new(meta.graph.Types)
			context.graph_data <- creator.new(meta.graph.Data)
			creator.set(0, "namespace", context.graph_ns)
			creator.set(0, "data", context.graph_data)
			creator.set(0, "types", context.graph_types)
			creator.set(0, "inheritance", context.graph_inheritance)
		}
		function leave() {
			foreach(v in context.resolvers)
				v.resolve(context)
		}
	}

	Primitive = class extends XmlWalker.Element {
		function run() {
			local creator = context.creator
			local name = attribute("name")
			local object = creator.new(meta.object.Primitive)
			creator.set(object, "name", name)
			context.register[name] <- RegisterEntry(meta.object.Primitive, object)
		}
	}

	MainDatabase = class extends XmlWalker.Element {
		function enter() {
			local creator = context.creator
			context.dbmain <- creator.new(meta.object.MainDatabase)
			context.namespace <- ":"
			creator.bind(0, meta.role.Target, 0, context.dbmain, context.graph_ns)
			context.binder_ns_embeds <- Binder.OnRequestNoRole(creator, context.graph_ns, context.dbmain, meta.role.Source, meta.relationship.embeds, meta.role.Target)
			context.binder_ns_owns <- Binder.OnRequestNoRole(creator, context.graph_ns, context.dbmain, meta.role.Source, meta.relationship.owns, meta.role.Target)
		}
	}

	AuxDatabase = class extends XmlWalker.Element {
		function enter() {
			local name = attribute("name")
			local creator = context.creator
			context.dbaux <- creator.new(meta.object.AuxDatabase)
			creator.set(context.dbaux, "name", name)
			context.binder_ns_embeds.bind(context.dbaux)
			context.namespace <- name + ":"
			context.binder_ns_owns <- Binder.OnRequestNoRole(creator, context.graph_ns, context.dbaux, meta.role.Source, meta.relationship.owns, meta.role.Target)
		}
	}

	Struct = class extends XmlWalker.Element {
		function enter() {
			local creator = context.creator
			context.composed <- creator.new(meta.object.Struct)
			processNamespace(this, meta.object.Struct, context.composed)
			processInheritance(this, meta.object.Struct, context.composed)
			context.binder_data_consists <- Binder.OnRequestNoRole(creator, context.graph_data, context.composed, meta.role.Source, meta.relationship.consists, meta.role.Target)
		}
		function leave() {
			delete context.namespace
			delete context.binder_data_consists
		}
	}

	Class = class extends XmlWalker.Element {
		function enter() {
			local creator = context.creator
			local abstract = attribute("abstract")
			context.composed <- creator.new(meta.object.Class)
			if(abstract != null && abstract)
				creator.set(context.composed, "abstract", true)
			else {
				creator.set(context.composed, "abstract", false)
				creator.set(context.composed, "typeid", attribute("typeid").tointeger())
			}
			processNamespace(this, meta.object.Class, context.composed)
			processInheritance(this, meta.object.Class, context.composed)
			context.binder_data_consists <- Binder.OnRequestNoRole(creator, context.graph_data, context.composed, meta.role.Source, meta.relationship.consists, meta.role.Target)
			context.binder_ns_nests <- Binder.OnRequestNoRole(creator, context.graph_ns, context.composed, meta.role.Source, meta.relationship.nests, meta.role.Target)
		}
		function leave() {
			delete context.namespace
			delete context.binder_ns_nests
			delete context.binder_data_consists
		}
	}

	Value = class extends XmlWalker.Element {
		function run() {
			local creator = context.creator
			local name = attribute("name")
			local typename = attribute("type")
			local deflt = attribute("default")
			local object = creator.new(meta.object.Value)
			creator.set(object, "name", name)
			if(deflt != null)
				creator.set(object, "default", deflt)
			context.binder_data_consists.bind(object)
			processType(this, object)
		}
	}

	Vector = class extends XmlWalker.Element {
		function run() {
			local creator = context.creator
			local name = attribute("name")
			local typename = attribute("type")
			local object = creator.new(meta.object.Vector)
			creator.set(object, "name", name)
			context.binder_data_consists.bind(object)
			processType(this, object)
		}
	}

	Array = class extends XmlWalker.Element {
		function run() {
			local creator = context.creator
			local name = attribute("name")
			local typename = attribute("type")
			local object = creator.new(meta.object.Array)
			local deflt = attribute("default")
			local size = attribute("size")
			creator.set(object, "name", name)
			creator.set(object, "size", size)
			if(deflt != null)
				creator.set(object, "default", deflt)
			context.binder_data_consists.bind(object)
			processType(this, object)
		}
	}

	Group = class extends XmlWalker.Element {
		function enter() {
			local creator = context.creator
			context.composed <- creator.new(meta.object.Group)
			processNamespace(this, meta.object.Group, context.composed)
			context.binder_data_defines <- Binder.OnRequestNoRole(creator, context.graph_data, context.composed, meta.role.Source, meta.relationship.defines, meta.role.Target)
			context.flagbit <- 0
			context.enumvalue <- 0
		}
		function leave() {
			delete context.namespace
			delete context.binder_data_defines
		}
	}

	Enum = class extends XmlWalker.Element {
		function run() {
			local creator = context.creator
			local name = attribute("name")
			local value = attribute("value")
			local object = creator.new(meta.object.Enum)
			creator.set(object, "name", name)
			if(value != null)
				context.enumvalue = value.tointeger()
			creator.set(object, "value", context.enumvalue)
			context.binder_data_defines.bind(object)
			context.enumvalue++
		}
	}

	Flag = class extends XmlWalker.Element {
		function run() {
			local creator = context.creator
			local name = attribute("name")
			local bit = attribute("bit")
			local object = creator.new(meta.object.Flag)
			creator.set(object, "name", name)
			if(bit != null)
				context.flagbit = bit.tointeger()
			creator.set(object, "bit", context.flagbit)
			context.binder_data_defines.bind(object)
			context.flagbit++
		}
	}

	Integer = class extends XmlWalker.Element {
		function run() {
			local creator = context.creator
			local name = attribute("name")
			local value = attribute("value")
			local object = creator.new(meta.object.Integer)
			creator.set(object, "name", name)
			creator.set(object, "value", value.tointeger())
			context.binder_data_defines.bind(object)
		}
	}
}

