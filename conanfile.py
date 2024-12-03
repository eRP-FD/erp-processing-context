# (C) Copyright IBM Deutschland GmbH 2021, 2023
# (C) Copyright IBM Corp. 2021, 2023
#
# non-exclusively licensed to gematik GmbH

from conan import ConanFile


class ErpProcessingContext(ConanFile):
    name = "erp-processing-context"
    license = "Nutzungsbedingungen für offenzulegende Software im Zusammenhang mit dem Fachdienst „E-Rezept“"
    url = "https://github.com/eRP-FD/erp-processing-context"
    description = "TEE processing context für den Dienst eRezept (erp). TEE = trusted execution environment oder deutsch VAU = vertrauenswürdige Ausführungsumgebung."
    settings = ['os', 'compiler', 'build_type', 'arch']
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
                       'openssl:shared': True,
                       'date:use_system_tz_db': True,
                       'libunwind:minidebuginfo': False}
    generators = "cmake"
    exports_sources = "."
    build_requires = []
    requires = ['antlr4-cppruntime/4.13.1',
                'boost/1.86.0',
                "botan/3.4.0",
                'date/3.0.1',  # date can be removed as soon as we use C++20
                'glog/0.7.0',
                'gsl-lite/0.41.0',
                'libxml2/2.11.7',
                'openssl/3.1.5@erp/stable-1',
                'rapidjson/cci.20220822',
                'magic_enum/0.9.5',
                'libpqxx/7.9.2',
                'libpq/14.9',
                'libunwind/1.8.0',
                'zstd/1.5.5',  # database compression
                'gtest/1.13.0',
                'hiredis/1.2.0@erp/stable',
                'redis-plus-plus/1.3.12',
                'zlib/1.3.1']

    def requirements(self):
        if self.options.with_tpmclient:
            self.requires('tpmclient/0.15.0-B37')
        if self.options.with_hsmclient:
            self.requires('hsmclient/2.13.0-B00')

    def build_requirements(self):
        if self.options.with_sbom:
            self.build_requires('sbom_generator/0.2@ibm/stable')

    def configure(self):
        if self.options.with_sbom:
            self.generators = "cmake", "sbom"

    def config_options(self):
        if self.settings.os == 'Windows':
            self.options['redis-plus-plus:fPIC'] = False
            self.options['hiredis:fPIC'] = False
            self.options['with_tpmclient'] = False
            self.options['with_hsmclient'] = False
