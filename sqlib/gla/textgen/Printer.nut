local Sink = import("gla.textgen.Sink")
local Filter = import("gla.textgen.Filter")

const CLUSTER_SIZE = 256

enum Fragment {
	Null,
	Text,
	Newline,
	Indent,
	Begin,
	End,
	Embed,
	Command	
}

local Cluster = class {
	next = null
	types = null
	data = null
	fill = 0

	constructor(clusterSize) {
		data = ::array(clusterSize)
		types = ::array(clusterSize)
	}
}

local FragmentData = class {
	type = null
	data = null
	constructor(type, data) {
		this.type = type
		this.data = data
	}
}
local Printer

Printer = class {
	_fragments = null

	constructor(clusterSize = CLUSTER_SIZE) {
		_fragments = []
	}

	function clear() {
		_fragments = []
	}

	function _append(type, data = null) {
		_fragments.push(FragmentData(type, data))
	}

	function pn(text = null) {
		if(text != null)
			_append(Fragment.Text, text)
		_append(Fragment.Newline)
	}

	function pni(text) {
		_append(Fragment.Text, text)
		_append(Fragment.Newline)
		_append(Fragment.Indent, 1)
	}

	function ipn(text) {
		_append(Fragment.Indent, 1)
		_append(Fragment.Text, text)
		_append(Fragment.Newline)
	}

	function pnu(text) {
		_append(Fragment.Text, text)
		_append(Fragment.Newline)
		_append(Fragment.Indent, -1)
	}

	function upn(text) {
		_append(Fragment.Indent, -1)
		_append(Fragment.Text, text)
		_append(Fragment.Newline)
	}

	function upni(text) {
		_append(Fragment.Indent, -1)
		_append(Fragment.Text, text)
		_append(Fragment.Newline)
		_append(Fragment.Indent, 1)
	}

	function upnu(text) {
		_append(Fragment.Indent, -1)
		_append(Fragment.Text, text)
		_append(Fragment.Newline)
		_append(Fragment.Indent, -1)
	}

	function pr(text) {
		if(text != null)
			_append(Fragment.Text, text)
	}

	function pri(text) {
		_append(Fragment.Text, text)
		_append(Fragment.Indent, 1)
	}

	function ipr(text) {
		_append(Fragment.Indent, 1)
		_append(Fragment.Text, text)
	}

	function pru(text) {
		_append(Fragment.Text, text)
		_append(Fragment.Indent, -1)
	}

	function upr(text) {
		_append(Fragment.Indent, -1)
		_append(Fragment.Text, text)
	}

	function upri(text) {
		_append(Fragment.Indent, -1)
		_append(Fragment.Text, text)
		_append(Fragment.Indent, 1)
	}

	function upru(text) {
		_append(Fragment.Indent, -1)
		_append(Fragment.Text, text)
		_append(Fragment.Indent, -1)
	}

	function pt(text, indentstr = "\t", nlstr = "\n") {
		local index = 0
		local indent = 0
		local curindent
		for(local next = text.find(nlstr); next != null; next = text.find(nlstr, index)) {
			curindent = 0
			while(text.find(indentstr, index) == index) {
				index += indentstr.len()
				curindent++
			}
			if(index < next) { //i.e. not an empty line
				if(curindent != indent) {
					_append(Fragment.Indent, curindent - indent)
					indent = curindent
				}
				_append(Fragment.Text, text.slice(index, next))
				_append(Fragment.Newline)
			}
			index = next + nlstr.len()
		}
		curindent = 0
		while(text.find(indentstr, index) == index) {
			index += indentstr.len()
			curindent++
		}
		if(index < text.len()) {
			if(curindent != indent) {
				_append(Fragment.Indent, curindent - indent)
				indent = curindent
			}
			_append(Fragment.Text, text.slice(index))
			_append(Fragment.Newline)
		}
		if(indent > 0)
			_append(Fragment.Indent, -indent)
	}

	function indent(amount = 1) {
		_append(Fragment.Indent, amount)
	}

	function unindent(amount = 1) {
		_append(Fragment.Indent, -amount)
	}

	function embed(printer) {
		assert(printer instanceof Printer)
		_append(Fragment.Embed, printer)
	}

	function begin(filter) {
		assert(filter instanceof Filter)
		_append(Fragment.Begin, filter)
	}

	function end() {
		_append(Fragment.End)
	}

	function command(data) {
		_append(Fragment.Command, data)
	}

	function commit(sink) {
		assert(sink instanceof Sink)
		local sinkstack = []
		sinkstack.push(sink)
		sink.begin()
		_commit(sinkstack)
		return sink.end()
	}

	function _commit(sinkstack) {
		foreach(v in _fragments) {
			local type = v.type
			local data = v.data
			switch(type) {
				case Fragment.Text:
					sinkstack.top().text(data)
					break
				case Fragment.Indent:
					sinkstack.top().indent(data)
					break
				case Fragment.Newline:
					sinkstack.top().newline()
					break
				case Fragment.Command:
					sinkstack.top().command(data)
					break
				case Fragment.Embed:
					data._commit(sinkstack)
					break
				case Fragment.Begin:
					data.begin(sinkstack.top())
					sinkstack.push(data)
					break
				case Fragment.End:
					sinkstack.top().end()
					sinkstack.pop()
					break
				default:
					assert(false)
			}
		}
	}
}

return Printer

