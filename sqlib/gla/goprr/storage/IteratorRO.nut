return class extends import("gla.goprr.storage.Iterator") {

	constructor(table, relationship, metarole) {
		local key
		if(metarole == null)
			key = [ relationship ]
		else
			key = [ relationship, metarole.name ]
		it = table.lower(key)
		begin = it.index
		end = table.upper(key).index
		current = array(3)
	}

	function metarole() {
		return _getcurrent()[0]
	}

	function role() {
		return _getcurrent()[1]
	}

	function object() {
		return _getcurrent()[2]
	}

	function _updatecurrent() {
		current[0] = it.cell(1)
		current[1] = it.cell(2)
		current[2] = it.cell(3)
	}
}

