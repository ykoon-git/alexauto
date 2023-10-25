import os, glob, logging
from conans import ConanFile, CMake, tools


class AvsDeviceSdkConan(ConanFile):

    python_requires = "aac-sdk-tools/1.0"
    python_requires_extend = "aac-sdk-tools.BaseSdkDependency"

    name = "avs-device-sdk"
    version = "1.26.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "cmake_find_package"
    exports_sources = "CMakeLists.txt", "patches/*"
    requires = [
        "sqlite3/3.37.2#8e4989a1ee5d3237a25a911fbcb19097",
        "opus/1.3.1#5132ab8db7b69dd8e26466e0b3b017dd",
    ]

    options = {
        "with_opus": [True, False],
        "with_endpoint_controllers": [True, False],
        "with_metrics": [True, False],
        "with_sensitive_logs": [True, False],
        "with_latency_logs": [True, False],
        "build_testing": [True, False],
        "with_curl_http_version_2_prior_knowledge": [True, False],
        "with_captions": [True, False],
    }
    default_options = {
        "with_opus": True,
        "with_endpoint_controllers": True,
        "with_metrics": True,
        "with_sensitive_logs": False,
        "with_latency_logs": False,
        "with_captions": True,
        "build_testing": False,
        "libcurl:shared": True,
        "libcurl:with_ssl": "openssl",
        "libcurl:with_nghttp2": True,
        "libnghttp2:shared": True,
        "libnghttp2:with_app": False,
        "openssl:shared": True,
        "opus:shared": False,
        "sqlite3:build_executable": False,
        "with_curl_http_version_2_prior_knowledge": False,
    }

    _source_subfolder = "source_subfolder"

    def requirements(self):
        self.requires(f"libcurl/8.0.1@{self.user}/{self.channel}")
        if self.options.with_captions:
            self.requires(f"webvtt/1.0@{self.user}/{self.channel}")

    def source(self):
        tools.get(f"https://github.com/alexa/avs-device-sdk/archive/v{self.version}.tar.gz")
        os.rename(f"avs-device-sdk-{self.version}", self._source_subfolder)

    def configure(self):
        self.settings.compiler.cppstd = "11"
        if self.settings.os == "Android":
            # Android does not support libraries with version extension (libssl.so.1.1)
            # so used static version off openssl for Android
            self.options["openssl"].shared = False
        if self.settings.os == "Macos":
            # Prevent conflict with CoreData framework
            self.options["sqlite3"].shared = False
        else:
            self.options["sqlite3"].shared = True

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.definitions["OPUS"] = "ON" if self.options.with_opus else "OFF"
        cmake.definitions["ENABLE_ALL_ENDPOINT_CONTROLLERS"] = "ON" if self.options.with_endpoint_controllers else "OFF"
        cmake.definitions["METRICS"] = "ON" if self.options.with_metrics else "OFF"
        cmake.definitions["ACSDK_EMIT_SENSITIVE_LOGS"] = self.options.with_sensitive_logs
        cmake.definitions["ACSDK_LATENCY_LOG"] = self.options.with_latency_logs
        cmake.definitions["RAPIDJSON_MEM_OPTIMIZATION"] = "OFF"
        cmake.definitions["BUILD_TESTING"] = "ON" if self.options.build_testing else "OFF"
        cmake.definitions["CAPTIONS"] = "ON" if self.options.with_captions else "OFF"
        if self.options.with_captions:   
            logging.info("Caption is enabled.")
            webvtt_root = self.deps_cpp_info["webvtt"].rootpath
            cmake.definitions["LIBWEBVTT_LIB_PATH"] = os.path.join(webvtt_root, "lib/libwebvtt.a")
            cmake.definitions["LIBWEBVTT_INCLUDE_DIR"] = os.path.join(webvtt_root, "include")
        if self.settings.os == "Android":
            cmake.definitions["ANDROID"] = "OFF"  # this is not a mistake!
        cmake.definitions["ACSDK_ENABLE_CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE"] = (
            "ON" if self.options.with_curl_http_version_2_prior_knowledge else "OFF"
        )
        if self.settings.os == "Neutrino":
            cmake.definitions["FILE_SYSTEM_UTILS"] = "OFF"      # FileSystemUtils is not supported on QNX but enabled by default

        crypto_libs = glob.glob(os.path.join(self.deps_cpp_info["openssl"].lib_paths[0], "libcrypto.*"))
        crypto_lib = sorted(crypto_libs, key=len)[0]
        cmake.definitions["CRYPTO_LIBRARY"] = crypto_lib

        crypto_include_dir = self.deps_cpp_info["openssl"].include_paths[0]
        cmake.definitions["CRYPTO_INCLUDE_DIR"] = crypto_include_dir

        cmake.configure(source_folder=self._source_subfolder)
        return cmake

    def _apply_patches(self):
        for pattern in [
            "patches/*.patch",
            f"patches/{self.settings.os}/*.patch",  # OS-specific patches
            f"patches/{self.settings.arch}/*.patch",  # Arch-specific patches
            f"patches/{self.settings.os}/{self.settings.arch}/*.patch",  # OS-and-arch-specific
        ]:
            for filename in sorted(glob.glob(pattern)):
                logging.info(f"applying patch: {filename}")
                tools.patch(base_path=self._source_subfolder, patch_file=filename)

    def build(self):
        self._apply_patches()
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        if self.settings.build_type == "Release":
            tools.mkdir(self.package_folder)
            # install stripped result
            cmake.build(target="install/strip")
        else:
            cmake.install()

        # copying test headers
        self.copy(
            "*.h",
            src=os.path.join(self._source_subfolder, "AVSCommon/SDKInterfaces/test/AVSCommon/SDKInterfaces"),
            dst="include/AVSCommon/SDKInterfaces/test",
        )
        self.copy(
            "*.h",
            src=os.path.join(self._source_subfolder, "Settings/test/Settings"),
            dst="include/AVSCommon/SDKInterfaces/test/Settings",
        )

    def package_info(self):
        self.cpp_info.names["pkg_config"] = "AlexaClientSDK"
        self.cpp_info.names["cmake_find_package"] = "AlexaClientSDK"
        self.cpp_info.libs = tools.collect_libs(self)
