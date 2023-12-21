# uprotocol-cpp-ulink-zenoh

## Welcome!

The main object of this module is to provide the C++ zenoh based uTransport

*_IMPORTANT NOTE:_ This project is under active development*

This module contains the implementation for pub-sub and RPC API`s defined in the uProtocol spec

## Getting Started
### Requirements:
- Compiler: GCC/G++ 11 or Clang 13
- vcpkg
- Ubuntu 22.04
- cgreen testing library

#### Zenoh dependencies

At first to make it working, you have to install zenoh-c , using the following instructions https://github.com/eclipse-zenoh/zenoh-c/tree/master

#### UPROTOCOL-CPP and UPROTOCOL-CORE-API dependencies
```
$ git clone https://github.com/eclipse-uprotocol/uprotocol-cpp.git
$ git clone https://github.com/eclipse-uprotocol/uprotocol-core-api.git
```
### Setup main CMAKE file, build and test
```
project(uprotocol LANGUAGES C CXX)

# add your repository or module
add_subdirectory(uprotocol-core-api)
add_subdirectory(uprotocol-cpp)
add_subdirectory(uprotocol-cpp-ulink-zenoh)
```
### compile
```
mkdir build
cd build
cmake ../
make
```
## Show your support

Give a ⭐️ if this project helped you!
