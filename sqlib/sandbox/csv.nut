local Parser = import("gla.csv.CallbackParser")

class MyParser extends Parser {
	constructor() {
		delim = ';'
	}

	function begin(index) {
		print("BEGIN ROW " + index)
	}

	function entry(index, value) {
		print("  -> " + index + " " + value)
	}

	function end(index, entries) {
		print("END ROW " + index + " " + entries)
	}
}

local p = MyParser()
p.parse("sandbox.test")

