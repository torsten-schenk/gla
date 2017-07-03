local from = {}
local to = {}

return class {
	function _inherited(attr) {
		local source = attr.source
		local target = attr.target
		local from2
		local to2
		if(!(source in from))
			from2 = {}
		if(!(target in to))
			to2 = {}
		from2 = from[source]
		to2 = to[target]
		if(target in from2)
			throw "converter already registered"
		else if(source in to2)
			throw "converter already registered"

		from2[target] <- this
		to2[source] <- this
	}
}

