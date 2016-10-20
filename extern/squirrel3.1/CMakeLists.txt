set(squirrel_SOURCES
	squirrel/sqapi.cpp
	squirrel/sqbaselib.cpp
	squirrel/sqfuncstate.cpp
	squirrel/sqdebug.cpp
	squirrel/sqlexer.cpp
	squirrel/sqobject.cpp
	squirrel/sqcompiler.cpp
	squirrel/sqstate.cpp
	squirrel/sqtable.cpp
	squirrel/sqmem.cpp
	squirrel/sqvm.cpp
	squirrel/sqclass.cpp
)

set(squirrel_HEADERS
	include/gla/squirrel.h
)

set(sqstdlib_SOURCES
	sqstdlib/sqstdblob.cpp
	sqstdlib/sqstdio.cpp
	sqstdlib/sqstdstream.cpp
	sqstdlib/sqstdmath.cpp
	sqstdlib/sqstdsystem.cpp
	sqstdlib/sqstdstring.cpp
	sqstdlib/sqstdaux.cpp
	sqstdlib/sqstdrex.cpp
)

include_directories(include/gla)

add_library(squirrel ${squirrel_SOURCES})
add_library(sqstdlib ${sqstdlib_SOURCES})

