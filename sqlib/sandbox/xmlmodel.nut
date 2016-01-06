local xmlns = import("gla.xml.model.namespace")

local xml = xmlns.Model("sandbox.Document")
//local xml = xmlns.Model("<graphml>sandbox.pop");

local graph = xml.graphs[xmlns.graph.Structure]

print("ROOT: " + graph.root.name)

local prevRelationship = null;
for(local i = 0; i < graph.relationships.len(); i++) {
	local relationship = graph.relationships.at(i, 0)
	if(relationship != prevRelationship) {
		prevRelationship = relationship
		print("------------------------------------------------------------------")
	}
	if(graph.relationships.at(i, 1) == xmlns.role.Attribute)
		print("ATTRIBUTE: " + graph.relationships.at(i, 2).name + " " + graph.relationships.at(i, 3).value);
	else if(graph.relationships.at(i, 1) == xmlns.role.Governor)
		print("GOVERNOR: " + graph.relationships.at(i, 3).name);
	else if(graph.relationships.at(i, 1) == xmlns.role.Child)
		print("CHILD: " + graph.relationships.at(i, 3).name);
	else if(graph.relationships.at(i, 1) == xmlns.role.FirstText)
		print("FIRST TEXT: '" + graph.relationships.at(i, 3).data + "'");
	else if(graph.relationships.at(i, 1) == xmlns.role.FollowingText)
		print("FOLLOWING TEXT: '" + graph.relationships.at(i, 3).data + "'");
}

// ITERATORS
local AttributeIterator = import("gla.xml.model.AttributeIterator")
local ChildIterator = import("gla.xml.model.ChildIterator")

for(local w = AttributeIterator(graph, graph.root); !w.atEnd(); w.next())
	print("GOT ATTRIBUTE: " + w.name + " -> " + w.value)

local w = AttributeIterator(graph, graph.root)
local found = w.seek("other")
print("FOUND: " + found + " " + w.name + " -> " + w.value)

for(local it = ChildIterator(graph, graph.root, { filter = "NodeA" }); !it.atEnd(); it.next())
	print("GOT CHILD: " + it.name)

// WALKER

local Walker = import("gla.goprr.Walker")
local StructureGraphSelector = import("gla.xml.model.StructureGraphSelector")
local StackExecuter = import("gla.util.StackExecuter")

local nodes = [ { 
		clazz = class extends import("gla.goprr.WalkerNode") {
			constructor(context) {
				print("NODE A CONTEXT: " + context)
			}
			function run() {
				print("NODE A RUN")
			}
			function catsh(exception) {
				print("NODE A CATCH: " + exception)
				return true
			}
		}
	}, {
		clazz = class extends import("gla.goprr.WalkerNode") {
			function run() {
				print("NODE AA RUN")
//				throw("An error")
			}
		}
	}
]

local edges = [ {
		target = 0,
		clazz = class {},
		parameters = null,
		iterator = {
			clazz = ChildIterator,
			parameters = { filter = "NodeA" }
		},
		selector = {
			clazz = StructureGraphSelector,
			parameters = null
		}
	}, {
		source = 0,
		target = 1,
		clazz = class {},
		iterator = {
			clazz = ChildIterator,
			parameters = { filter = "NodeAA" }
		}
	}
]

class MyWalker extends Walker </ nodes = nodes, edges = edges /> {
}

local myWalker = MyWalker(xml, graph.root)
local exec = StackExecuter(myWalker);

exec.execute()

