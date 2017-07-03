local strlib = import("squirrel.stringlib")

local regexParse = strlib.regexp(@"parse:(.*)")
local regexImport = strlib.regexp(@"import:([a-zA-Z_0-9\.]+)->(a-zA-Z0-9\.]+)")

return class {
	constructor() {
	}

	function parse(package, source, options = null) {
		return this
	}

	function convert(package, options = null) {
		return this
	}

	function generate(package, target, options = null) {
		return this
	}
}

