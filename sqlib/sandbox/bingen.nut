local ModelParser = import("gla.xml.model.Parser")
local ModelReader = import("gla.goprr.storage.ModelReader")
local ModelCreator = import("gla.goprr.storage.ModelCreator")
local XmlImporter = import("gla.bindb.model.XmlImporter")
local Database = import("gla.storage.memory.Database")
local IOSink = import("gla.textgen.IOSink")
local xmlmeta = import("gla.xml.model.meta")
local binmeta = import("gla.bindb.model.meta")
local Generator = import("gla.bindb.model.GenerateSqClasses")
local Printer = import("gla.textgen.Printer")
local PackagePath = import("gla.io.PackagePath")

local xmldb = Database()
local bindb = Database()

local parser = ModelParser(xmldb)
parser.trim = true
//parser.parse("sandbox.Document")
parser.parse("net.eyeeg.ngsu.DataScheme")

local converter = XmlImporter(ModelReader(xmldb), ModelCreator(bindb))
local StackExecuter = import("gla.util.StackExecuter")
StackExecuter(converter).execute()

local printer = Printer()
local gen = Generator(ModelReader(bindb))
gen.single(printer, "net.eyeeg.ngsu.DataSchemeQuirks")
StackExecuter(gen).execute()
printer.commit(IOSink(PackagePath("<nut>tmp.Bin", true)))

local def = import("tmp.Bin")
//local def = import("net.eyeeg.ngsu.DataScheme")
/*

local list = def.DriftCorrectFrame()
local io = PackagePath("<dat>tmp.serial", true).open("w")
list.serialize(io)
*/
