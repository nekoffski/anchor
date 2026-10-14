from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import cmake_layout


class AnchorConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    default_options = {
        "*:fPIC": True,
        "*:shared": False,
        "libarchive/*:with_zlib": True,
    }

    def requirements(self):
        self.requires("cli11/2.4.2")
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.14.1")
        self.requires("stduuid/1.2.3")
        self.requires("nlohmann_json/3.12.0")
        self.requires("asio/1.30.2")
        self.requires("libconfig/1.7.3")
        self.requires("libarchive/3.8.7")

    def build_requirements(self):
        self.test_requires("gtest/1.15.0")

    def validate(self):
        if self.settings.os not in ("Macos", "Linux"):
            raise ConanInvalidConfiguration(
                "Anchor currently supports only macOS and Linux."
            )

    def layout(self):
        cmake_layout(self)
