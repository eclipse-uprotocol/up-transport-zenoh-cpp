# uprotocol-cpp-ulink-zenoh

## Welcome!

The main object of this module is to provide the C++ zenoh based uTransport

*_IMPORTANT NOTE:_ This project is under active development*

This module contains the implementation for pub-sub and RPC API`s defined in the uProtocol spec

## Getting Started
### Requirements:
- Compiler: GCC/G++ 11 or Clang 13
- Ubuntu 22.04
- conan : 1.59

#### Zenoh dependencies

At first to make it working, you have to install zenoh-c , using the following instructions https://github.com/eclipse-zenoh/zenoh-c/tree/master

```
$ git clone https://github.com/eclipse-uprotocol/up-client-zenoh-cpp.git
```

#### Building locally 

```
$ cd up-cpp-client-zenoh
$ mkdir build
$ cd build
$ conan install ../conaninfo
$ cmake ../
$ make -j 
```

#### Creating conan package locally 

ensure that the conan profile is configured to use ABI 11 (libstdc++11: New ABI.) standards according to https://docs.conan.io/en/1.60/howtos/manage_gcc_abi.html
```
$ cd up-cpp-client-zenoh
$ conan create . --build=missing
```

#### Compiling sample apps

ensure that the conan profile is configured to use ABI 11 (libstdc++11: New ABI.) standards according to https://docs.conan.io/en/1.60/howtos/manage_gcc_abi.html
```
$ cd up-cpp-client-zenoh
$ cd samples
$ mkdir build
$ cd build
$ conan install ../conaninfo
$ cmake ../
$ make -j
```

## Show your support

Give a ⭐️ if this project helped you!
