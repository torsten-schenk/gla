run("sandbox.importme");
local Map = import("gla.util.Map");

local table = {};

table["asd"] <- "bla";
table["asd"] <- "dsa";

print("TABLE: " + table["asd"] + "\n");

local map = Map(6);

map[["A", "AA", "AAA", "AAAA", "AAAAA", "AAAAAA"]] <- "AAAAAAA"

local submap = Map(map, ["A", "AA"]);

print("GET: " + map[["A", "AA", "AAA", "AAAA", "AAAAA", "AAAAAA"]] + " " + submap[["AAA", "AAAA", "AAAAA", "AAAAAA"]] + "\n");

submap[["AAB", "AAAA", "AAAAA", "AAAAAA"]] <- "AABAAAA"
print("GET: " + map[["A", "AA", "AAB", "AAAA", "AAAAA", "AAAAAA"]] + "\n");

submap[["AAB", "AAAA", "AAAAA", "AAAAAA"]] = "AABAAAB"
print("GET: " + map[["A", "AA", "AAB", "AAAA", "AAAAA", "AAAAAA"]] + "\n");

/*delete submap[["AAB", "AAAA", "AAAAA", "AAAAAA"]]
print("GET: " + map[["A", "AA", "AAB", "AAAA", "AAAAA", "AAAAAA"]] + "\n");*/

local subsubmap = Map(submap, ["AAA", "AAAA", "AAAAA"])
subsubmap["AAAAAB"] <- "AAAAABA";
print("GET: " + subsubmap["AAAAAA"] + " " + map[["A", "AA", "AAB", "AAAA", "AAAAA", "AAAAAA"]] + "\n");

