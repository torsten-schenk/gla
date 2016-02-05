return class extends import("gla.xml.SaxParser") {
	stack = null
	nextid = null

	doctable = null //extern: read-only

	constructor(doctable) {
		stack = []
		nextid = 0
		this.doctable = doctable
	}

	function clear(doctable) {
		stack = []
		nextid = 0
		this.doctable = doctable
	}

	function begin(ns, element) {
		local parentid
		if(stack.len() == 0)
			parentid = null
		else
			parentid = stack.top()
		doctable._structure.insert(parentid, [ ns, element, nextid ])
		doctable._attributes.insert([ nextid, null, ns ], element)
		stack.push(nextid)
		nextid++
	}

	function end(ns, element) {
		stack.pop()
	}

	function text(text) {
		doctable._structure.insert(stack.top(), [ null, null, text ])
	}

	function attribute(ns, key, value) { //after all attributes have been parsed, this method will be called again with 'key' and 'value' == null
		if(key != null) {
			doctable._attributes.insert([ stack.top(), key, ns ] , [ value ])
		}
	}
}

