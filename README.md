# uProtocol C++ Zenoh Transport (up-transport-zenoh-cpp)

## Welcome!

This library provides a Zenoh-based uProtocol transport for C++ uEntities.

_*IMPORTANT NOTE:* This project is under active development_

This module contains the Zenoh implementation of the Layer 1 `UTransport` API
from [up-cpp][cpp-api-repo].

## Getting Started

### Requirements:

- Compiler: GCC/G++ 11 or Clang 13
- Conan : 1.59 or latest 2.X

#### Basic Conan packages

Using the recipes found in [up-conan-recipes][conan-recipe-repo], build these
Conan packages:

1. [up-core-api][spec-repo] -
   `conan create --version 1.6.0-alpha4 --build=missing up-core-api/release`
2. [up-cpp][cpp-api-repo] -
   `conan create --version 1.0.1-dev --build=missing up-cpp/developer -o commitish=af55b7899fb8d2e30de1b11f975750e9d1135bbd -o fork=qnx-ports/up-cpp`

#### Zenoh Conan packages

We support two basic zenoh backend (zenoh-c/zenoh-pico),
also to have backward compatibility support temporary zenoh-c packaging.

Building (Temporary) Zenoh Packages
1. [zenoh-c][zenoh-repo] - `conan create --version 1.5.0 zenohc-tmp/prebuilt`
2. [zenoh-cpp][zenoh-repo] -
   `conan create --version 1.5.0 zenohcpp-tmp/from-source`

Building Zenoh Packages - with zenoh-c backend
1. [zenoh-c][zenoh-repo] - `conan create --version 1.5.0 zenoh-c/prebuilt`
2. [zenoh-cpp][zenoh-repo] -
   `conan create --version 1.5.0 zenoh-cpp -o backend=zenoh-c`

Building Zenoh Packages - with zenoh-pico backend
1. [zenoh-pico][zenoh-repo] - `conan create --version 1.5.0 zenoh-pico`
2. [zenoh-cpp][zenoh-repo] -
   `conan create --version 1.5.0 zenoh-cpp -o backend=zenoh-pico`

**NOTE:** all `conan` commands in this document use Conan 2.x syntax. Please
adjust accordingly when using Conan 1.x.

## How to Use the Library

To add up-transport-zenoh-cpp to your conan build dependencies, place following
in your conanfile.txt:

```text
[requires]
up-transport-zenoh-cpp/[>=1.0.0 <2.0.0]

[generators]
CMakeDeps
CMakeToolchain

[layout]
cmake_layout
```

**NOTE:** If using conan version 1.59 Ensure that the conan profile is
configured to use ABI 11 (libstdc++11: New ABI) standards according to [the
Conan documentation for managing gcc ABIs][conan-abi-docs].

## Building locally

The following steps are only required for developers to locally build and test
up-transport-zenoh-cpp, If you are making a project that uses
up-transport-zenoh-cpp, follow the steps in the
[How to Use the Library](#how-to-use-the-library) section above.

### With Conan for dependencies

```bash
cd up-transport-zenoh-cpp

# Default is (Temporary) Zenoh Packages
conan install . --build=missing
# OR explicite zenoh-c backend
conan install . --build=missing -o backend=zenoh-c
# OR explicite zenoh-pico backend
conan install . --build=missing -o backend=zenoh-pico

cmake --preset conan-release
cd build/Release
cmake --build . -- -j
```

Once the build completes, tests can be run with `ctest`.

**NOTE:** If you want to test with zenoh-pico backend you have to run
zenoh-router. Please look at [up-conan-readme][conan-recipe-docs]

### QNX build with conan
```bash
cd up-conan-recipes
conan create --version=3.21.12 --build=missing protobuf
conan config install tools/qnx-8.0-extension/settings_user.yml
conan create -pr:h=tools/profiles/nto-8.0-x86_64 --version=3.21.12 --build=missing protobuf
conan create -pr:h=tools/profiles/nto-8.0-x86_64 --version=1.6.0-alpha4 up-core-api/release/
conan create -pr:h=tools/profiles/nto-8.0-x86_64 --version=1.14.0 gtest

conan create -pr:h=tools/profiles/nto-8.0-x86_64 --version=1.0.1-dev --build=missing up-cpp/developer -o commitish=af55b7899fb8d2e30de1b11f975750e9d1135bbd -o fork=qnx-ports/up-cpp

conan create -pr:h=tools/profiles/nto-8.0-x86_64 --version=1.5.0 zenoh-pico
conan create -pr:h=tools/profiles/nto-8.0-x86_64 --version=1.5.0 -o backend=zenoh-pico zenoh-cpp

cd ../up-transport-zenoh-cpp
conan install -pr:h=../up-conan-recipes/tools/profiles/nto-8.0-x86_64 --build=missing -o backend=zenoh-pico .
cmake --preset conan-release
cd build/Release
cmake --build . -- -j
```


### With dependencies installed as system libraries

**TODO** Verify steps for pure cmake build without Conan.

### Generate UT Coverage

To get code coverage, perform the steps above, but replace `cmake --preset...`
with

```bash
cd build/Release
cmake ../../ -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Coverage
```

Once the tests complete, the Unit Test Coverage report can be generated from the
base up-cpp folder with: ./Coverage.sh

```bash
./coverage.sh
```

### Creating the Conan package

See: [up-conan-recipes][conan-recipe-repo]

## Show your support

Give a ⭐️ if this project helped you!

[conan-recipe-repo]: https://github.com/eclipse-uprotocol/up-conan-recipes
[spec-repo]: https://github.com/eclipse-uprotocol/up-spec
[cpp-api-repo]: https://github.com/eclipse-uprotocol/up-cpp
[zenoh-repo]: https://github.com/eclipse-zenoh/zenoh-cpp
[conan-abi-docs]: https://docs.conan.io/en/1.60/howtos/manage_gcc_abi.html
[conan-recipe-docs]: https://github.com/eclipse-uprotocol/up-conan-recipes/blob/main/README.md
