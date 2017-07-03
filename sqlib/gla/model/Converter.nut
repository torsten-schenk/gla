local from = {}
local to = {}

return class {
	function _inherited(attr) {
		local source = attr.source
		local target = attr.target
		local from2
		local to2
		if(!(source in from))
			from[source] <- {}
		if(!(target in to))
			to[target] <- {}
		from2 = from[source]
		to2 = to[target]
		if(target in from2)
			throw "converter already registered"
		else if(source in to2)
			throw "converter already registered"

		from2[target] <- this
		to2[source] <- this
	}

	constructor(model) {
		assert(model.getclass().getattributes(null).meta == getclass().getattributes(null).source)
	}

	convert = null //function convert(options = null)
}

