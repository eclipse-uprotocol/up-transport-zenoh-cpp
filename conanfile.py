from conan import ConanFile, tools
from conans import ConanFile, CMake

from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
import shutil

class up_client_zenog_cpp(ConanFile):
    name = "up-client-zenoh-cpp"
    version = "0.1"

    # Optional metadata
    license = "Apache-2.0 license"
    url = "https://github.com/eclipse-uprotocol/up-client-zenoh-cpp"
    description = "C++ uLink Library for zenoh transport"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [False, False]}
    default_options = {"shared": True, "fPIC": False}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "lib/*"
    requires = [
        "up-cpp/0.1"
    ]
    generators = "CMakeDeps"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_testing": [True, False],
        "build_unbundled": [True, False],
        "build_cross_compiling": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": False,
        "build_testing": False,
        "build_unbundled": False,
        "build_cross_compiling": False,
    }

    # def configure(self):
    #     self.options["up-cpp"].shared = True

    def requirements(self):
        if self.options.build_unbundled:
            self.requires("up-cpp/1.5.1")
            self.requires("zenohc/cci.20240213")
            self.requires("protobuf/3.21.12" + ("@cross/cross" if self.options.build_cross_compiling else ""))
        else:
            self.requires("up-cpp/0.1")
            self.requires("spdlog/1.13.0")
            self.requires("protobuf/3.21.12")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["up-client-zenoh-cpp"]
