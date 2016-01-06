local ModelParser = import("gla.xml.model.Parser")
local ModelReader = import("gla.goprr.storage.ModelReader")
local ModelCreator = import("gla.goprr.storage.ModelCreator")
local XmlImporter = import("gla.bindb.model.XmlImporter")
local Database = import("gla.storage.memory.Database")
local Dump = import("gla.goprr.DumpGraphML")
local IOSink = import("gla.textgen.IOSink")
local xmlmeta = import("gla.xml.model.meta")
local binmeta = import("gla.bindb.model.meta")

local xmldb = Database()
local bindb = Database()

local parser = ModelParser(xmldb)
parser.trim = true
//parser.parse("sandbox.Document")
parser.parse("net.eyeeg.ngsu.DataScheme")



/*local dump = Dump()
dump.dump(ModelReader(xmldb, xmlmeta), "tmp.test")
return*/




local converter = XmlImporter(ModelReader(xmldb), ModelCreator(bindb))
local StackExecuter = import("gla.util.StackExecuter")
StackExecuter(converter).execute()


local dump = Dump()
dump.root = false
dump.dump(ModelReader(bindb), "tmp.test")


/*local reader = ModelReader(bindb)
for(local propit = reader.properties(647); !propit.atEnd(); propit.next())
	print("PROPERTY: " + propit.name() + " " + propit.value())
local table = bindb.open("global")
for(local i = 0; i < table.len(); i++) {
	local row = table.rowAt(i)
	if(row[3] == "triggers")
		print("FOUND ROW: " + row[0] + " " + row[1] + " " + row[2] + " " + row[3])
}
*/
