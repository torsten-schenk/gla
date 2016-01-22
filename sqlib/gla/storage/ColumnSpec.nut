local cbridge = import("gla.storage._cbridge")

return class {
	columns = null
	names = null
	kcols = 0
	vcols = 0

	constructor(kspec, vspec = null, anonymous = true) {
		local i
		local v
		local ncols = 0

		if(typeof(kspec) == "integer") {
			kcols = kspec
			ncols += kspec
			kspec = array(kcols)
			for(local i = 0; i < kcols; i++)
				kspec[i] = { type = null }
		}
		else if(typeof(kspec) == "table") {
			kcols = 1
			ncols++
			kspec = [ kspec ]
		}
		else if(typeof(kspec) == "array") {
			foreach(i, v in kspec) {
				if(typeof(v) != "table")
					throw "Invalid key column specification: expected either array of tables or single table"
			}
			kcols = kspec.len()
			ncols += kspec.len()
		}
		else if(kspec != null)
			throw "Invalid key column specification: expected either null, table or array of tables"

		if(typeof(vspec) == "integer") {
			vcols = vspec
			ncols += vspec
			vspec = array(vcols)
			for(local i = 0; i < vcols; i++)
				vspec[i] = { type = null }
		}
		else if(typeof(vspec) == "table") {
			vcols = 1
			ncols++
			vspec = [ vspec ]
		}
		else if(typeof(vspec) == "array") {
			foreach(i, v in vspec) {
				if(typeof(v) != "table")
					throw "Invalid value column specification: expected either array of tables or single table"
			}
			vcols = vspec.len()
			ncols += vspec.len()
		}
		else if(vspec == null)
			vspec = []
		else
			throw "Invalid value column specification: expected either null, table or array of tables"

		columns = array(ncols)
		names = {}
		foreach(i, v in kspec)
			_parseColumn(i, v, anonymous)
		foreach(i, v in vspec)
			_parseColumn(kcols + i, v, anonymous)

		cbridge.colspecInit(this)
	}

	function indexOf(name) {
		if(name in names)
			return names[name]
		else
			return null
	}

	function find(value) {
		if(typeof(value) == "integer") {
			if(value < 0)
				throw "Invalid column index: must be >= 0"
			else if(value >= columns.len())
				throw "Invalid column index: must be < " + column.len()
			else
				return value
		}
		else if(typeof(value) == "string") {
			if(value in names)
				return names[value]
			else
				throw "No such column '" + value + "'"
		}
		else
			throw "Invalid column index"
	}

	function isValidFor(col, value) {
		return columns[col].type == null || columns[col].type == typeof(value)
	}

	function _parseColumn(col, spec, anonymous) {
		columns[col] = {}
		if("name" in spec) {
			if(spec.name in names)
				throw "Invalid column specification: column name '" + spec.name + "' used multiple times"
			names[spec.name] <- col
			columns[col].name <- spec.name
		}
		else if(!anonymous)
			throw "Invalid column specification: anonymous columns not allowed"

		if("type" in spec) {
			switch(spec.type) {
				case null:
				case "integer":
				case "bool":
				case "string":
				case "float":
					columns[col].type <- spec.type
					break;
				default:
					throw "Invalid column type '" + spec.type + "': unknown or not supported"
			}
		}
		else
			columns[col].type <- null
	}
}

