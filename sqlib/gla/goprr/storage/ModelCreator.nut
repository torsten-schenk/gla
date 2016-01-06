local Object = import("gla.goprr.meta.Object")
local Graph = import("gla.goprr.meta.Graph")
local Role = import("gla.goprr.meta.Role")
local Relationship = import("gla.goprr.meta.Relationship")
local ColumnSpec = import("gla.storage.ColumnSpec")

//TODO constants: use $include after preprocessor has been created
const ROW_MODELINFO = 0
const ROW_PROPERTY = 1
const ROW_EXPLOSION = 2
const ROW_DECOMPOSITION = 3

const ROW_OBJECT = 10
const ROW_GRAPH = 11
const ROW_RELATIONSHIP = 12
const ROW_ROLE = 13

return class {
	meta = null
	db = null
	curgid = 1
	curlid = null
	curbid = null

	global = null //table for global instances (object/graph)
	locals = null //tables for local instances (role/relationship)
	bindr = null //tables for bindings: relationship to object
	bindo = null //table for bindings: object to relationship

	constructor(db, meta = null) {
		this.db = db
		local builder

		locals = {}
		bindr = {}
		bindo = {}
		curlid = {}
		curbid = {}

		builder = db.Builder(ColumnSpec([
			{ name = "rowtype", type = "integer" },
			{ name = "id", type = "integer" },
			{ name = "aux", type = null }
		], [
			{ name = "value", type = null }
		]))
		builder.multikey = true
		global = db.open("global", "ct", builder)
		if(meta != null)
			initMeta(meta)
	}

	function initMeta(meta) {
		if(this.meta != null)
			throw("Error initializing creator with metamodel: already set")
		this.meta = meta
		global.insert([ ROW_MODELINFO, 0, rt.entityof(meta) ], meta.version )
	}

	function new(metanp, graph = 0) {
		if(metanp instanceof Object) {
			assert(graph == 0)
			global.insert([ ROW_OBJECT, curgid, null ], metanp.name)
			return curgid++
		}
		else if(metanp instanceof Graph) {
			assert(graph == 0)
			global.insert([ ROW_GRAPH, curgid, null ], metanp.name)

			local builder
			builder = db.Builder(ColumnSpec([
				{ name = "rowtype", type = "integer" },
				{ name = "id", type = "integer" },
				{ name = "aux", type = null }
			], [
				{ name = "value", type = null }
			]))
			builder.multikey = true
			locals[curgid] <- db.open("local_" + curgid, "ct", builder)
			curlid[curgid] <- 1
			
			builder = db.Builder(ColumnSpec([
				{ name = "relationship", type = "integer" },
				{ name = "metarole", type = "string" },
			], [
				{ name = "role", type = "integer" },
				{ name = "object", type = "integer" }
			]))
			builder.multikey = true
			bindr[curgid] <- db.open("bindr_" + curgid, "ct", builder)

			builder = db.Builder(ColumnSpec([
				{ name = "object", type = "integer" },
				{ name = "metarole", type = "string" }
			], [
				{ name = "role", type = "integer" },
				{ name = "relationship", type = "integer" }
			]))
			builder.multikey = true
			bindo[curgid] <- db.open("bindo_" + curgid, "ct", builder)

			curbid[curgid] <- 1
			return curgid++
		}
		else if(metanp instanceof Role) {
			assert(graph != 0)
			assert(graph in curlid)
			locals[graph].insert([ ROW_ROLE, curlid[graph], null ], metanp.name)
			return curlid[graph]++
		}
		else if(metanp instanceof Relationship) {
			assert(graph != 0)
			assert(graph in curlid)
			locals[graph].insert([ ROW_RELATIONSHIP, curlid[graph], null ], metanp.name)
			return curlid[graph]++
		}
	}

	function set(id, name, value, graph = 0) { //id == 0: set a model property
		if(graph == 0)
			global.insert([ ROW_PROPERTY, id, name ], value)
		else
			locals[graph].insert([ ROW_PROPERTY, id, name ], value)
	}

	function bind(relationship, metarole, role, object, graph) {
		bindr[graph].insert([ relationship, metarole.name ], [ role, object ])
		bindo[graph].insert([ object, metarole.name ], [ role, relationship ])
	}

	function explode(object, graph) {
		global.insert([ ROW_EXPLOSION, object, null ], graph)
	}

	function decompose(object, metagraph, graph) {
		global.insert([ ROW_DECOMPOSITION, object, metagraph.name ], graph)
	}
}

