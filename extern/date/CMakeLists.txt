set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-function -std=c++11")
set(date_SOURCES
	interface.cpp)

add_library(date ${date_SOURCES})

