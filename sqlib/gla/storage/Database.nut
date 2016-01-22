local cbridge = import("gla.storage._cbridge")

return class {
	_tables = null

	constructor(params = {}) { //TODO add another parameter: default params for 'newBuilder()'
		cbridge.database.ctor(this, params)
		_tables = {}
	}

	function open(name, mode = "", builder = null, magic = 0) {
		local table

		assert(typeof(name) == "string")
		if(name in _tables)
			table = _tables[name]
		else {
			table = cbridge.database.openTable(this, name, magic, mode, builder)
			_tables[name] <- table
		}

		table._refcount++
		return table
	}

	function close(name = null) {
		if(name == null) {
			cbridge.database.close(this)
			_tables = {}
		}
		else if(name in _tables) {
			local table = _tables[name]
			table._db = null
			table._name = null
			cbridge.database.closeTable(this, table)
			table._c = null
			/* TODO gc() */
			delete _tables[name]
		}
		else
			throw "Error closing table '" + name + "': table not existing or not opened"
	}

	function exists(name) {
		if(name in _tables)
			return true
		else
			return cbridge.database.tableExists(this, name)
	}

	function spec(name) {
	}
}

