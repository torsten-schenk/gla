local strlib = import("squirrel.stringlib")

enum Type {
	None,
	LowerSpace, //example lower space
	CapitalSpace, //Example Capital Space
	UpperSpace, //EXAMPLE UPPER SPACE
	LowerUnderscore, //example_lower_underscore
	CapitalUnderscore, //Example_Capital_Underscore
	UpperUnderscore, //EXAMPLE_UPPER_UNDERSCORE
	LowerCamel, //exampleLowerCamel
	CapitalCamel, //ExampleCapitalCamel
	UpperGlue, //EXAMPLEUPPER
	LowerGlue //examplelower
}

local detect = function(identifier, options) {
	throw "identifier type detection not yet implemented"
}

local regex

local split = function(string, type, options) {
	assert(string != null && string.len() > 0)
	switch(type) {
		case Type.LowerSpace:
		case Type.CapitalSpace:
		case Type.UpperSpace:
			if(string[0] == ' ' || string[string.len() - 1] == ' ')
				throw "invalid identifier '" + string + "' for type lower underscore: must not begin or end with ' '"
			else if(string.find("  ") != null)
				throw "invalid identifier '" + string + "' for type lower underscore: must not contain two or more of ' '"
			else
				return strlib.split(string, " ")

		case Type.LowerUnderscore:
		case Type.CapitalUnderscore:
		case Type.UpperUnderscore:
			if(string[0] == '_' || string[string.len() - 1] == '_')
				throw "invalid identifier '" + string + "' for type lower underscore: must not begin or end with '_'"
			else if(string.find("__") != null)
				throw "invalid identifier '" + string + "' for type lower underscore: must not contain two or more of '_'"
			else
				return strlib.split(string, "_")

		case Type.CapitalCamel:
		case Type.LowerCamel: {
			local start = 0
			local fragments = []
			for(local i = 1; i < string.len(); i++)
				if(string[i] >= 'A' && string[i] <= 'Z') {
					fragments.push(string.slice(start, i))
					start = i
				}
			fragments.push(string.slice(start))
			return fragments
		}

		case Type.UpperGlue:
		case Type.LowerGlue:
			throw "cannot split identifier of type upper/lower glue"
	}
}

function combine(fragments, type, options) {
	assert(fragments != null && fragments.len() > 0)
	local sep = ""

	switch(type) {
		case Type.LowerSpace:
		case Type.LowerUnderscore:
		case Type.LowerGlue:
			foreach(i, v in fragments)
				fragments[i] = v.tolower()
			break

		case Type.LowerCamel:
			foreach(i, v in fragments)
				fragments[i] = v.tolower()
			for(local i = 1; i < fragments.len(); i++) {
				assert(fragments[i].len() > 0)
				fragments[i] = fragments[i].slice(0, 1).toupper() + fragments[1].slice(1)
			}
			break

		case Type.CapitalSpace:
		case Type.CapitalUnderscore:
		case Type.CapitalCamel:
			foreach(i, v in fragments)
				fragments[i] = v.tolower()
			for(local i = 0; i < fragments.len(); i++) {
				assert(fragments[i].len() > 0)
				fragments[i] = fragments[i].slice(0, 1).toupper() + fragments[i].slice(1)
			}
			break

		case Type.UpperSpace:
		case Type.UpperUnderscore:
		case Type.UpperGlue:
			foreach(i, v in fragments)
				fragments[i] = v.toupper()
			break
	}

	switch(type) {
		case Type.LowerSpace:
		case Type.CapitalSpace:
		case Type.UpperSpace:
			sep = " "
			break

		case Type.LowerUnderscore:
		case Type.CapitalUnderscore:
		case Type.UpperUnderscore:
			sep = "_"
			break

	}

	local result = ""
	foreach(i, v in fragments) {
		if(i > 0)
			result += sep
		result += v
	}
	return result
}

return class {
	static LowerSpace = Type.LowerSpace
	static CapitalSpace = Type.CapitalSpace
	static UpperSpace = Type.UpperSpace
	static LowerUnderscore = Type.LowerUnderscore
	static CapitalUnderscore = Type.CapitalUnderscore
	static UpperUnderscore = Type.UpperUnderscore
	static LowerCamel = Type.LowerCamel
	static CapitalCamel = Type.CapitalCamel
	static UpperGlue = Type.UpperGlue
	static LowerGlue = Type.LowerGlue

	options = null
	type = null
	fragments = null

	string = null

	constructor(identifier = null, type = null, options = null) {
		if(identifier == null || identifier == "" || identifier == [])
			return

		if(type == null)
			type = detect(identifier, options)
		this.type = type
		this.options = options

		if(typeof(identifier) == "string") {
			string = identifier
			fragments = split(identifier, type, options)
		}
		else if(typeof(identifier) == "array") {
			fragments = clone(identifier)
			string = combine(identifier, type, options)
		}
	}

	function convert(type, options = null) {
		local result = getclass().instance()
		result.type = type
		result.options = options
		result.fragments = clone(fragments)
		result.string = combine(result.fragments, type, options) //NOTE: 'fragments' gets intentionally modified
		return result
	}

	function validate() {
		local expected = combine(fragments, type, options)
		if(expected == string)
			return null
		else
			return "invalid identifier" //TODO return more detailed error message
	}

	function tostring() {
		return string
	}
}

