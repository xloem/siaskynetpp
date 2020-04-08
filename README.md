# siaskynetpp
A C++ interface to Sia Skynet.  See https://siasky.net .

## Usage
The interface is simple.  See example.cpp for example code.

To use this library without requiring its installation, include it as a submodule:
```
$ git submodule add git@github.com:xloem/siaskynetpp siaskynetpp
$ git submodule update --init --recursive
```

Then you can include it in a CMakeLists.txt that's anything like this:
```
cmake_minimum_required (VERSION 3.10)

add_subdirectory (siaskynetpp)

link_libraries (${SIASKYNETPP_LIBRARIES})
include_directories (${SIASKYNETPP_INCLUDE_DIRS})

add_executable (example example.cpp)
```

To build your project with cmake:
```
$ mkdir build
$ cd build
$ cmake ..
$ make
$ ./example
```
