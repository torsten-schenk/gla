local PathUtil = import("gla.io.PathUtil")
local strlib = import("squirrel.stringlib")
local SimpleTable = import("gla.storage.SimpleTable")

local identifier = "[a-zA-Z_\\-][a-zA-Z0-9_\\-]*"
local regexSnippet = strlib.regexp("^([\\t]*)\\*\\*\\*\\s*(" + identifier + ")?\\s*(@" + identifier + ")?\\s*(\\{.*)?$")
local regexLineComment = strlib.regexp("^[\\t]*\\*\\*\\*#.*$")
local regexCommentStart = strlib.regexp("^[\\t]*\\*\\*\\*\\[.*$")
local regexCommentEnd = strlib.regexp(".*\\]\\*\\*\\*\\s*$")
local regexWarn = strlib.regexp("^[\\t]*(\\*\\*\\*\\s*.*)$")
local regexText = strlib.regexp("^([\\t]*)(.*)$")
local regexEmpty = strlib.regexp("^[\\s]*$")

local Runtime = class {
	stack = null
	Snippets = null
	DefaultSnippet = null
	inComment = false
	emptyLines = 0

	descend = null //[ parent name ] -> [ child ]
	ascend = null // [ child ] -> [ parent name ]

	constructor() {
		stack = []
		Snippets = {}
		DefaultSnippet = import("gla.snippet.DefaultSnippet")
		descend = SimpleTable(2, 1)
		ascend = SimpleTable(1, 2)
	}
}

local processSection = function(rt, line) {
	local cap
	if(rt.inComment) {
		cap = regexCommentEnd.capture(line)
		if(cap != null)
			rt.inComment = false
		return true
	}

	cap = regexCommentStart.capture(line)
	if(cap != null) {
		rt.inComment = true
		return true
	}

	cap = regexLineComment.capture(line)
	if(cap != null)
		return true

	cap = regexSnippet.capture(line)
	if(cap != null) {
		local depth = cap[1].end - cap[1].begin
		local name = line.slice(cap[2].begin, cap[2].end)
		local snippetName = null
		local context = null
		if(cap[3].begin < cap[3].end)
			snippetName = line.slice(cap[3].begin + 1, cap[3].end)
		if(cap[4].begin < cap[4].end)
			context = eval("return " + line.slice(cap[4].begin, cap[4].end))
		if(snippetName != null && !(snippetName in rt.Snippets))
			throw "no such snippet class: '" + snippetName + "'"
		local Snippet
		if(snippetName != null)
			Snippet = rt.Snippets[snippetName]
		else if(rt.stack.len() == 0)
			Snippet = rt.DefaultSnippet
		else
			Snippet = rt.stack.top().getclass()

		if(rt.stack.len() == 0 && depth != 0)
			throw "invalid first snippet: must not be indented"
		else if(depth > rt.stack.len())
			throw "indentation of child snippet too many levels below parent"
		while(rt.stack.len() > depth) {
			local cur = rt.stack.top()
			if(cur.newlineBeforeFinish)
				cur.newline()
			cur.finish()
			rt.stack.pop()
		}
		local parent = null
		if(rt.stack.len() > 0)
			parent = rt.stack.top()
		local snippet = Snippet(context, parent)
		if(parent != null) {
			if(parent.newlineBeforeEmbed) {
				for(local i = 0; i <= rt.emptyLines; i++)
					parent.newline()
			}
			parent.embed(snippet)
			if(parent.newlineAfterEmbed)
				parent.newline()
		}
		rt.stack.push(snippet)
		rt.descend.insert([ parent name ], [ snippet ])
		rt.ascend.insert([ snippet ], [ parent name ])
		return true
	}

	cap = regexWarn.capture(line)
	if(cap != null) {
		print("Warning: invalid line directive '" + line.slice(cap[1].begin, cap[1].end) + "'")
		return false
	}
	return false
}

local processEmpty = function(rt, line) {
	return line.len() == 0 || regexEmpty.capture(line) != null
}

local processText = function(rt, line) {
	local cap = regexText.capture(line)
	local depth = cap[1].end - cap[1].begin
	if(depth == 0) //ignore top-level text here
		return false
	local cur = rt.stack.top()
	local text = line.slice(depth)
	print("GOT TEXT: '" + text + "' " + depth + " " + rt.stack.len())

	while(rt.stack.len() > depth) {
		local cur = rt.stack.top()
		if(cur.newlineBeforeFinish)
			cur.newline()
		cur.finish()
		rt.stack.pop()
	}
	for(local i = 0; i <= rt.emptyLines; i++)
		cur.newline()
	cur.text(text)
}

return class {
	meta = null

	ascend = null
	descend = null

	constructor(meta) {
		this.meta = meta
	}

	function parse(entity) {
		entity = PathUtil.canonicalizeEntity(entity, "snip")
		local io = open(entity, "r")
		local lines = strlib.split(io.readfull(), "\n") //TODO read file line-by-line instead
		local rt = Runtime()
		io.close()
		foreach(i, v in lines) {
			if((rt.stack.len() == 0 || !rt.stack.top().raw) && processSection(rt, v)) {
				rt.emptyLines = -1
			}
			else if(processEmpty(rt, v)) {
				rt.emptyLines++
			}
			else if(processText(rt, v)) {
				rt.emptyLines = 0
			}
		}

		ascend = rt.ascend
		descend = rt.descend
	}

	function dump(parent = null, indent = 0) {
		local out = ""
		for(local i = 0; i < indent; i++)
			out += "  "
		if(parent == null)
			print(out + "<root>")
		else
			print(out + ascend.value(parent)[1])
		for(local it = descend.group([ parent ]); !it.atEnd(); it.next()) {
			dump(it.cell(2), indent + 1)
		}
	}
}

