local Directory = import("gla.util.Directory")
local Printer = import("gla.textgen.Printer")

return class {
	root = null
	current = null
	curentry = null
	directory = null
	stack = null

	constructor() {
		directory = Directory()
		stack = []

		root = Printer()
		current = root
	}

	function select(path, ascend = 0) {
		local id = curentry
		while((ascend > 0 || ascend = -1) && id != null)
			id = directory.parent(id)
		if(path != null && path != "") {
			if(id != null)
				id = directory.rfind(id, path)
			else
				id = directory.find(path)
			if(id == null)
				throw "printer not found"
		}
		stack.push(curentry)
		curentry = id
		if(curentry == null)
			current = root
		else
			current = directory.data(curentry)
	}

	function unselect() {
		curentry = stack.top()
		stack.pop()
		if(curentry == null)
			current = root
		else
			current = directory.data(curentry)
	}

	function unselectall() {
		stack.clear()
		curentry = null
		current = root
	}

	function insert(path) {
		local sub = Printer()
		directory.rinsert(curentry, path, sub)
		current.embed(sub)
	}

	function commit(sink) {
		return root.commit(sink)
	}

	function dump() {
		directory.dump()
	}

/*	function append(path) {
		directory.rappend(
	}*/
	function clear() {
		current.clear()
	}

	function pn(text = null) {
		current.pn(text)
	}

	function pni(text) {
		current.pni(text)
	}

	function ipn(text) {
		current.ipn(text)
	}

	function pnu(text) {
		current.pnu(text)
	}

	function upn(text) {
		current.upn(text)
	}

	function upni(text) {
		current.upni(text)
	}

	function ipnu(text) {
		current.ipnu(text)
	}

	function upnu(text) {
		current.upnu(text)
	}

	function pr(text) {
		current.pr(text)
	}

	function pri(text) {
		current.pri(text)
	}

	function ipr(text) {
		current.ipr(text)
	}

	function pru(text) {
		current.pru(text)
	}

	function upr(text) {
		current.upr(text)
	}

	function upri(text) {
		current.upri(text)
	}

	function upru(text) {
		current.upru(text)
	}

	function pt(text, indentstr = "\t", nlstr = "\n") {
		current.pt(text, indentstr, nlstr)
	}

	function embed(other) {
		current.embed(other)
	}

	function indent(amount = 1) {
		current.indent(amount)
	}

	function unindent(amount = 1) {
		current.unindent(amount)
	}

	function begin(filter) {
		current.begin(filter)
	}

	function end() {
		current.end()
	}

	function command(data) {
		current.command(data)
	}
/*	function _get(name) {
		if(current == null)
			throw null
			::print("GET: " + name + " " + this.current + " " + this.root)
		return current[name]
	}*/
}

