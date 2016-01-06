const ROW_PROPERTY = 1
const ROW_EXPLOSION = 2

const ROW_OBJECT = 10
const ROW_GRAPH = 11
const ROW_RELATIONSHIP = 12
const ROW_ROLE = 13

return class extends import("gla.goprr.storage.Iterator") {
	table = null

	constructor(table, id) {
		local key = [ ROW_PROPERTY, id ]
		it = table.lower(key)
		begin = it.index
		end = table.upper(key).index
		assert(begin <= end)
		current = array(2)
		this.table = table
	}

	function name() {
		return _getcurrent()[0]
	}

	function value() {
		return _getcurrent()[1]
	}

	function _updatecurrent() {
		current[0] = it.cell(2)
		current[1] = it.cell(3)
	}
}

