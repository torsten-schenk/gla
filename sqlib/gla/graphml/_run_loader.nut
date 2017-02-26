local Walker = import("gla.xml.model.Walker")

return {
	nodes = [
		{ classname = "Document" }
		{ classname = "Graph" }
		{ classname = "Node" }
		{ classname = "NodeData" }
		{ classname = "GenericNode" }
		{ classname = "NodeLabel" }
		{ classname = "Edge" }
		{ classname = "EdgeData" }
		null
		{ classname = "EdgeLabel" }
		{ classname = "Key" }
		{ classname = "Arrows" }
		{ classname = "UmlNoteNode" }
		null
		{ classname = "Shape" }
		{ classname = "UmlClassNode" }
		{ classname = "UmlNode" }
		{ classname = "NodeAttributeLabel" }
		{ classname = "NodeMethodLabel" }
		null
		null
		null
		{ classname = "NodeGeometry" }
		{ classname = "TableNode" }
		null
		{ classname = "RowLabelParam" }
		{ classname = "ColLabelParam" }
		{ classname = "Table" }
		{ classname = "Cols" }
		{ classname = "Rows" }
		{ classname = "Row" }
		{ classname = "Col" }
		{ classname = "NestedError" }
		{ classname = "Insets" }
		null
		{ classname = "NodeStyle" }
		{ classname = "GenericEdge" }
		null
		{ classname = "EdgeStyle" }
		{ classname = "LineStyle" }
	]
	edges = [
		{
			source = 0
			target = 1
			priority = 1
			type = Walker.CHILDREN
			element = "graph"
		}
		{
			source = 1
			target = 2
			type = Walker.CHILDREN
			element = "node"
		}
		{
			source = 2
			target = 3
			type = Walker.CHILDREN
			element = "data"
		}
		{
			source = 3
			target = 4
			type = Walker.CHILDREN
			element = "GenericNode"
		}
		{
			source = 4
			target = 21
			type = Walker.NULL
		}
		{
			source = 1
			target = 6
			priority = 1
			type = Walker.CHILDREN
			element = "edge"
		}
		{
			source = 6
			target = 7
			type = Walker.CHILDREN
			element = "data"
		}
		{
			source = 7
			target = 8
			type = Walker.CHILDREN
			element = "PolyLineEdge"
		}
		{
			source = 8
			target = 9
			type = Walker.CHILDREN
			element = "EdgeLabel"
		}
		{
			source = 0
			target = 10
			priority = -1
			type = Walker.CHILDREN
			element = "key"
		}
		{
			source = 8
			target = 11
			type = Walker.CHILDREN
			element = "Arrows"
		}
		{
			source = null
			target = 0
			type = Walker.DOCUMENT
		}
		{
			source = 2
			target = 1
			priority = 1
			type = Walker.CHILDREN
			element = "graph"
		}
		{
			source = 3
			target = 12
			type = Walker.CHILDREN
			element = "UMLNoteNode"
		}
		{
			source = 3
			target = 13
			type = Walker.CHILDREN
			element = "ShapeNode"
		}
		{
			source = 13
			target = 14
			type = Walker.CHILDREN
			element = "Shape"
		}
		{
			source = 3
			target = 15
			type = Walker.CHILDREN
			element = "UMLClassNode"
		}
		{
			source = 15
			target = 16
			type = Walker.CHILDREN
			element = "UML"
		}
		{
			source = 16
			target = 17
			type = Walker.CHILDREN
			element = "AttributeLabel"
		}
		{
			source = 16
			target = 18
			type = Walker.CHILDREN
			element = "MethodLabel"
		}
		{
			source = 15
			target = 21
			type = Walker.NULL
		}
		{
			source = 3
			target = 19
			type = Walker.CHILDREN
			element = "ProxyAutoBoundsNode"
		}
		{
			source = 19
			target = 20
			type = Walker.CHILDREN
			element = "Realizers"
		}
		{
			source = 20
			target = 21
			type = Walker.CHILDREN
			element = "GroupNode"
		}
		{
			source = 21
			target = 5
			type = Walker.CHILDREN
			element = "NodeLabel"
		}
		{
			source = 13
			target = 21
			type = Walker.NULL
		}
		{
			source = 21
			target = 22
			type = Walker.CHILDREN
			element = "Geometry"
		}
		{
			source = 3
			target = 23
			type = Walker.CHILDREN
			element = "TableNode"
		}
		{
			source = 23
			target = 21
			priority = 1
			type = Walker.NULL
		}
		{
			source = 5
			target = 24
			type = Walker.CHILDREN
			element = "ModelParameter"
		}
		{
			source = 24
			target = 25
			type = Walker.CHILDREN
			element = "RowNodeLabelModelParameter"
		}
		{
			source = 24
			target = 26
			type = Walker.CHILDREN
			element = "ColumnNodeLabelModelParameter"
		}
		{
			source = 23
			target = 27
			type = Walker.CHILDREN
			element = "Table"
		}
		{
			source = 27
			target = 28
			type = Walker.CHILDREN
			element = "Columns"
		}
		{
			source = 27
			target = 29
			type = Walker.CHILDREN
			element = "Rows"
		}
		{
			source = 29
			target = 30
			type = Walker.CHILDREN
			element = "Row"
		}
		{
			source = 28
			target = 31
			type = Walker.CHILDREN
			element = "Column"
		}
		{
			source = 31
			target = 32
			type = Walker.CHILDREN
			element = "Columns"
		}
		{
			source = 30
			target = 32
			type = Walker.CHILDREN
			element = "Rows"
		}
		{
			source = 27
			target = 33
			type = Walker.CHILDREN
			element = "Insets"
		}
		{
			source = 30
			target = 33
			type = Walker.CHILDREN
			element = "Insets"
		}
		{
			source = 31
			target = 33
			type = Walker.CHILDREN
			element = "Insets"
		}
		{
			source = 21
			target = 34
			type = Walker.CHILDREN
			element = "StyleProperties"
		}
		{
			source = 34
			target = 35
			type = Walker.CHILDREN
			element = "Property"
		}
		{
			source = 20
			target = 4
			type = Walker.CHILDREN
			element = "GenericGroupNode"
		}
		{
			source = 7
			target = 36
			type = Walker.CHILDREN
			element = "GenericEdge"
		}
		{
			source = 8
			target = 37
			type = Walker.CHILDREN
			element = "StyleProperties"
		}
		{
			source = 36
			target = 8
			type = Walker.NULL
		}
		{
			source = 37
			target = 38
			type = Walker.CHILDREN
			element = "Property"
		}
		{
			source = 8
			target = 39
			type = Walker.CHILDREN
			element = "LineStyle"
		}
		{
			source = 12
			target = 21
			type = Walker.NULL
		}
	]
}
