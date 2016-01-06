local Super = import("gla.textgen.Sink")

return class extends Super {
	function begin() {
	}

	function end() {
	}

	function text(text) {
		print("TEXT: " + text)
	}

	function indent(amount) {
		print("INDENT: " + amount)
	}

	function newline() {
		print("NEWLINE")
	}

	function token(data) {
		print("TOKEN: " + data)
	}
}

