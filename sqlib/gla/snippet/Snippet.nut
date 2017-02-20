return class {
	context = null
	raw = false
	newlineBeforeEmbed = true
	newlineAfterEmbed = false
	newlineBeforeFinish = true

	tokens = null

	constructor(context, parent = null) {
		if(context != null)
			this.context = context
		else
			this.context = {}
		if(parent != null)
			this.context.setdelegate(parent.context)
		tokens = []
	}

	//these functions are called from the parser.
	function parse(text) {}
	function newline() {}
	function embed(snippet) {}
	function finish() {}

	//these functions are called by parse(), embed() and finish()
	function putText(text) {
	}

	function putSlot(name, params) {
	}

	function putChild(snippet) {
	}

	function putNewline(n) {
	}

	function putIndent(n) {
	}

	function execute() {}
}

