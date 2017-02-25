local XmlWalker = import("gla.xml.model.Walker")
//local strlib = import("squirrel.stringlib")
local walkerdef = import("gla.graphml._run_loader")

/*local XmlWalker.Element = class extends WalkerNode {
	attribs = null
	text = null
	constructor(context) {
		local textit = FirstTextIterator(context._graph, context._source)
		attribs = AttributeIterator(context._graph, context._source)
		if(textit.text != null) {
			text = strlib.strip(textit.text)
			if(text.len() == 0)
				text = null
		}
	}
}*/


local Base
Base = class extends XmlWalker </ nodes = walkerdef.nodes, edges = walkerdef.edges /> {
	NodeParser = null
	EdgeParser = null

	nodeParser = null
	edgeParser = null

	constructor(reader, NodeParser, EdgeParser) {
		XmlWalker.constructor.call(this, reader)
		this.NodeParser = NodeParser
		this.EdgeParser = EdgeParser
	}

	Document = class extends XmlWalker.Element {
		function enter() {
			if(walker.NodeParser == null)
				context.nodeParser <- null
			else
				context.nodeParser <- walker.NodeParser(context)
			if(walker.EdgeParser == null)
				context.edgeParser <- null
			else
				context.edgeParser <- walker.EdgeParser(context)
			walker.edgeParser = context.edgeParser
			walker.nodeParser = context.nodeParser
			context.keys <- { node = null, edge = null }
		}
		function run() {
			if(context.keys.node == null)
				throw "missing key 'nodegraphics'"
			if(context.keys.edge == null)
				throw "missing key 'edgegraphics'"
		}
	}
	Key = class extends XmlWalker.Element {
		function run() {
			if(attribute("for") == "node" && attribute("yfiles.type") == "nodegraphics") {
				if(context.keys.node != null)
					throw "key 'nodegraphics' defined multiple times"
				else if(attribute("id") == null)
					throw "key 'nodegraphics' without id"
				else
					context.keys.node = attribute("id")
			}
			else if(attribute("for") == "edge" && attribute("yfiles.type") == "edgegraphics") {
				if(context.keys.edge != null)
					throw "key 'edgegraphics' defined multiple times"
				else if(attribute("id") == null)
					throw "key 'edgegraphics' without id"
				else
					context.keys.edge = attribute("id")
			}
		}
	}
	Graph = class extends XmlWalker.Element {
		function enter() {
			if("node" in context) {
				context.graph <- context.node
				context.node.type = "graph"
			}
			else
				context.graph <- null
		}
	}
	Node = class extends XmlWalker.Element {
		function enter() {
			context.node <- {
				id = attribute("id"),
				nameLabel = null,
				type = null,
				bpmn = {
					activity = {
						type = null,
						adhoc = null,
						closed =null
					}
					ev = {
						type = null,
						flow = null,
						characteristic = null
					}
					gateway = {
						type = null
					}
					direction = null
				}
				uml = {
					stereotype = null,
					constraint = null,
					methodLabel = null
				}
				attributeLabel = null,
				effect3d = null,
				table = null,
				geometry = {
					x = null,
					y = null,
					w = null,
					h = null
				}
			}
			if(context.node.id == null)
				throw "missing node id"
		}
		function leave() {
			if(context.node.type == null)
				throw "node " + context.node.id + ": unknown node type"
			if(context.nodeParser != null)
				context.nodeParser.parsed(context.node, context.graph)
		}
	}
	Edge = class extends XmlWalker.Element {
		function enter() {
			context.edge <- {
				id = attribute("id"),
				type = null,
				nameLabel = null,
				source = {
					type = null,
					id = attribute("source"),
					multiplicity = null
				}
				target = {
					type = null,
					id = attribute("target"),
					multiplicity = null
				}
			}
		}
		function leave() {
			if(context.edgeParser != null)
				context.edgeParser.parsed(context.edge)
		}
	}
	NodeData = class extends XmlWalker.Element {
		function enter() {
			return attribute("key") == context.keys.node
		}
	}
	EdgeData = class extends XmlWalker.Element {
		function enter() {
			return attribute("key") == context.keys.edge
		}
	}
	GenericNode = class extends XmlWalker.Element {
		function enter() {
			local config = attribute("configuration")
			context.genericNodeConfig <- {
				extensions = null
			}
			switch(config) {
				case "com.yworks.entityRelationship.big_entity":
					context.node.type = "er:entity_with_attributes"
					break
				case "com.yworks.entityRelationship.small_entity":
					context.node.type = "er:entity"
					break
				case "com.yworks.entityRelationship.relationship":
					context.node.type = "er:relationship"
					break
				case "com.yworks.entityRelationship.attribute":
					context.node.type = "er:attribute"
					break
				case "com.yworks.flowchart.annotation":
					context.node.type = "flow:annotation"
					break
				case "com.yworks.bpmn.Artifact.withShadow":
				case "com.yworks.bpmn.Artifact":
					context.genericNodeConfig.extensions = "bpmn:artifact"
					break
				case "com.yworks.bpmn.Conversation.withShadow":
				case "com.yworks.bpmn.Conversation":
					context.node.type = "bpmn:conversation"
					break
				case "com.yworks.bpmn.Activity.withShadow":
				case "com.yworks.bpmn.Activity":
					context.node.type = "bpmn:activity"
					context.genericNodeConfig.extensions = "bpmn:activity"
					break
				case "com.yworks.bpmn.Event.withShadow":
				case "com.yworks.bpmn.Event":
					context.node.type = "bpmn:event"
					context.genericNodeConfig.extensions = "bpmn:event"
					break
				case "com.yworks.bpmn.Gateway.withShadow":
				case "com.yworks.bpmn.Gateway":
					context.node.type = "bpmn:gateway"
					context.genericNodeConfig.extensions = "bpmn:gateway"
					break
				default:
					throw "unsupported node with type " + config
					break
			}
		}
	}
	Shape = class extends XmlWalker.Element {
		function run() {
			local shape = attribute("type")
			switch(shape) {
				case "rectangle3d":
					context.node.type = "shape:rectangle_3d"
					break
				case "ellipse":
					context.node.type = "shape:ellipse"
					break
				case "diamond":
					context.node.type = "shape:diamond"
					break
				case "rectangle":
					context.node.type = "shape:rectangle"
					break
				case "triangle":
					context.node.type = "shape:triangle"
					break
				case "roundrectangle":
					context.node.type = "shape:roundrectangle"
					break
				case "hexagon":
					context.node.type = "shape:hexagon"
					break
			}
		}
	}
	NodeStyle = class extends XmlWalker.Element {
		function run() {
			local name = attribute("name")
			local value = attribute("value")
			if("genericNodeConfig" in context) {
				if(context.genericNodeConfig.extensions == "bpmn:artifact")
					switch(name) {
						case "com.yworks.bpmn.type":
							if(value == "ARTIFACT_TYPE_ANNOTATION")
								context.node.type = "bpmn:annotation"
							else if(value == "ARTIFACT_TYPE_DATA_OBJECT")
								context.node.type = "bpmn:dataobject"
							else if(value == "ARTIFACT_TYPE_DATA_STORE")
								context.node.type = "bpmn:datastore"
							else if(value == "ARTIFACT_TYPE_REQUEST_MESSAGE") {
								context.node.type = "bpmn:message"
								context.node.bpmn.direction = "request"
							}
							break
						case "com.yworks.bpmn.dataObjectType":
							if(value == "DATA_OBJECT_TYPE_INPUT")
								context.node.bpmn.direction = "input"
							else if(value == "DATA_OBJECT_TYPE_OUTPUT")
								context.node.bpmn.direction = "output"
							break
					}
				else if(context.genericNodeConfig.extensions == "bpmn:event")
					switch(attributes["name"]) {
						case "com.yworks.bpmn.type":
							if(value == "EVENT_TYPE_PLAIN")
								context.node.bpmn.ev.type = "plain"
							else if(value == "EVENT_TYPE_MESSAGE")
								context.node.bpmn.ev.type = "message"
							else if(value == "EVENT_TYPE_TIMER")
								context.node.bpmn.ev.type = "timer"
							else if(value == "EVENT_TYPE_ERROR")
								context.node.bpmn.ev.type = "error"
							else
								throw "unknown bpmn event type: " + value
							break
						case "com.yworks.bpmn.characteristic":
							if(value == "EVENT_CHARACTERISTIC_END")
								context.node.bpmn.ev.flow = "end"
							else if(value == "EVENT_CHARACTERISTIC_START")
								context.node.bpmn.ev.flow = "start"
							else if(value == "EVENT_CHARACTERISTIC_START_EVENT_SUB_PROCESS_INTERRUPTING") {
								context.node.bpmn.ev.flow = "start"
								context.node.bpmn.ev.characteristic = "subProcessInterrupting"
							}
							else if(value == "EVENT_CHARACTERISTIC_INTERMEDIATE_THROWING") {
								context.node.bpmn.ev.flow = "intermediate"
								context.node.bpmn.ev.characteristic = "throwing"
							}
							else if(value == "EVENT_CHARACTERISTIC_INTERMEDIATE_BOUNDARY_INTERRUPTING") {
								context.node.bpmn.ev.flow = "intermediate"
								context.node.bpmn.ev.characteristic = "boundaryInterrupting"
							}
							else if(value = "EVENT_CHARACTERISTIC_INTERMEDIATE_CATCHING") {
								context.node.bpmn.ev.flow = "intermediate"
								context.node.bpmn.ev.characteristic = "catching"
							}
							else
								throw "unknown bpmn event characteristic: " + value
							break
					}
				else if(context.genericNodeConfig.extensions == "bpmn:activity")
					switch(name) {
						case "com.yworks.bpmn.activityType":
							if(value == "ACTIVITY_TYPE_TASK")
								context.node.bpmn.activity.type = "task"
							break
						case "com.yworks.bpmn.marker1":
						case "com.yworks.bpmn.marker2":
						case "com.yworks.bpmn.marker3":
							if(value == "BPMN_MARKER_AD_HOC")
								context.node.bpmn.activity.adhoc = true
							else if(value == "BPMN_MARKER_CLOSED")
								context.node.bpmn.activity.closed = true
							else if(value == "BPMN_MARKER_OPEN")
								context.node.bpmn.activity.closed = false
							break
					}
				else if(context.genericNodeConfig.extensions == "bpmn:gateway")
					switch(name) {
						case "com.yworks.bpmn.type":
							if(value == "GATEWAY_TYPE_PARALLEL")
								context.node.bpmn.gateway.type = "parallel"
							else if(value == "GATEWAY_TYPE_PLAIN")
								context.node.bpmn.gateway.type = "plain"
							else if(value == "GATEWAY_TYPE_DATA_BASED_EXCLUSIVE")
								context.node.bpmn.gateway.type = "exclusive"
							else if(value == "GATEWAY_TYPE_INCLUSIVE")
								context.node.bpmn.gateway.type = "inclusive"
							else if(value == "GATEWAY_TYPE_COMPLEX")
								context.node.bpmn.gateway.type = "complex"
							else if(value == "GATEWAY_TYPE_EVENT_BASED_EXCLUSIVE")
								context.node.bpmn.gateway.type = "eventExclusive"
							else if(value == "GATEWAY_TYPE_EVENT_BASED_EXCLUSIVE_START_PROCESS")
								context.node.bpmn.gateway.type = "eventExclusiveStart"
							else if(value == "GATEWAY_TYPE_PARALLEL_EVENT_BASED_EXCLUSIVE_START_PROCESS")
								context.node.bpmn.gateway.type = "eventParallelStart"
							else
								throw "unknown bpmn gateway: " + value
							break
					}
			}
		}
	}
	NoteNode = class extends XmlWalker.Element {
		function run() {
			context.node.type = "uml:note"
		}
	}
	UmlClassNode = class extends XmlWalker.Element {
		function run() {
			context.node.type = "uml:class"
		}
	}
	UmlNode = class extends XmlWalker.Element {
		function run() {
			local stereotype = attribute("stereotype")
			local constraint = attribute("constraint")
			if(stereotype != null && stereotype.len() > 0)
				context.node.uml.stereotype = stereotype
			if(constraint != null && constraint.len() > 0)
				context.node.uml.constraint = constraint
			context.node.effect3d = attribute("use3DEffect") == "true"
		}
	}
	TableNode = class extends XmlWalker.Element {
		tableDelegate = {
			function nodeRow(node) {
				local nodey = node.geometry.y
				nodey -= bbox.y
				nodey -= colinsets.t
				foreach(idx, row in this.rows)
					if(nodey >= row.offset && nodey < row.offset + row.size && nodey + node.geometry.h <= row.offset + row.size)
						return row
				return null
			}
			function nodeCol(node) {
				local nodex = node.geometry.x
				nodex -= bbox.x
				nodex -= colinsets.l
				foreach(idx, col in this.cols)
					if(nodex >= col.offset && nodex < col.offset + col.size && nodex + node.geometry.w <= col.offset + col.size)
						return col
				return null
			}
		}
		function enter() {
			context.node.type = "table"
			context.table <- {
				rows = {}
				cols = {}
				rowinsets = {
					l = 0,
					r = 0
				}
				colinsets = {
					t = 0,
					b = 0
				}
				bbox = {
					x = null,
					y = null,
					w = null,
					h = null
				}
			}
			context.table.setdelegate(tableDelegate)
			context.tabledata <- {
				l = null,
				r = null,
				t = null,
				b = null,
				w = null,
				h = null
			}
			context.node.table = context.table
		}
		function leave() {
			context.table.bbox.x = context.tabledata.l + context.node.geometry.x
			context.table.bbox.y = context.tabledata.t + context.node.geometry.y
			context.table.bbox.w = context.node.geometry.w - context.tabledata.l - context.tabledata.r
			context.table.bbox.h = context.node.geometry.h - context.tabledata.t - context.tabledata.b
		}
	}
	Table = class extends XmlWalker.Element {
		function enter() {
			context.insets <- {
				l = null,
				r = null,
				t = null,
				b = null
			}
		}
		function leave() {
			context.tabledata.l = context.insets.l
			context.tabledata.r = context.insets.r
			context.tabledata.t = context.insets.t
			context.tabledata.b = context.insets.b
		}
	}
	Rows = class extends XmlWalker.Element {
		function enter() {
			context.rowdata <- {
				index = 0,
				l = 0,
				r = 0,
				h = 0
			}
		}
		function leave() {
			context.table.rowinsets.l = context.rowdata.l
			context.table.rowinsets.r = context.rowdata.r
		}
	}
	Cols = class extends XmlWalker.Element {
		function enter() {
			context.coldata <- {
				index = 0,
				t = 0,
				b = 0,
				w = 0
			}
		}
		function leave() {
			context.table.colinsets.t = context.coldata.t
			context.table.colinsets.b = context.coldata.b
		}
	}
	Row = class extends XmlWalker.Element {
		function enter() {
			context.insets <- {
				l = null,
				r = null,
				t = null,
				b = null
			}
		}
		function run() {
			local height = attribute("height").tofloat()
			local id = attribute("id")
			if(id in context.table.rows)
				throw "duplicate row id '" + id + "'"
			context.table.rows[id] <- {
				id = id,
				index = context.rowdata.index++,
				nameLabel = null,
				offset = context.rowdata.h,
				size = height
			}
			context.rowdata.h += height
		}
		function leave() {
			if(context.insets.t != 0.0 || context.insets.b != 0.0)
				throw "unsupported insets: table rows must not have a top/bottom inset"
			if(context.insets.l > context.rowdata.l)
				context.rowdata.l = context.insets.l
			if(context.insets.r > context.rowdata.r)
				context.rowdata.r = context.insets.r
		}
	}
	Col = class extends XmlWalker.Element {
		function enter() {
			context.insets <- {
				l = null,
				r = null,
				t = null,
				b = null
			}
		}
		function run() {
			local width = attribute("width").tofloat()
			local id = attribute("id")
			if(id in context.table.cols)
				throw "duplicate column id '" + id + "'"
			context.table.cols[id] <- {
				id = id,
				index = context.coldata.index++,
				nameLabel = null,
				offset = context.coldata.w,
				size = width
			}
			context.coldata.w += width
		}
		function leave() {
			if(context.insets.l != 0.0 || context.insets.r != 0.0)
				throw "unsupported insets: table column must not have a left/right inset"
			if(context.insets.t > context.coldata.t)
				context.coldata.t = context.insets.t
			if(context.insets.b > context.coldata.b)
				context.coldata.b = context.insets.b
		}
	}
	Insets = class extends XmlWalker.Element {
		function run() {
			context.insets.l = attribute("left").tofloat()
			context.insets.r = attribute("right").tofloat()
			context.insets.t = attribute("top").tofloat()
			context.insets.b = attribute("bottom").tofloat()
		}
	}
	NestedError = class extends XmlWalker.Element {
		function run() {
			throw "nested rows/columns in table currently not supported"
		}
	}
	GraphLabel = class extends XmlWalker.Element {
		function run() {
			if(context.node.nameLabel != null) {
				if(text != context.node.nameLabel)
					throw "graph has different labels: '" + context.node.nameLabel + "' and '" + firsttext + "' (please check open and closed state labels)"
			}
			else
				context.node.nameLabel = firsttext
		}
	}
	NodeLabel = class extends XmlWalker.Element {
		function enter() {
			context.label <- {
				row = null,
				col = null,
				text = null
			}
		}
		function run() {
			if(firsttext == null || firsttext.len() == 0)
				return
			switch(attribute("configuration")) {
				case "com.yworks.entityRelationship.label.attributes":
					if(context.node.attributeLabel != null)
						throw "data label defined multiple times"
					context.node.attributeLabel = firsttext
					break
				case "com.yworks.entityRelationship.label.name":
					if(context.node.nameLabel != null)
						throw "name label defined multiple times"
					context.node.nameLabel = firsttext
					break
				default:
					break
			}
			context.label.text = firsttext
		}
		function leave() {
			if(firsttext == null || firsttext.len() == 0)
				return
			if(attribute("configuration") == null) {
				if(context.label.row == null && context.label.col == null) {
					if(context.node.nameLabel != null)
						throw "name label defined multiple times"
					context.node.nameLabel = firsttext
				}
				else if(context.label.row != null) {
					if(context.table == null)
						throw "row label defined but no table present on node"
					if(!(context.label.row in context.table.rows))
						throw "no such row for node label: '" + context.label.row + "'"
					context.table.rows[context.label.row].nameLabel = firsttext
				}
				else if(context.label.col != null) {
					if(context.table == null)
						throw "column label defined but no table present on node"
					if(!(context.label.col in context.table.cols))
						throw "no such column for node label: '" + context.label.col + "'"
					context.table.cols[context.label.col].nameLabel = firsttext
				}
			}
		}
	}
	RowLabelParam = class extends XmlWalker.Element {
		function run() {
			context.label.row = attribute("id")
		}
	}
	ColLabelParam = class extends XmlWalker.Element {
		function run() {
			context.label.col = attribute("id")
		}
	}
	NodeGeometry = class extends XmlWalker.Element {
		function run() {
			context.node.geometry.x = attribute("x").tofloat()
			context.node.geometry.y = attribute("y").tofloat()
			context.node.geometry.w = attribute("width").tofloat()
			context.node.geometry.h = attribute("height").tofloat()
		}
	}
	NodeAttributeLabel = class extends XmlWalker.Element {
		function run() {
			if(context.node.attributeLabel != null)
				throw "attribute label defined multiple times"
			context.node.attributeLabel = firsttext
		}
	}
	NodeMethodLabel = class extends XmlWalker.Element {
		function run() {
			if(context.node.uml.methodLabel != null)
				throw "method label defined multiple times"
			context.node.uml.methodLabel = firsttext
		}
	}
	GenericEdge = class extends XmlWalker.Element {
		function enter() {
			context.genericEdgeConfig <- {
				extensions = null
			}
			switch(attribute("configuration")) {
				case "com.yworks.bpmn.Connection":
					context.genericEdgeConfig.extensions = "bpmn:connection"
					break
			}
		}
	}
	EdgeLabel = class extends XmlWalker.Element {
		function run() {
			if(context.edge.nameLabel != null)
				throw "name label defined multiple times"
			context.edge.nameLabel = firsttext
		}
	}
	EdgeStyle = class extends XmlWalker.Element {
		function run() {
			local value = attribute("value")
			local name = attribute("name")
			if("genericEdgeConfig" in context)
				if(context.genericEdgeConfig.extensions == "bpmn:connection")
					switch(name) {
						case "com.yworks.bpmn.type":
							if(value == "CONNECTION_TYPE_SEQUENCE_FLOW")
								context.edge.type = "bpmn:sequence"
							else if(value == "CONNECTION_TYPE_CONDITIONAL_FLOW")
								context.edge.type = "bpmn:condition"
							else if(value == "CONNECTION_TYPE_DEFAULT_FLOW")
								context.edge.type = "bpmn:default"
							else
								throw "unknown bpmn connection type: '" + name + "'"
							break
					}
		}
	}
	Arrows = class extends XmlWalker.Element {
		function run() {
			local source = attribute("source")
			local target = attribute("target")
			if(source == null)
				throw "missing source arrow"
			if(target == null)
				throw "missing target arrow"
			context.edge.source.type = source
			context.edge.target.type = target
			context.edge.source.multiplicity = multiplicityFor(source)
			context.edge.target.multiplicity = multiplicityFor(target)
		}
		function multiplicityFor(type) {
			switch(type) {
				case "crows_foot_one":
				case "crows_foot_one_optional":
					return { max = 1 }
				case "crows_foot_many":
					return {}
				case "crows_foot_many_optional":
					return { min = 0 }
				case "crows_foot_one_mandatory":
					return { min = 1, max = 1 }
				case "crows_foot_many_mandatory":
					return { min = 1 }
				default:
					return null
			}
		}
	}

	static function simpleLoad(graphml, NodeParser, EdgeParser, context = null, dotrim = true) {
		local Database = import("gla.storage.memory.Database")
		local ModelParser = import("gla.xml.model.Parser")
		local ModelReader = import("gla.goprr.storage.ModelReader")
		local StackExecuter = import("gla.util.StackExecuter")
		local PackagePath = import("gla.io.PackagePath")

		local db = Database()
		local parser = ModelParser(db)
		local path = PackagePath(graphml, true)
		path.extdefault("graphml")
		parser.trim = dotrim
		parser.parse(path)

		local loader = Base(ModelReader(db), NodeParser, EdgeParser)
		loader.basecontext = context
		StackExecuter(loader).execute()
		return loader
	}

	static function canonicalizeEdge(data, source, target) {
		if(data.source.type == source && data.target.type == target)
			return true
		else if(data.source.type == target && data.target.type == source) {
			local tmp = data.source
			data.source = data.target
			data.target = tmp
			return true
		}
		else
			return false
	}
}

return Base

