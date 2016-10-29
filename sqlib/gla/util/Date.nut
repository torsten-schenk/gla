local strlib = import("squirrel.stringlib")

local regex = strlib.regexp("\\s*([0-9]+)-([0-9]+)-([0-9]+)\\s*")

local pad = function(n, v) {
	local result = v.tostring()
	while(result.len() < n)
		result = "0" + result
	return result
}

return class {
	year = 0
	month = 0
	day = 0

	//function fromepoch(offset) 'offset': days since 0000-01-01
	//function toepoch(): days since 0000-01-01
	//function today()

	function reset() {
		year = 0
		month = 0
		day = 0
	}

	function parse(string) {
		if(string == null || string == "") {
			year = 0
			month = 0
			day = 0
			return true
		}
		local cap = regex.capture(string)
		if(cap == null)
			return false
		year = string.slice(cap[1].begin, cap[1].end).tointeger()
		month = string.slice(cap[2].begin, cap[2].end).tointeger()
		day = string.slice(cap[3].begin, cap[3].end).tointeger()
		return true
	}

	function _tostring() {
		return pad(4, year) + "-" + pad(2, month) + "-" + pad(2, day)
	}

	function dump() {
		print("year=" + year + " month=" + month + " day=" + day)
	}
}

