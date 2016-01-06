local Path = import("gla.io.PackagePath")
local strlib = import("squirrel.stringlib")

local regex = strlib.regexp(@"([^_]+)_suffix.jpeg")
local p = Path("sys.cwd")
for(p.select(); p.isValid(); p.next()) {
	local cap = regex.capture(p.entity())
	if(cap != null) {
		local renamed = p.entity().slice(cap[1].begin, cap[1].end) + "_newsuffix.jpg"
		p.rename(renamed)
	}
}

