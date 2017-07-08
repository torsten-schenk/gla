local strlib = import("squirrel.stringlib")

enum Style {
	None,
	LowerSpace, //example lower space
	CapitalSpace, //Example Capital Space
	UpperSpace, //EXAMPLE UPPER SPACE
	LowerUnderscore, //example_lower_underscore
	CapitalUnderscore, //Example_Capital_Underscore
	UpperUnderscore, //EXAMPLE_UPPER_UNDERSCORE
	LowerHyphen, //example-lower-hyphen
	CapitalHyphen, //Example-Capital-Hyphen
	UpperHyphen, //EXAMPLE-UPPER-HYPHEN
	LowerCamel, //exampleLowerCamel
	CapitalCamel, //ExampleCapitalCamel
	UpperGlue, //EXAMPLEUPPER
	LowerGlue //examplelower
}

local detect = function(identifier, options) {
	throw "identifier style detection not yet implemented"
}

local regexStyle = strlib.regexp("^([a-z]+)[-._ ]([a-z]+)$")

local split = function(string, style, options) {
	assert(string != null && string.len() > 0)
	switch(style) {
		case Style.LowerSpace:
		case Style.CapitalSpace:
		case Style.UpperSpace:
			if(string[0] == ' ' || string[string.len() - 1] == ' ')
				throw "invalid identifier '" + string + "' for style lower underscore: must not begin or end with ' '"
			else if(string.find("  ") != null)
				throw "invalid identifier '" + string + "' for style lower underscore: must not contain two or more of ' '"
			else
				return strlib.split(string, " ")

		case Style.LowerUnderscore:
		case Style.CapitalUnderscore:
		case Style.UpperUnderscore:
			if(string[0] == '_' || string[string.len() - 1] == '_')
				throw "invalid identifier '" + string + "' for style lower underscore: must not begin or end with '_'"
			else if(string.find("__") != null)
				throw "invalid identifier '" + string + "' for style lower underscore: must not contain two or more of '_'"
			else
				return strlib.split(string, "_")

		case Style.LowerHyphen:
		case Style.CapitalHyphen:
		case Style.UpperHyphen:
			if(string[0] == '-' || string[string.len() - 1] == '-')
				throw "invalid identifier '" + string + "' for style lower hyphen: must not begin or end with '-'"
			else if(string.find("--") != null)
				throw "invalid identifier '" + string + "' for style lower hyphen: must not contain two or more of '-'"
			else
				return strlib.split(string, "-")

		case Style.CapitalCamel:
		case Style.LowerCamel: {
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

		case Style.UpperGlue:
		case Style.LowerGlue:
			throw "cannot split identifier of style upper/lower glue"
	}
}

function combine(fragments, style, options) {
	assert(fragments != null && fragments.len() > 0)
	local sep = ""

	switch(style) {
		case Style.LowerSpace:
		case Style.LowerUnderscore:
		case Style.LowerHyphen:
		case Style.LowerGlue:
			foreach(i, v in fragments)
				fragments[i] = v.tolower()
			break

		case Style.LowerCamel:
			foreach(i, v in fragments)
				fragments[i] = v.tolower()
			for(local i = 1; i < fragments.len(); i++) {
				assert(fragments[i].len() > 0)
				fragments[i] = fragments[i].slice(0, 1).toupper() + fragments[1].slice(1)
			}
			break

		case Style.CapitalSpace:
		case Style.CapitalUnderscore:
		case Style.CapitalHyphen:
		case Style.CapitalCamel:
			foreach(i, v in fragments)
				fragments[i] = v.tolower()
			for(local i = 0; i < fragments.len(); i++) {
				assert(fragments[i].len() > 0)
				fragments[i] = fragments[i].slice(0, 1).toupper() + fragments[i].slice(1)
			}
			break

		case Style.UpperSpace:
		case Style.UpperUnderscore:
		case Style.UpperHyphen:
		case Style.UpperGlue:
			foreach(i, v in fragments)
				fragments[i] = v.toupper()
			break
	}

	switch(style) {
		case Style.LowerSpace:
		case Style.CapitalSpace:
		case Style.UpperSpace:
			sep = " "
			break

		case Style.LowerUnderscore:
		case Style.CapitalUnderscore:
		case Style.UpperUnderscore:
			sep = "_"
			break

		case Style.LowerHyphen:
		case Style.CapitalHyphen:
		case Style.UpperHyphen:
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
	static LowerSpace = Style.LowerSpace
	static CapitalSpace = Style.CapitalSpace
	static UpperSpace = Style.UpperSpace
	static LowerUnderscore = Style.LowerUnderscore
	static CapitalUnderscore = Style.CapitalUnderscore
	static UpperUnderscore = Style.UpperUnderscore
	static LowerHyphen = Style.LowerHyphen
	static CapitalHyphen = Style.CapitalHyphen
	static UpperHyphen = Style.UpperHyphen
	static LowerCamel = Style.LowerCamel
	static CapitalCamel = Style.CapitalCamel
	static UpperGlue = Style.UpperGlue
	static LowerGlue = Style.LowerGlue

	options = null
	style = null
	fragments = null

	string = null

	constructor(identifier = null, style = null, options = null) {
		if(identifier == null || identifier == "" || identifier == [])
			return

		if(style == null)
			style = detect(identifier, options)
		this.style = style
		this.options = options

		if(typeof(identifier) == "string") {
			string = identifier
			fragments = split(identifier, style, options)
		}
		else if(typeof(identifier) == "array") {
			fragments = clone(identifier)
			string = combine(identifier, style, options)
		}
	}

	function convert(style, options = null) {
		local result = getclass().instance()
		result.style = style
		result.options = options
		result.fragments = clone(fragments)
		result.string = combine(result.fragments, style, options) //NOTE: 'fragments' gets intentionally modified
		return result
	}

	function validate() {
		local expected = combine(fragments, style, options)
		if(expected == string)
			return null
		else
			return "invalid identifier" //TODO return more detailed error message
	}

	function tostring() {
		return string
	}

	static function parsestyle(text) {
		text = text.tolower()
		local cap = regexStyle.capture(text)
		if(cap == null)
			throw "Unknown identifier style: '" + text + "'"
		local caze = text.slice(cap[1].begin, cap[1].end)
		local sep = text.slice(cap[2].begin, cap[2].end)
		if(sep == "space") {
			if(caze == "lower")
				return Style.LowerSpace
			else if(caze == "capital")
				return Style.CapitalSpace
			else if(caze == "upper")
				return Style.UpperSpace
		}
		else if(sep == "underscore") {
			if(caze == "lower")
				return Style.LowerUnderscore
			else if(caze == "capital")
				return Style.CapitalUnderscore
			else if(caze == "upper")
				return Style.UpperUnderscore
		}
		else if(sep == "hyphen") {
			if(caze == "lower")
				return Style.LowerHyphen
			else if(caze == "capital")
				return Style.CapitalHyphen
			else if(caze == "upper")
				return Style.UpperHyphen
		}
		else if(sep == "camel") {
			if(caze == "lower")
				return Style.LowerCamel
			else if(caze == "capital")
				return Style.CapitalCamel
		}
		else if(sep == "glue") {
			if(caze == "lower")
				return Style.LowerGlue
			else if(caze == "upper")
				return Style.UpperGlue
		}
		throw "Unknown identifier style: '" + text + "'"
	}
}

