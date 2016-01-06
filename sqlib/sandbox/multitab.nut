local MultiTable = import("gla.util.MultiTable")

local tab = MultiTable(2, 3)

tab[[1, 2]] <- [3, 4, 5]

local val = tab[[1, 2]];
dump.opstack(val);
print("GET: " + tab[[1, 2]][0] + " " + tab[[1, 2]][1] + " " + tab[[1, 2]][2] + "\n");
