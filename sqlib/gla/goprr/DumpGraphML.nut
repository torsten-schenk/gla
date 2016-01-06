local Printer = import("gla.textgen.Printer")
local IOSink = import("gla.textgen.IOSink")

function escape(string) {
	local result = ""
	local start = 0

	for(local i = 0; i < string.len(); i++) {
		switch(string[i]) {
			case '<':
				result += string.slice(start, i)
				result += "&lt;"
				start = i + 1
				break;
			case '>':
				result += string.slice(start, i)
				result += "&gt;"
				start = i + 1
				break;
			case '"':
				result += string.slice(start, i)
				result += "&quot;"
				start = i + 1
				break;
			case "'"[0]:
				result += string.slice(start, i)
				result += "&apos;"
				start = i + 1
				break;
			case "&"[0]:
				result += string.slice(start, i)
				result += "&amp;"
				start = i + 1
				break;
			default:
				break;
		}
	}
	result += string.slice(start, string.len())
	return result
}

function mkrelationship(p, meta, id) {
	p.pni("<node id=\"rel" + id + "\">")
	p.pn("<data key=\"d5\"/>")
	p.pni("<data key=\"d6\">")
	p.pni("<y:GenericNode configuration=\"com.yworks.entityRelationship.relationship\">")
	p.pn("<y:Geometry height=\"56.0\" width=\"90.0\" x=\"1005.0\" y=\"302.0\"/>")
	p.pn("<y:Fill color=\"#E8EEF7\" color2=\"#B7C9E3\" transparent=\"false\"/>")
	p.pn("<y:BorderStyle color=\"#000000\" type=\"line\" width=\"1.0\"/>")
	p.pni("<y:NodeLabel alignment=\"center\" autoSizePolicy=\"content\" fontFamily=\"Dialog\" fontSize=\"12\" fontStyle=\"plain\" hasBackgroundColor=\"false\" hasLineColor=\"false\" height=\"17.96875\" modelName=\"custom\" textColor=\"#000000\" visible=\"true\" width=\"78.203125\" x=\"5.8984375\" y=\"19.015625\">" + (meta == null ? "ROOT" : meta.name + " (" + id + ")") + "<y:LabelModel>")
	p.ipn("<y:SmartNodeLabelModel distance=\"4.0\"/>")
	p.upn("</y:LabelModel>")
	p.pni("<y:ModelParameter>")
	p.pn("<y:SmartNodeLabelModelParameter labelRatioX=\"0.0\" labelRatioY=\"0.0\" nodeRatioX=\"0.0\" nodeRatioY=\"0.0\" offsetX=\"0.0\" offsetY=\"0.0\" upX=\"0.0\" upY=\"-1.0\"/>")
	p.upn("</y:ModelParameter>")
	p.upn("</y:NodeLabel>")
	p.pni("<y:StyleProperties>")
	p.pn("<y:Property class=\"java.lang.Boolean\" name=\"y.view.ShadowNodePainter.SHADOW_PAINTING\" value=\"true\"/>")
	p.upn("</y:StyleProperties>")
	p.upn("</y:GenericNode>")
	p.upn("</data>")
	p.upn("</node>")
}

function mkobject(p, meta, id, propit, indent) {
	p.pni("<node id=\"obj" + id + "\">")
	p.pn("<data key=\"d5\"/>")
	p.pni("<data key=\"d6\">")
	p.pni("<y:GenericNode configuration=\"com.yworks.entityRelationship.big_entity\">")
	p.pn("<y:Geometry height=\"94.0\" width=\"111.0\" x=\"754.5\" y=\"283.0\"/>")
	p.pn("<y:Fill color=\"#E8EEF7\" color2=\"#B7C9E3\" transparent=\"false\"/>")
	p.pn("<y:BorderStyle color=\"#000000\" type=\"line\" width=\"1.0\"/>")
	p.pn("<y:NodeLabel alignment=\"center\" autoSizePolicy=\"content\" backgroundColor=\"#B7C9E3\" configuration=\"com.yworks.entityRelationship.label.name\" fontFamily=\"Dialog\" fontSize=\"12\" fontStyle=\"plain\" hasLineColor=\"false\" height=\"17.96875\" modelName=\"internal\" modelPosition=\"t\" textColor=\"#000000\" visible=\"true\" width=\"39.033203125\" x=\"35.9833984375\" y=\"4.0\">" + meta.name + " (" + id + ")</y:NodeLabel>")
	p.pr("<y:NodeLabel alignment=\"left\" autoSizePolicy=\"content\" configuration=\"com.yworks.entityRelationship.label.attributes\" fontFamily=\"Dialog\" fontSize=\"12\" fontStyle=\"plain\" hasBackgroundColor=\"false\" hasLineColor=\"false\" height=\"45.90625\" modelName=\"custom\" textColor=\"#000000\" visible=\"true\" width=\"67.791015625\" x=\"2.0\" y=\"29.96875\">")
	local first = true
	p.unindent(indent + 5)
	for(; !propit.atEnd(); propit.next()) {
		if(!first)
			p.pn()
		first = false
		p.pr(propit.name() + "=" + escape(propit.value().tostring()))
	}
	p.indent(indent + 5)
	p.pni("<y:LabelModel>")
	p.pn("<y:ErdAttributesNodeLabelModel/>")
	p.upn("</y:LabelModel>")
	p.pni("<y:ModelParameter>")
	p.ipn("<y:ErdAttributesNodeLabelModelParameter/>")
	p.upn("</y:ModelParameter>")
	p.upn("</y:NodeLabel>")
	p.pni("<y:StyleProperties>")
	p.pn("<y:Property class=\"java.lang.Boolean\" name=\"y.view.ShadowNodePainter.SHADOW_PAINTING\" value=\"true\"/>")
	p.upn("</y:StyleProperties>")
	p.upn("</y:GenericNode>")
	p.upn("</data>")
	p.upn("</node>")
}

function mkbinding(p, meta, index, source, target, reverse) {
	if(reverse)
		p.pni("<edge id=\"role" + index + "\" source=\"obj" + target + "\" target=\"rel" + source + "\">")
	else
		p.pni("<edge id=\"role" + index + "\" source=\"rel" + source + "\" target=\"obj" + target + "\">")
	p.pn("<data key=\"d9\"/>")
	p.pni("<data key=\"d10\">")
	p.pni("<y:PolyLineEdge>")
	p.pn("<y:Path sx=\"0.0\" sy=\"0.0\" tx=\"0.0\" ty=\"0.0\"/>")
	p.pn("<y:LineStyle color=\"#000000\" type=\"line\" width=\"1.0\"/>")
	p.pn("<y:Arrows source=\"none\" target=\"standard\"/>")
	p.pni("<y:EdgeLabel alignment=\"center\" configuration=\"AutoFlippingLabel\" distance=\"2.0\" fontFamily=\"Dialog\" fontSize=\"12\" fontStyle=\"plain\" hasBackgroundColor=\"false\" hasLineColor=\"false\" height=\"17.96875\" modelName=\"custom\" preferredPlacement=\"anywhere\" ratio=\"0.5\" textColor=\"#000000\" visible=\"true\" width=\"38.60546875\" x=\"50.4296875\" y=\"-38.984375\">" + meta.name + "<y:LabelModel>")
	p.ipn("<y:SmartEdgeLabelModel autoRotationEnabled=\"false\" defaultAngle=\"0.0\" defaultDistance=\"10.0\"/>")
	p.upn("</y:LabelModel>")
	p.pni("<y:ModelParameter>")
	p.pn("<y:SmartEdgeLabelModelParameter angle=\"0.0\" distance=\"30.0\" distanceToCenter=\"true\" position=\"left\" ratio=\"0.5\" segment=\"0\"/>")
	p.upn("</y:ModelParameter>")
	p.pn("<y:PreferredPlacementDescriptor angle=\"0.0\" angleOffsetOnRightSide=\"0\" angleReference=\"absolute\" angleRotationOnRightSide=\"co\" distance=\"-1.0\" frozen=\"true\" placement=\"anywhere\" side=\"anywhere\" sideReference=\"relative_to_edge_flow\"/>")
	p.upn("</y:EdgeLabel>")
	p.pn("<y:BendStyle smoothed=\"false\"/>")
	p.upn("</y:PolyLineEdge>")
	p.upn("</data>")
	p.upn("</edge>")
}

return class {
	root = true

	constructor() {
	}

	function dump(reader, baseentity) {
		for(local graphit = reader.graphs(); !graphit.atEnd(); graphit.next()) {
			local p = Printer()
			local sink = IOSink("<graphml>" + baseentity + "_" + graphit.id())
			local encountered = {}
			local metagraph = reader.metanp(graphit.id())

			p.pn("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>")
			p.pni("<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\" xmlns:java=\"http://www.yworks.com/xml/yfiles-common/1.0/java\" xmlns:sys=\"http://www.yworks.com/xml/yfiles-common/markup/primitives/2.0\" xmlns:x=\"http://www.yworks.com/xml/yfiles-common/markup/2.0\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:y=\"http://www.yworks.com/xml/graphml\" xmlns:yed=\"http://www.yworks.com/xml/yed/3\" xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns http://www.yworks.com/xml/schema/graphml/1.1/ygraphml.xsd\">")
			p.pn("<!--Created by yEd 3.14.2-->")
			p.pn("<key attr.name=\"Description\" attr.type=\"string\" for=\"graph\" id=\"d0\"/>")
			p.pn("<key for=\"port\" id=\"d1\" yfiles.type=\"portgraphics\"/>")
			p.pn("<key for=\"port\" id=\"d2\" yfiles.type=\"portgeometry\"/>")
			p.pn("<key for=\"port\" id=\"d3\" yfiles.type=\"portuserdata\"/>")
			p.pn("<key attr.name=\"url\" attr.type=\"string\" for=\"node\" id=\"d4\"/>")
			p.pn("<key attr.name=\"description\" attr.type=\"string\" for=\"node\" id=\"d5\"/>")
			p.pn("<key for=\"node\" id=\"d6\" yfiles.type=\"nodegraphics\"/>")
			p.pn("<key for=\"graphml\" id=\"d7\" yfiles.type=\"resources\"/>")
			p.pn("<key attr.name=\"url\" attr.type=\"string\" for=\"edge\" id=\"d8\"/>")
			p.pn("<key attr.name=\"description\" attr.type=\"string\" for=\"edge\" id=\"d9\"/>")
			p.pn("<key for=\"edge\" id=\"d10\" yfiles.type=\"edgegraphics\"/>")
			p.pni("<graph edgedefault=\"directed\" id=\"G\">")
			p.pn("<data key=\"d0\"/>")

			for(local objectit = reader.objects(); !objectit.atEnd(); objectit.next())
				if(reader.isbound(objectit.id(), graphit.id()))
					mkobject(p, objectit.meta(), objectit.id(), objectit.properties(), 0)

			if(root && reader.hasroot(graphit.id()))
				mkrelationship(p, null, 0)
			for(local relit = reader.relationships(graphit.id()); !relit.atEnd(); relit.next())
				mkrelationship(p, relit.meta(), relit.id())

			for(local bindit = reader.relbindings(graphit.id()); !bindit.atEnd(); bindit.next()) {
				local metabinding
				if(bindit.source() == 0)
					metabinding = metagraph.root
				else
					metabinding = metagraph.binding[reader.metanp(bindit.source(), graphit.id())]
				if(bindit.source() != 0 || root)
					mkbinding(p, bindit.metarole(), bindit.at(), bindit.source(), bindit.target(), bindit.metarole() in metabinding.incoming)
			}

			p.upn("</graph>")
			p.pni("<data key=\"d7\">")
			p.pn("<y:Resources/>")
			p.upn("</data>")
			p.upn("</graphml>")

			p.commit(sink)
		}
	}
}

