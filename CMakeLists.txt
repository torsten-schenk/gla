cmake_minimum_required(VERSION 2.8.8)

project(gla)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function")

#find_package(GLIB REQUIRED)
#include_directories(${GLIB_INCLUDES})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/extern/squirrel3.1/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/extern/date")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

find_package(Csv REQUIRED)
include_directories(${CSV_INCLUDE_DIR})

find_package(APR REQUIRED)
include_directories(${APR_INCLUDES})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${APR_CFLAGS}")

find_package(APRUtil REQUIRED)
include_directories(${APRUTIL_INCLUDE_DIR})

find_package(PCRE REQUIRED)
include_directories(${PCRE_INCLUDE_DIRS})

find_package(BTree REQUIRED)
include_directories(${BTREE_INCLUDE_DIRS})

find_package(GraVM REQUIRED)
include_directories(${GRAVM_INCLUDE_DIRS})

find_package(EXPAT REQUIRED)

set(gla_SOURCES
	rt.c preproc.c regex.c path.c store.c packreg.c entityreg.c mount.c io.c mnt_dir.c mnt_internal.c mnt_registry.c mnt_stdio.c

	storage/module.c storage/colspec.c storage/interface.c storage/memory.c storage/bdb.c
	xml/module.c xml/saxparser.c
	util/module.c util/stackexec.c util/exec.c util/time.c util/date.c
	io/module.c io/io.c io/buffer.c io/packpath.c io/hash.c
	csv/module.c csv/cbparser.c
	sqstdlib/module.c
	locale/module.c
)

set(gla_HEADERS
	core.h
)

set(alltest_SOURCES
)

set(gla_SOURCE_FILES)
set(gla_HEADER_FILES)
set(alltest_SOURCE_FILES)

foreach(x ${gla_SOURCES})
	list(APPEND gla_SOURCE_FILES "src/${x}")
endforeach()

foreach(x ${gla_HEADERS})
	list(APPEND gla_HEADER_FILES "include/gla/${x}")
endforeach()

foreach(x ${alltest_SOURCES})
	list(APPEND alltest_SOURCE_FILES "test/${x}")
endforeach()

macro(install_headers)
	foreach(_hdr ${ARGN})
		string(REGEX MATCH "(.*)[/]" _dir ${_hdr})
		string(REPLACE "include/" "" _dir ${_dir})
		install(FILES ${_hdr} DESTINATION /opt/gla/include/${_dir})
	endforeach()
	#	install(FILES src/ieg-db.h DESTINATION /opt/ieg/include)
endmacro()

add_subdirectory(extern/squirrel3.1)
add_subdirectory(extern/date)

add_executable(gla src/main.c ${gla_SOURCE_FILES} ${gla_HEADER_FILES})
target_link_libraries(gla squirrel sqstdlib date ${APR_LIBS} ${APRUTIL_LIBRARIES} ${PCRE_LIBRARIES} ${BTREE_LIBRARIES} ${GRAVM_LIBRARIES} ${EXPAT_LIBRARIES} ${CSV_LIBRARIES} -ldb -ldl)

install(TARGETS gla DESTINATION /opt/gla/bin)
install_headers(${gla_HEADER_FILES})
install(DIRECTORY sqlib DESTINATION /opt/gla)
#add_executable(alltest src/main.c ${gla_SOURCE_FILES} ${gla_HEADER_FILES})
#set_target_properties(alltest PROPERTIES COMPILE_FLAGS -DTESTING)
#target_link_libraries(alltest squirrel sqstdlib ${APR_LIBS} ${APRUTIL_LIBRARIES} ${PCRE_LIBRARIES} ${BTREE_LIBRARIES} ${GRAVM_LIBRARIES} -lcunit)

#cmake .. -DCMAKE_BUILD_TYPE=Debug

