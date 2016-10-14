local Path = import("gla.io.Path")
local PackagePath = import("gla.io.PackagePath")
local Super = import("gla.textgen.Sink")

return class extends Super {
	_newline = "\n"
	_indent = "\t"
	_path = null
	_io = null

	_curindent = 0
	_isnewline = true

	constructor(path) {
		if(path instanceof Path)
			_path = path
		else if(typeof(path) == "string")
			_path = PackagePath(path, true)
		else
			throw "Invalid path given"
	}

	function begin() {
		_io = open(_path, "w")
		_curindent = 0
		_isnewline = true
	}

	function end() {
		_io.close()
		_io = null
	}

	function text(text) {
		if(_isnewline)
			for(local i = 0; i < _curindent; i++)
				_io.write(_indent)
		_io.write(text)
		_isnewline = false
	}

	function indent(amount) {
		_curindent += amount
	}

	function newline() {
		_io.write(_newline)
		_isnewline = true
	}

	function token(data) {
	}
}

