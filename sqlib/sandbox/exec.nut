local Buffer = import("gla.io.Buffer")
local Executer = import("gla.util.Executer")

local e = Executer("ls", "/")
//local stdin = e.stdin()
local stdout = e.stdout()
//local stderr = e.stderr()
e.execute()
local buffer = Buffer()
stdout.read(buffer)
e.wait(true)

print("SIZE: " + buffer.size())
print("GOT: '" + buffer.tostring() + "'")

while(!buffer.reof()) {
	print(buffer.read("u8").tochar())
}

