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

    def requirements(self):
        self.requires("up-cpp/0.1")

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
