//TODO move this class to a different package. Reason: it is supposed to be part of the frontend, so it is optimized for user interaction, not for data processing.
local Table = import("gla.util.SimpleTable")

return class {
	table = null
	listeners = null

	constructor(datatype = null, ...) {
		listeners = []
//		table = Table()
		if(datatype != null) {
			switch(datatype) {
				case "string":
					break
				case "class":
					break
				case "instance":
					break
				default:
					throw "Unkown datatype for registry: '" + datatype + "'"
			}
		}
	}

	function insert(value, path = null) { //TODO add short/long description/name/help object?
		if(typeof(path) == "array") {
		}
		else if(typeof(path) == "string") {
		}
		else if(path == null) {
		}
		else
			throw "Invalid path given"
	}
}

