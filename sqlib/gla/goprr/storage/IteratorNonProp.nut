local IteratorProp = import("gla.goprr.storage.IteratorProp")

const ROW_PROPERTY = 1
const ROW_EXPLOSION = 2

const ROW_OBJECT = 10
const ROW_GRAPH = 11
const ROW_RELATIONSHIP = 12
const ROW_ROLE = 13

return class extends import("gla.goprr.storage.Iterator") {
	metanps = null //a table from a MetaModel
	table = null
	rowtype = null

	constructor(metanps, table, rowtype) {
		local key = [ rowtype ]
		it = table.lower(key)
		begin = it.index
		end = table.upper(key).index
		current = array(2)
		this.metanps = metanps
		this.table = table
		this.rowtype = rowtype
	}

	function id() {
		return _getcurrent()[0]
	}

	function meta() {
		return metanps[_getcurrent()[1]]
	}

	function property(name) {
		local key = [ ROW_PROPERTY, id(), name ]
		if(table.exists(key))
			return table.value(key)
		else
			return null
	}

	function properties() {
		return IteratorProp(table, _getcurrent()[0])
	}

	function explosion() {
		if(rowtype != ROW_OBJECT)
			return null
		local key = [ ROW_EXPLOSION, id(), null ]
		if(table.exists(key))
			return table.value(key)
		else
			return null
	}

	function _updatecurrent() {
		current[0] = it.cell(1)
		current[1] = it.cell(3)
	}
}

