local SaxParser = import("gla.xml.SaxParser")

local MySaxParser = class extends SaxParser {
	function begin(element) {
		print("BEGIN: " + element)
	}

	function end(element) {
		print("END: " + element)
	}

	function text(text) {
		print("TEXT: '" + text + "'")
	}

	function attribute(key, value) {
		print("ATTRIBUTE: " + key + " = " + value)
	}
}

local parser = MySaxParser()

parser.parse("sandbox.Document")

print("END")

