| Package         | Version      | Lizenz       | Download-Link                                                         |
|-----------------|--------------|--------------|-----------------------------------------------------------------------|
| antlr           | 4.13.1       | BSD          | https://www.antlr.org/download.html                                   |
| boost           | 1.87.0       | Boost        | https://github.com/boostorg/boost/tree/boost-1.87.0                   |
| botan           | 3.6.1        | BSD 2-Clause | https://github.com/randombit/botan                                    |
| date            | 3.0.3        | MIT          | https://github.com/HowardHinnant/date/tree/v3.0.3                     |
| glog            | 0.7.1        | BSD-3-Clause | https://github.com/google/glog/tree/v0.7.1                            |
| gramine         | 1.6          | LGPL 3.0     | https://github.com/gramineproject/gramine/releases/tag/v1.6           |
| gsl-lite        | 0.41.0       | MIT          | https://github.com/gsl-lite/gsl-lite/tree/v0.41.0                     |
| gtest           | 1.16.0       | BSD-3-Clause | https://github.com/google/googletest/releases/tag/v1.16.0             |
| hiredis         | 1.2.0        | BSD-3-Clause | https://github.com/redis/hiredis/releases/tag/v1.2.0                  |
| libpq           | 15.12        | PostgreSQL   | https://github.com/postgres/postgres/tree/REL_15_12/src/backend/libpq |
| libpqxx         | 7.10.0       | BSD-3-Clause | https://github.com/jtv/libpqxx/tree/7.10.0                            |
| libunwind       | 1.8.1        | MIT          | https://github.com/libunwind/libunwind/releases/tag/v1.8.1            |
| libxml2         | 2.13.6       | MIT          | https://github.com/GNOME/libxml2/tree/v2.13.6                         |
| magic_enum      | 0.9.7        | MIT          | https://github.com/Neargye/magic_enum/releases/tag/v0.9.7             |
| openssl*        | 3.1.8        | OpenSSL      | https://github.com/openssl/openssl/tree/openssl-3.1.8                 |
| rapidjson       | cci.20230929 | MIT          | https://github.com/Tencent/rapidjson                                  |
| redis-plus-plus | 1.3.13       | Apache-2.0   | https://github.com/sewenew/redis-plus-plus/releases/tag/1.3.13        |
| zlib            | 1.3.1        | Zlib License | https://github.com/madler/zlib/tree/v1.3.1                            |
| zstd            | 1.5.7        | BSD-3-Clause | https://github.com/facebook/zstd/tree/v1.5.7                          |

\* openssl 3.1.8: The openssl 3.1.8 is based on the version provided by conan-center (https://conan.io/center/)
additionally a patch has been applied to access the embedded OCSP response.
The changed package recipes and the patch are located in the subfolder `conan-recipes/openssl`

\* openssl 3.1.8: Die verwendete openssl 3.1.8 basiert auf der auf conan-center (https://conan.io/center/) verfügbaren.
zusätzlich wird ein weiterer Patch angewandt, der den Zugriff auf die eingebettete OCSP-Response ermöglicht.
Das geänderte Conan-Rezept und der Patch befinden sich im Unterordner `conan-recipes/openssl`
