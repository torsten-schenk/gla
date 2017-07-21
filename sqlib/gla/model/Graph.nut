return class {
	debug = null //name printed in verbose log

	outgoing = null //[ source target Edge edge ]
	incoming = null //[ target source Edge edge ]

	edges = null
	rvedges = null

	function checklink(Edge, source, target) {} //throw an error message if graph does not allow an instance of Edge to link nodes source -> target
}

