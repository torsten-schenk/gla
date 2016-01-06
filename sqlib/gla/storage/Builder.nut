local cbridge = import("gla.storage._cbridge")

return class {
	constructor(colspec) {
		cbridge.builder.ctor(this, colspec)
	}

	function _set(key, value) {
		set(key, value)
	}

	function _get(key) {
		return get(key)
	}
}

