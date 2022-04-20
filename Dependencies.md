| Package         | Version      | Lizenz       | Download-Link                                                        |
|-----------------|--------------|--------------|----------------------------------------------------------------------|
| boost           | 1.77.0       | Boost        | https://github.com/boostorg/boost/tree/boost-1.77.0                  |
| date            | 3.0.1        | MIT          | https://github.com/HowardHinnant/date/tree/v3.0.1                    |
| glog            | 0.4.0        | BSD-3-Clause | https://github.com/google/glog/tree/v0.4.0                           |
| gsl-lite        | 0.39.0       | MIT          | https://github.com/gsl-lite/gsl-lite/tree/v0.39.0                    |
| libxml2         | 2.9.12       | MIT          | https://github.com/GNOME/libxml2/tree/v2.9.10                        |
| openssl*        | 1.1.1n       | OpenSSL      | https://github.com/openssl/openssl/tree/OpenSSL_1_1_1n               |
| rapidjson       | cci.20211112 | MIT          | https://github.com/Tencent/rapidjson                                 |
| magic_enum      | 0.7.3        | MIT          | https://github.com/build2-packaging/magic_enum/releases/tag/v0.7.3   |
| zlib            | 1.2.12       | Zlib License | https://github.com/madler/zlib/releases/tag/v1.2.12                  |
| libpqxx         | 7.6.0        | BSD-3-Clause | https://github.com/jtv/libpqxx/tree/7.6.0                            |
| zstd            | 1.5.0        | BSD-3-Clause | https://github.com/facebook/zstd/tree/v1.5.0                         |
| libpq           | 13.4         | PostgreSQL   | https://github.com/postgres/postgres/tree/REL_13_4/src/backend/libpq |
| gtest           | 1.10.0       | BSD-3-Clause | https://github.com/google/googletest/releases/tag/release-1.10.0     |
| hiredis         | 1.0.2        | BSD-3-Clause | https://github.com/redis/hiredis/tree/v1.0.2                         |
| redis-plus-plus | 1.3.2        | Apache-2.0   | https://github.com/sewenew/redis-plus-plus/tree/1.3.2                |
| gramine         | 1.0          | LGPL 3.0     | https://github.com/gramineproject/gramine/releases/tag/v1.0          |

 \* openssl 1.1.1n: The openssl 1.1.1n is based on the version provided by conan-center (https://conan.io/center/)
   additionally a patch has been applied to access the embedded OCSP response.
   The changed package recipes and the patch are located in the subfolder `conan-recipes/openssl`

 \* openssl 1.1.1n: Die verwendete openssl 1.1.1n basiert auf der auf conan-center (https://conan.io/center/) Verfügbaren.
   zusätzlich wird ein weiterer Patch angewandt, der den Zugriff auf die eingebettete OCSP-Response ermöglicht.
   Das geänderte Conan-Rezept und der Patch befinden sich im Unterordner `conan-recipes/openssl`
