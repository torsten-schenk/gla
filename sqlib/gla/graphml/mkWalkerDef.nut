return function(nodeclasses) {
	return {
		nodes = [
			{ clazz = nodeclasses["Document"] },
			{ clazz = nodeclasses["Graph"] },
			{ clazz = nodeclasses["Node"] },
			{ clazz = nodeclasses["NodeData"] },
			{ clazz = nodeclasses["GenericNode"] },
			{ clazz = nodeclasses["NodeLabel"] },
			{ clazz = nodeclasses["Edge"] },
			{ clazz = nodeclasses["EdgeData"] },
			null,
			{ clazz = nodeclasses["EdgeLabel"] },
			{ clazz = nodeclasses["Key"] },
			{ clazz = nodeclasses["Arrows"] },
			{ clazz = nodeclasses["NoteNode"] },
			null,
			{ clazz = nodeclasses["Shape"] },
			{ clazz = nodeclasses["UmlClassNode"] },
			{ clazz = nodeclasses["UmlNode"] },
			{ clazz = nodeclasses["NodeAttributeLabel"] },
			{ clazz = nodeclasses["NodeMethodLabel"] },
			null,
			null,
			null,
			{ clazz = nodeclasses["NodeGeometry"] },
			{ clazz = nodeclasses["TableNode"] },
			null,
			{ clazz = nodeclasses["RowLabelParam"] },
			{ clazz = nodeclasses["ColLabelParam"] },
			{ clazz = nodeclasses["Table"] },
			{ clazz = nodeclasses["Cols"] },
			{ clazz = nodeclasses["Rows"] },
			{ clazz = nodeclasses["Row"] },
			{ clazz = nodeclasses["Col"] },
			{ clazz = nodeclasses["NestedError"] },
			{ clazz = nodeclasses["Insets"] },
			null,
			{ clazz = nodeclasses["NodeStyle"] },
			{ clazz = nodeclasses["GenericEdge"] },
			null,
			{ clazz = nodeclasses["EdgeStyle"] }
		],
		edges = [ {
				source = 0,
				target = 1,
				priority = 1,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "graph" }
				},
			}, {
				source = 1,
				target = 2,
				priority = 0,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "node" }
				},
			}, {
				source = 2,
				target = 3,
				priority = 0,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "data" }
				},
			}, {
				source = 3,
				target = 4,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "GenericNode" }
				},
			}, {
				source = 4,
				target = 21,
			}, {
				source = 1,
				target = 6,
				priority = 1,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "edge" }
				},
			}, {
				source = 6,
				target = 7,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "data" }
				},
			}, {
				source = 7,
				target = 8,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "PolyLineEdge" }
				},
			}, {
				source = 8,
				target = 9,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "EdgeLabel" }
				},
			}, {
				source = 0,
				target = 10,
				priority = -1,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "key" }
				},
			}, {
				source = 8,
				target = 11,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Arrows" }
				},
			}, {
				selector = { clazz = import("gla.xml.model.StructureGraphSelector") },
				target = 0,
			}, {
				source = 2,
				target = 1,
				priority = 1,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "graph" }
				},
			}, {
				source = 3,
				target = 12,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "UMLNoteNode" }
				},
			}, {
				source = 3,
				target = 13,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "ShapeNode" }
				},
			}, {
				source = 13,
				target = 14,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Shape" }
				},
			}, {
				source = 3,
				target = 15,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "UMLClassNode" }
				},
			}, {
				source = 15,
				target = 16,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "UML" }
				},
			}, {
				source = 16,
				target = 17,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "AttributeLabel" }
				},
			}, {
				source = 16,
				target = 18,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "MethodLabel" }
				},
			}, {
				source = 15,
				target = 21,
			}, {
				source = 3,
				target = 19,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "ProxyAutoBoundsNode" }
				},
			}, {
				source = 19,
				target = 20,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Realizers" }
				},
			}, {
				source = 20,
				target = 21,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "GroupNode" }
				},
			}, {
				source = 21,
				target = 5,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "NodeLabel" }
				},
			}, {
				source = 13,
				target = 21,
			}, {
				source = 21,
				target = 22,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Geometry" }
				},
			}, {
				source = 3,
				target = 23,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "TableNode" }
				},
			}, {
				source = 23,
				target = 21,
				priority = 1,
			}, {
				source = 5,
				target = 24,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "ModelParameter" }
				},
			}, {
				source = 24,
				target = 25,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "RowNodeLabelModelParameter" }
				},
			}, {
				source = 24,
				target = 26,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "ColumnNodeLabelModelParameter" }
				},
			}, {
				source = 23,
				target = 27,
				priority = 0,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Table" }
				},
			}, {
				source = 27,
				target = 28,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Columns" }
				},
			}, {
				source = 27,
				target = 29,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Rows" }
				},
			}, {
				source = 29,
				target = 30,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Row" }
				},
			}, {
				source = 28,
				target = 31,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Column" }
				},
			}, {
				source = 31,
				target = 32,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Columns" }
				},
			}, {
				source = 30,
				target = 32,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Rows" }
				},
			}, {
				source = 27,
				target = 33,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Insets" }
				},
			}, {
				source = 30,
				target = 33,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Insets" }
				},
			}, {
				source = 31,
				target = 33,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Insets" }
				},
			}, {
				source = 21,
				target = 34,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "StyleProperties" }
				},
			}, {
				source = 34,
				target = 35,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Property" }
				},
			}, {
				source = 20,
				target = 4,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "GenericGroupNode" }
				},
			}, {
				source = 7,
				target = 36,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "GenericEdge" }
				},
			}, {
				source = 8,
				target = 37,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "StyleProperties" }
				},
			}, {
				source = 36,
				target = 8,
			}, {
				source = 37,
				target = 38,
				iterator = {
					clazz = import("gla.xml.model.ChildIterator"),
					parameters = { filter = "Property" }
				},
			}
		]
	}
}

