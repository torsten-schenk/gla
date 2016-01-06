local IteratorProp = import("gla.goprr.storage.IteratorProp")

const ROW_PROPERTY = 1
const ROW_EXPLOSION = 2

const ROW_OBJECT = 10
const ROW_GRAPH = 11
const ROW_RELATIONSHIP = 12
const ROW_ROLE = 13

return class extends import("gla.goprr.storage.Iterator") {
	metaroles = null
	table = null

	constructor(metaroles, table, owner) {
		if(owner == 0) {
			it = table.begin()
			begin = it.index
			end = table.end().index
		}
		else {
			local key = [ owner ]
			it = table.lower(key)
			begin = it.index
			end = table.upper(key).index
		}
		current = array(4)
		this.metaroles = metaroles
		this.table = table
	}

	function source() { //relationship or object id (equals 'owner' in case owner in ctor is != 0)
		return _getcurrent()[0]
	}

	function target() { //object or relationship
		return _getcurrent()[3]
	}

	function metarole() {
		return metaroles[_getcurrent()[1]]
	}

	function role() {
		return _getcurrent()[2]
	}

	function _updatecurrent() {
		current[0] = it.cell(0)
		current[1] = it.cell(1)
		current[2] = it.cell(2)
		current[3] = it.cell(3)
	}
}

