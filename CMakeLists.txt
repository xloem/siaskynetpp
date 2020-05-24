cmake_minimum_required (VERSION 3.10)

project (siaskynetpp CXX)

set (SIASKYNETPP_LIBRARIES siaskynetpp CACHE INTERNAL "")
set (SIASKYNETPP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE INTERNAL "")

include(CMakeDependentOption)
cmake_dependent_option(SIASKYNETPP_WITH_INSTALL
    "With install target" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

add_subdirectory (dependencies)

add_library (siaskynetpp source/siaskynet.cpp "${SIASKYNETPP_INCLUDE_DIRS}/siaskynet.hpp")

target_link_libraries (siaskynetpp
	PRIVATE
		${CPR_LIBRARIES}
		ghc_filesystem
)
target_include_directories (siaskynetpp
	PRIVATE
		${JSON_INCLUDE_DIRS}
		${CPR_INCLUDE_DIRS}
	PUBLIC
		$<BUILD_INTERFACE:${SIASKYNETPP_INCLUDE_DIRS}>
		$<INSTALL_INTERFACE:include>
)

install (TARGETS siaskynetpp RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install (FILES include/siaskynet.hpp DESTINATION include)

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT hasParent)
	add_executable (siaskynetpp_example example.cpp)
	target_link_libraries (siaskynetpp_example siaskynetpp)

	add_subdirectory (tools)
endif()

if(SIASKYNETPP_WITH_INSTALL)
    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS siaskynetpp siaskynet)
    #cmake bugs with interface dependencies
    #install(TARGETS siaskynetpp EXPORT siaskynetppConfig)
    #install(EXPORT siaskynetppConfig NAMESPACE siaskynetpp:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/siaskynetpp)
endif()
