cmake_minimum_required (VERSION 3.10)

project (siaskynetpp CXX)

set (SIASKYNETPP_LIBRARIES siaskynetpp CACHE INTERNAL "")
set (SIASKYNETPP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE INTERNAL "")

add_subdirectory (dependencies)

find_package(Threads REQUIRED)

add_library (${SIASKYNETPP_LIBRARIES} source/siaskynet.cpp source/siaskynet_multiportal.cpp "${SIASKYNETPP_INCLUDE_DIRS}/siaskynet.hpp" "${SIASKYNETPP_INCLUDE_DIRS}/siaskynet_multiportal.hpp")

target_link_libraries (${SIASKYNETPP_LIBRARIES} ${CPR_LIBRARIES} Threads::Threads)
target_include_directories (${SIASKYNETPP_LIBRARIES} PRIVATE ${JSON_INCLUDE_DIRS} PRIVATE ${CPR_INCLUDE_DIRS} PRIVATE ${SIASKYNETPP_INCLUDE_DIRS})

install (TARGETS ${SIASKYNETPP_LIBRARIES} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install (FILES include/siaskynet.hpp include/siaskynet_multiportal.hpp DESTINATION include)

add_executable (siaskynetpp_example example.cpp)
target_link_libraries (siaskynetpp_example ${SIASKYNETPP_LIBRARIES})
target_include_directories (siaskynetpp_example PRIVATE ${SIASKYNETPP_INCLUDE_DIRS})

add_subdirectory (tools)
