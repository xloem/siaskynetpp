find_package(OpenSSL REQUIRED)

add_executable (stream-up stream-up.cpp)
target_link_libraries (stream-up ${SIASKYNETPP_LIBRARIES} OpenSSL::Crypto)
target_include_directories (stream-up PRIVATE ${SIASKYNETPP_INCLUDE_DIRS} PRIVATE ${JSON_INCLUDE_DIRS})

add_executable (stream-down stream-down.cpp)
target_link_libraries (stream-down ${SIASKYNETPP_LIBRARIES} OpenSSL::Crypto)
target_include_directories (stream-down PRIVATE ${SIASKYNETPP_INCLUDE_DIRS} PRIVATE ${JSON_INCLUDE_DIRS})
