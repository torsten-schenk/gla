//class must have attribute 'edges': an array of tables with the mandatory entries:
//{ source = <node id>, target = <node id>, priority = <priority> }; if <node id> is an integer >= 0 and < n, an internal node is
//referred to (by index). if <node id> is a string, an the node can be accessed externally (TO BE DONE)
//if <node id> is null, the implicit root node is referred to (only valid as source)
//the class attribute 'nodes': an array of tables without mandatory entries describing all nodes
//veto returns: null, true: no veto, proceed; false: veto, don't proceed
return class {
	begin = null //function begin(), return: veto
	end = null //function end(), return: none
	descend = null //function descend(), return: vero
	ascend = null //function ascend(), return: none

	edgeBegin = null //function edgeBegin(id); return: veto
	edgeNext = null //function edgeNext(id, iteration); returns whether there is a next element (null: next element only in first iteration); if there is no such method, a single iteration will be performed on each edge
	edgeEnd = null //function edgeEnd(id); return: none
	edgePrepare = null //function edgePrepare(id); return: none
	edgeUnprepare = null //function edgeUnprepare(id); return: none
	edgeAbort = null //function edgeAbort(id); return: none (throwing will cause a fatal error and abort execution completely)
	edgeCatch = null //function edgeCatch(id); return: true: error catched, continue; false/null: exception not catched (throwing will update the exception value)

	nodeEnter = null //function nodeEnter(id); return: veto
	nodeLeave = null //function nodeLeave(id); return: none
	nodeRun = null //function nodeRun(id); return: none
	nodeCatch = null //function nodeCatch(id, value); returns whether 'value' has been catched
}

