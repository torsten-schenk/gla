local SimpleTable = import("gla.storage.SimpleTable")

return class {
	outgoing = null //[ source target Edge edge ]
	incoming = null //[ target source Edge edge ]

	constructor() {
		outgoing = SimpleTable(4, 0)
		incoming = SimpleTable(4, 0)
	}

	function checklink(Edge, source, target) {} //throw an error message if graph does not allow an instance of Edge to link nodes source -> target
}

