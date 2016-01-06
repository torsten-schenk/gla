local cbridge = import("gla.xml._cbridge")

return class {
	function parse(entity) {
		return cbridge.sax.parse(this, entity)
	}

	function begin(ns, element) {
		return true
	}

	function end(ns, element) {
		return true
	}

	function nsbegin(name) {
	}

	function nsend(name) {
	}

	function text(text) {
		return true
	}

	function attribute(ns, key, value) { //after all attributes have been parsed, this method will be called again with 'key' and 'value' == null
		return true
	}
}

