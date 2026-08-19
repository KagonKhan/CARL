from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class CARLRecipe(ConanFile):
    name = "carl"
    version = "0.1.0"
    package_type = "library"

    license = "BSD-2-Clause"
    description = "C++ library for parsing YAML config files into C++ structures"
    url = "https://github.com/KagonKhan/CARL"

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = "CMakeLists.txt", "cmake/*", "src/*", "include/*"

    def requirements(self):
        self.requires("yaml-cpp/0.9.0")
        self.requires("fmt/12.1.0")

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["carl"]
        self.cpp_info.set_property("cmake_file_name", "carl")
        self.cpp_info.set_property("cmake_target_name", "carl::carl")
