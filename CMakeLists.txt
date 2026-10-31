cmake_minimum_required(VERSION 3.16)
project(RandomSaver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(RandomSaver MODULE RandomSaver.cpp)

# The screen saver preferences app lists and loads the files in the
# "Screen Savers" directories by name, so the add-on has to be a file
# called exactly "RandomSaver": no "lib" prefix, no ".so" suffix.
set_target_properties(RandomSaver PROPERTIES
	PREFIX ""
	SUFFIX ""
	OUTPUT_NAME "RandomSaver")

target_link_libraries(RandomSaver PRIVATE be screensaver)
