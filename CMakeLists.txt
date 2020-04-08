add_subdirectory (dependencies)

include_directories (${JSON_ICLUDE_DIRS} ${CPR_INCLUDE_DIRS})

# organization ideas:
#	- this could be just a backend library, and include cpr etc internally

add_executable (test test.cpp)
target_link_libraries (test ${CPR_LIBRARIES})



