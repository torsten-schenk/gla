local Buffer = import("gla.io.Buffer")
local Super = import("gla.textgen.IOSink")

return class extends Super {
	_clusterSize = null

	constructor(clusterSize = 1024) {
		_clusterSize = clusterSize
	}

	function begin() {
		_io = Buffer(_clusterSize)
		_curindent = 0
		_isnewline = true
	}

	function end() {
		local result = _io.tostring()
		_io.close()
		_io = null
		return result
	}
}

