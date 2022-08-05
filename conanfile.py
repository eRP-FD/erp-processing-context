#!/usr/bin/env python3

#  (C) Copyright IBM Deutschland GmbH 2021
#  (C) Copyright IBM Corp. 2021

from conans import CMake
from conans import ConanFile
from conans import tools
import os


class ErpProcessingContext(ConanFile):
    name = "erp-processing-context"
    license = "Nutzungsbedingungen für offenzulegende Software im Zusammenhang mit dem Fachdienst „E-Rezept“"
    url = "https://github.com/eRP-FD/erp-processing-context"
    description = "TEE processing context für den Dienst eRezept (erp). TEE = trusted execution environment oder deutsch VAU = vertrauenswürdige Ausführungsumgebung."
    settings = {'os': ['Linux', 'Windows'],
                'compiler': ['gcc', 'Visual Studio', 'clang'],
                'build_type': ['Debug', 'Release', 'RelWithDebInfo'],
                'arch': ['x86_64']}
    options = {'with_tpmclient': [True, False],
               'with_hsmclient': [True, False],
               'with_sbom': [True, False]}
    default_options = {'boost:bzip2': False,
                       'boost:header_only': True,
                       'gsl-lite:on_contract_violation': 'throw',
                       'glog:with_gflags': False,
                       'libpq:with_openssl': True,
                       'libxml2:iconv': False,  # To prevent zlib conflicts between libraries
                       'libxml2:zlib': False,
                       'tss:with_hardware_tpm': True,
                       'redis-plus-plus:fPIC': True,
                       'redis-plus-plus:shared': False,
                       'redis-plus-plus:with_tls': True,
                       'hiredis:fPIC': True,
                       'hiredis:shared': False,
                       'hiredis:with_ssl': True,
                       'with_tpmclient': True,
                       'with_hsmclient': True,
                       'with_sbom': False,
                       'zlib:shared': True,
                       'date:use_system_tz_db': True,
                       'gmp:shared': True,
                       'mpfr:shared': True}
    generators = "cmake"
    exports_sources = "."
    build_requires = []
    requires = ['antlr4-cppruntime/4.10.1',
                'boost/1.79.0',
                'date/3.0.1',  # date can be removed as soon as we use C++20
                'glog/0.6.0',
                'gsl-lite/0.40.0',
                'libxml2/2.9.14',
                'openssl/1.1.1q@erp/stable-1',
                'rapidjson/cci.20211112',
                'magic_enum/0.7.3',
                'zlib/1.2.12',  # when updating this, also update /docker/manifest.*.template files
                'libpqxx/7.7.3',
                'libpq/13.6',
                'zstd/1.5.2',  # database compression
                'gtest/1.11.0',
                'hiredis/1.0.2',
                'redis-plus-plus/1.3.3',
                'gmp/6.2.1',
                'mpfr/4.1.0']

    def requirements(self):
        if self.options.with_tpmclient:
            self.requires('tpmclient/0.14.0')
        if self.options.with_hsmclient:
            self.requires('hsmclient/2.3.0-B63')

    def build_requirements(self):
        if self.options.with_sbom:
            self.build_requires('sbom_generator/0.1@ibm/stable')

    def configure(self):
        if self.options.with_sbom:
            self.generators = "cmake", "sbom"

    def config_options(self):
        if self.settings.os == 'Windows':
            self.options['redis-plus-plus:fPIC'] = False
            self.options['hiredis:fPIC'] = False
            self.options['with_tpmclient'] = False
            self.options['with_hsmclient'] = False
