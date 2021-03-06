| Package         | Version      | Lizenz       | Download-Link                                                        |
|-----------------|--------------|--------------|----------------------------------------------------------------------|
| boost           | 1.79.0       | Boost        | https://github.com/boostorg/boost/tree/boost-1.79.0                  |
| date            | 3.0.1        | MIT          | https://github.com/HowardHinnant/date/tree/v3.0.1                    |
| glog            | 0.6.0        | BSD-3-Clause | https://github.com/google/glog/tree/v0.6.0                           |
| gsl-lite        | 0.40.0       | MIT          | https://github.com/gsl-lite/gsl-lite/tree/v0.40.0                    |
| libxml2         | 2.9.14       | MIT          | https://github.com/GNOME/libxml2/tree/v2.9.14                        |
| openssl*        | 1.1.1q       | OpenSSL      | https://github.com/openssl/openssl/tree/OpenSSL_1_1_1q               |
| rapidjson       | cci.20211112 | MIT          | https://github.com/Tencent/rapidjson                                 |
| magic_enum      | 0.7.3        | MIT          | https://github.com/build2-packaging/magic_enum/releases/tag/v0.7.3   |
| libpqxx         | 7.7.3        | BSD-3-Clause | https://github.com/jtv/libpqxx/tree/7.7.3                            |
| zstd            | 1.5.2        | BSD-3-Clause | https://github.com/facebook/zstd/tree/v1.5.2                         |
| libpq           | 13.6         | PostgreSQL   | https://github.com/postgres/postgres/tree/REL_13_6/src/backend/libpq |
| gtest           | 1.11.0       | BSD-3-Clause | https://github.com/google/googletest/releases/tag/release-1.11.0     |
| hiredis         | 1.0.2        | BSD-3-Clause | https://github.com/redis/hiredis/tree/v1.0.2                         |
| redis-plus-plus | 1.3.3        | Apache-2.0   | https://github.com/sewenew/redis-plus-plus/tree/1.3.3                |
| gramine         | 1.0          | LGPL 3.0     | https://github.com/gramineproject/gramine/releases/tag/v1.0          |

 \* openssl 1.1.1q: The openssl 1.1.1q is based on the version provided by conan-center (https://conan.io/center/)
   additionally a patch has been applied to access the embedded OCSP response.
   The changed package recipes and the patch are located in the subfolder `conan-recipes/openssl`

 \* openssl 1.1.1q: Die verwendete openssl 1.1.1q basiert auf der auf conan-center (https://conan.io/center/) Verf??gbaren.
   zus??tzlich wird ein weiterer Patch angewandt, der den Zugriff auf die eingebettete OCSP-Response erm??glicht.
   Das ge??nderte Conan-Rezept und der Patch befinden sich im Unterordner `conan-recipes/openssl`
