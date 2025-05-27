# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH

import json
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.sbom.cyclonedx import cyclonedx_1_4
from conan.tools.scm import Git

class ErpProcessingContext(ConanFile):
    name = "erp-processing-context"
    package_type = "application"
    license = "Nutzungsbedingungen für offenzulegende Software im Zusammenhang mit dem Fachdienst „E-Rezept“"
    url = "https://github.com/eRP-FD/erp-processing-context"
    description = "TEE processing context für den Dienst eRezept (erp). TEE = trusted execution environment oder deutsch VAU = vertrauenswürdige Ausführungsumgebung."
    settings = ['os', 'compiler', 'build_type', 'arch']
    options = {
        'release_version' : ["ANY"],
        'with_ccache': [True, False],
        'with_hsm_tpm_production': [True, False],
        'with_hsm_mock': [True, False],
        'with_sbom': [True, False],
        'with_warning_as_error': [True, False],
    }
    default_options = {
        'boost/*:bzip2': False,
        'boost/*:header_only': True,
        'date/*:use_system_tz_db': True,
        'glog/*:with_gflags': False,
        'gtest/*:build_gmock': True,
        'gsl-lite/*:on_contract_violation': 'throw',
        'hiredis/*:fPIC': True,
        'hiredis/*:shared': False,
        'hiredis/*:with_ssl': True,
        'libpq/*:with_openssl': True,
        'libunwind/*:minidebuginfo': False,
        'libxml2/*:iconv': False,  # To prevent zlib conflicts between libraries
        'libxml2/*:zlib': False,
        'openssl/*:shared': True,
        'redis-plus-plus/*:fPIC': True,
        'redis-plus-plus/*:shared': False,
        'redis-plus-plus/*:with_tls': True,
        'tss/*:with_hardware_tpm': True,
        'zlib/*:shared': True,
        'release_version': "1.18.0-DEVELOP",
        'with_ccache': False,
        'with_hsm_tpm_production': True,
        'with_hsm_mock': False,
        'with_sbom': False,
        'with_warning_as_error': False,
    }
    # generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "."
    requires = [
        'antlr4-cppruntime/4.13.1',
        'boost/1.87.0',
        'botan/3.6.1',
        'date/3.0.3',  # date can be removed as soon as we use C++20
        'glog/0.7.1',
        'gsl-lite/0.41.0',
        'gtest/1.16.0',
        'hiredis/1.2.0',
        'libpqxx/7.10.1',
        'libxml2/2.13.6',
        'magic_enum/0.9.7',
        'openssl/3.1.8+erp',
        'rapidjson/cci.20230929',
        'redis-plus-plus/1.3.13',
        'zlib/1.3.1',
        'zstd/1.5.7'  # database compression
    ]

    def set_version(self):
        if not self.version:
            git = Git(self, folder=self.recipe_folder)
            self.version = git.run('describe --tags --abbrev=5 --match "v-[0-9\.]*"')[2:].lower()

    def requirements(self):
        if self.options.with_hsm_tpm_production:
            self.requires('tpmclient/0.15.0-b40')
            self.requires('hsmclient/2.13.0-b89')
        self.requires('libunwind/1.8.1', override=True) # Conflict originates from glog/0.7.1
        self.requires('libpq/16.8', override=True) # Conflict originates from libpqxx/7.10.1

    def build_requirements(self):
        self.tool_requires('xmlsec/1.3.6', options={"shared": False})


    def layout(self):
        cmake_layout(self)

    def config_options(self):
        if self.settings.os == 'Windows':
            self.options['redis-plus-plus:fPIC'] = False
            self.options['hiredis:fPIC'] = False
            self.options['with_hsm_tpm_production'] = False
            self.options['with_hsm_mock'] = True

    def _generate_sbom(self):
        sbom_cyclonedx_1_4 = cyclonedx_1_4(self)  # .subgraph) -> See ERP-26151 for further details:
        generators_folder = self.generators_folder
        file_name = "sbom.cdx.json"
        os.makedirs(os.path.join(generators_folder, "sbom"), exist_ok=True)

        with open(os.path.join(generators_folder, "sbom", file_name), 'w') as f:
            json.dump(sbom_cyclonedx_1_4, f, indent=4)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["ERP_BUILD_VERSION"] = self.version or "LOCAL"
        tc.cache_variables["ERP_RELEASE_VERSION"] = self.options.release_version
        tc.cache_variables["ERP_WITH_HSM_MOCK"] = self.options.with_hsm_mock
        tc.cache_variables["ERP_WITH_HSM_TPM_PRODUCTION"] = self.options.with_hsm_tpm_production
        tc.cache_variables["ERP_WARNING_AS_ERROR"] = self.options.with_warning_as_error
        if self.options.with_ccache:
            tc.cache_variables["CMAKE_CXX_COMPILER_LAUNCHER"] = "ccache"

        tc.generate()

        if self.options.with_sbom:
            self._generate_sbom()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
