local StackExecuter = import("gla.util.StackExecuter")

local MyStackTask = class extends import("gla.util.StackTask") </
	nodes = [ null ],
	edges = [ {
		target = 0,
		priority = 0
	}, {
		source = 0,
		target = 1,
		priority = 0
	} ]
/> {
	context = null

	constructor() {
		this.context = {}
	}

	function begin() { //return false for veto
		print("BEGIN")
	}

	function end() { //no return
		print("END")
	}

	function descend() { //return false for veto
		print("DESCEND")
	}

	function ascend() {
		print("ASCEND")
	}

	function edgeBegin(id) {
		print("EDGE BEGIN " + id)
	}

	function edgeNext(id, iteration) {
		print("EDGE NEXT " + id + " " + iteration)
		return iteration < 2
	}

	function edgeEnd(id) {
		print("EDGE END " + id)
	}

	function edgePrepare(id) {
		print("EDGE PREPARE " + id)
	}

	function edgeUnprepare(id) {
		print("EDGE UNPREPARE " + id)
	}

	function edgeAbort(id, exception) {
		print("EDGE ABORT " + id + " " + exception)
	}

	function edgeCatch(id, exception) {
		print("EDGE CATCH " + id + " " + exception)
	}

	function nodeEnter(id) {
		print("NODE ENTER " + id)
	}

	function nodeLeave(id) {
		print("NODE LEAVE " + id)
	}

	function nodeRun(id) {
		print("NODE RUN " + id)
	}

	function nodeCatch(id, exception) {
		print("NODE CATCH " + id + " " + exception)
	}
}

local task = MyStackTask()
local exec = StackExecuter(task)
exec.execute()

print("FINISHED")

