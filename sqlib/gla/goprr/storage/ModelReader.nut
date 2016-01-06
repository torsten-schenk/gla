local IteratorRO = import("gla.goprr.storage.IteratorRO")
local IteratorRR = import("gla.goprr.storage.IteratorRR")
local IteratorNonProp = import("gla.goprr.storage.IteratorNonProp")
local IteratorProp = import("gla.goprr.storage.IteratorProp")
local IteratorBinding = import("gla.goprr.storage.IteratorBinding")

//TODO constants: use $include after preprocessor has been created
const ROW_PROPERTY = 1
const ROW_EXPLOSION = 2

const ROW_OBJECT = 10
const ROW_GRAPH = 11
const ROW_RELATIONSHIP = 12
const ROW_ROLE = 13

return class {
	meta = null
	db = null
	global = null
	locals = null
	bindr = null
	bindo = null

	constructor(db) { //NOTE: if meta == null, it must be set later before invoking any read operation
		local metaversion
		local metaentity
		local it
		this.db = db
		this.locals = {}
		this.bindr = {}
		this.bindo = {}

		global = db.open("global", "r")
		it = global.lower([ ROW_MODELINFO ])
		if(it.cell(0) != ROW_MODELINFO)
			throw "Missing modelinfo in database"
		metaversion = it.cell(3)
		metaentity = it.cell(2)
		meta = import(metaentity)
		if(meta.version != metaversion)
			throw "Incompatible model version in database"
	}

	function metanp(id, graph = 0) {
		local key = [ 0, id, null ]
		if(graph == 0) {
			key[0] = ROW_OBJECT
			if(global.exists(key))
				return meta.object[global.value(key)]
			key[0] = ROW_GRAPH
			return meta.graph[global.value(key)]
		}
		else {
			local table = _getlocal(graph)
			key[0] = ROW_ROLE
			if(table.exists(key))
				return meta.role[table.value(key)]
			key[0] = ROW_RELATIONSHIP
			return meta.relationship[table.value(key)]
		}
	}

	function get(id, name, graph = 0) { //id == 0: get a model property
		if(graph == 0)
			return global.tryValue([ ROW_PROPERTY, id, name ])
		else
			return _getlocal(graph).tryValue([ ROW_PROPERTY, id, name ])
	}

	function explosion(object) {
		return global.value([ ROW_EXPLOSION, object, null])
	}

	function objects() {
		return IteratorNonProp(meta.object, global, ROW_OBJECT)
	}

	function graphs() {
		return IteratorNonProp(meta.graph, global, ROW_GRAPH)
	}

	function roles(graph) {
		return IteratorNonProp(meta.role, _getlocal(graph), ROW_ROLE)
	}

	function relationships(graph) {
		return IteratorNonProp(meta.relationship, _getlocal(graph), ROW_RELATIONSHIP)
	}

	function relbindings(graph, relationship = 0) {
		return IteratorBinding(meta.role, _getbindr(graph), relationship)
	}

	function objbindings(graph, object = 0) {
		return IteratorBinding(meta.role, _getbindo(graph), object)
	}

	function isbound(object, graph) {
		local table = _getbindo(graph)
		return table.lower([ object ]).index < table.upper([ object ]).index
	}

	function hasroot(graph) {
		local table = _getbindr(graph)
		return table.lower([ 0 ]).index < table.upper([ 0 ]).index
	}

	function properties(id, graph = 0) {
		if(graph == 0)
			return IteratorProp(global, id)
		else
			return IteratorProp(_getlocal(graph), id)
	}

	//NOTE: the functions below just return the FIRST entry in the database.
	//returns: [ role id, relationship id ]
	function obj2rel(object, metarole, metarelationship, graph) {
		local table = _getbindo(graph)
		return table.tryValue([ object, metarole.name ])
	}

	//returns: [ role id, object id ]
	function rel2obj(relationship, metarole, graph) {
		local table = _getbindr(graph)
		return table.tryValue([ relationship, metarole.name ])
	}

	//iterate all roles/objects for a relationship in a given metarole
	function objectsin(graph, relationship, metarole = null) {
		return IteratorRO(_getbindr(graph), relationship, metarole)
	}

	//iterate all roles/relationships for an object in a given metarole
	function relationshipsfor(graph, object, metarole = null) {
		return IteratorRR(_getbindo(graph), object, metarole)
	}

	function _getlocal(graph) {
		if(!(graph in locals))
			locals[graph] <- db.open("local_" + graph, "r")
		return locals[graph]
	}

	function _getbindr(graph) {
		if(!(graph in bindr))
			bindr[graph] <- db.open("bindr_" + graph, "r")
		return bindr[graph]
	}

	function _getbindo(graph) {
		if(!(graph in bindo))
			bindo[graph] <- db.open("bindo_" + graph, "r")
		return bindo[graph]
	}
}

