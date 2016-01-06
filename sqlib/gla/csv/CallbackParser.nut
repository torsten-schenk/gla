local cbridge = import("gla.csv._cbridge")

return class {
	delim = ','
	quote = '"'

	function parse(entity) {
		return cbridge.callback.parse(this, entity, delim, quote)
	}

	function entry(index, string) {
		return true
	}

	function end(index, entries) {
		return true
	}

	function begin(index) {
		return true
	}
}

