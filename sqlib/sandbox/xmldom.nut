print("Importing Document class...\n");

local Document = import("gla.xml.dom.Document");

print("  --> done\n");

print("DOCUMENT IS: " + Document + ", " + typeof(Document) + "\n");

local inst = Document("sandbox.Document");

/*local other = import("sandbox.importme");
print("imported: " + other + "\n");*/

