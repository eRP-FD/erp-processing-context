| Package         | Version      | Lizenz       | Download-Link                                                        |
|-----------------|--------------|--------------|----------------------------------------------------------------------|
| antlr           | 4.13.0       | BSD          | https://www.antlr.org/download.html                                  |
| boost           | 1.82.0       | Boost        | https://github.com/boostorg/boost/tree/boost-1.82.0                  |
| date            | 3.0.1        | MIT          | https://github.com/HowardHinnant/date/tree/v3.0.1                    |
| glog            | 0.6.0        | BSD-3-Clause | https://github.com/google/glog/tree/v0.6.0                           |
| gramine         | 1.4          | LGPL 3.0     | https://github.com/gramineproject/gramine/releases/tag/v1.4          |
| gsl-lite        | 0.40.0       | MIT          | https://github.com/gsl-lite/gsl-lite/tree/v0.40.0                    |
| gtest           | 1.13.0       | BSD-3-Clause | https://github.com/google/googletest/releases/tag/v1.13.0            |
| hiredis         | 1.1.0        | BSD-3-Clause | https://github.com/redis/hiredis/releases/tag/v1.1.0                 |
| libpq           | 14.7         | PostgreSQL   | https://github.com/postgres/postgres/tree/REL_14_7/src/backend/libpq |
| libpqxx         | 7.7.5        | BSD-3-Clause | https://github.com/jtv/libpqxx/tree/7.7.5                            |
| libxml2         | 2.11.4       | MIT          | https://github.com/GNOME/libxml2/tree/v2.11.4                        |
| magic_enum      | 0.9.1        | MIT          | https://github.com/Neargye/magic_enum/releases/tag/v0.9.1            |
| openssl*        | 1.1.1u       | OpenSSL      | https://github.com/openssl/openssl/tree/OpenSSL_1_1_1u               |
| rapidjson       | cci.20220822 | MIT          | https://github.com/Tencent/rapidjson                                 |
| redis-plus-plus | 1.3.7        | Apache-2.0   | https://github.com/sewenew/redis-plus-plus/releases/tag/1.3.7        |
| zlib            | 1.2.13       | Zlib License | https://github.com/madler/zlib/tree/v1.2.13                          |
| zstd            | 1.5.5        | BSD-3-Clause | https://github.com/facebook/zstd/tree/v1.5.5                         |

 \* openssl 1.1.1*: The openssl 1.1.1* is based on the version provided by conan-center (https://conan.io/center/)
   additionally a patch has been applied to access the embedded OCSP response.
   The changed package recipes and the patch are located in the subfolder `conan-recipes/openssl`

 \* openssl 1.1.1*: Die verwendete openssl 1.1.1* basiert auf der auf conan-center (https://conan.io/center/) Verfügbaren.
   zusätzlich wird ein weiterer Patch angewandt, der den Zugriff auf die eingebettete OCSP-Response ermöglicht.
   Das geänderte Conan-Rezept und der Patch befinden sich im Unterordner `conan-recipes/openssl`
