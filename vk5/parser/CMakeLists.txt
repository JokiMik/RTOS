
cmake_minimum_required(VERSION 3.24.0)

set (This TimeParser)

project(${This} CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add static linking flags for the entire project
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")

enable_testing()

add_subdirectory(googletest)

set(Headers
	TimeParser.h
)
set(Sources
	TimeParser.cpp
)

add_library(${This} STATIC ${Sources} ${Headers})

add_subdirectory(test_cases)


