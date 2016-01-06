local MetaModel = import("gla.goprr.MetaModel");

dump.opstack(MetaModel);

local metaModel = MetaModel();

local xmlMeta = import("gla.xml.model.meta");

dump.opstack(MetaModel);

dump.opstack(xmlMeta);
print("The value is: " + xmlMeta.graph("blubb") + " " + xmlMeta.bla() + "\n");

print((xmlMeta instanceof MetaModel) + "\n");
