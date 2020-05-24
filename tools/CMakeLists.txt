
add_executable (siaskynet siaskynet.cpp)
target_link_libraries (siaskynet siaskynetpp)

find_package(OpenSSL REQUIRED)

add_executable (stream-up stream-up.cpp)
target_link_libraries (stream-up siaskynetpp OpenSSL::Crypto)
target_include_directories (stream-up PRIVATE ${JSON_INCLUDE_DIRS})

add_executable (stream-down stream-down.cpp)
target_link_libraries (stream-down siaskynetpp OpenSSL::Crypto)
target_include_directories (stream-down PRIVATE ${JSON_INCLUDE_DIRS})
