# uprotocol-cpp-ulink-zenoh

## Welcome!

The main object of this module is to provide the C++ zenoh based uTransport

*_IMPORTANT NOTE:_ This project is under active development*

This module contains the implementation for pub-sub and RPC API`s defined in the uProtocol spec

## Getting Started
### Requirements:
- Compiler: GCC/G++ 11 or Clang 13
- Ubuntu 22.04
- conan : 1.59 or latest 2.X

#### Zenoh dependencies

1. install up-cpp library https://github.com/eclipse-uprotocol/up-cpp
2. install zenoh-c , using the following instructions https://github.com/eclipse-zenoh/zenoh-c/tree/master

```
$ git clone https://github.com/eclipse-uprotocol/up-client-zenoh-cpp.git
```
## How to Use the Library
To add up-cpp to your conan build dependencies, simply add the following to your conanfile.txt:
```
[requires]
up-cpp/0.1
up-cpp-client-zenoh/0.1
protobuf/3.21.12

[generators]
CMakeDeps
CMakeToolchain

[layout]
cmake_layout

```
**NOTE:** If using conan version 1.59 Ensure that the conan profile is configured to use ABI 11 (libstdc++11: New ABI.) standards according to https://docs.conan.io/en/1.60/howtos/manage_gcc_abi.html

### Building locally 
```
$ cd up-cpp-client-zenoh
$ conan install conaninfo/  --output-folder=.
$ cd build/Release
$ cmake ../../ -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
$ make -j 
```

#### Creating conan package locally 

```
$ cd up-cpp-client-zenoh
$ conan create . 
```

## Show your support

Give a ⭐️ if this project helped you!
