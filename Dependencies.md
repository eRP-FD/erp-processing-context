| Package         | Version      | Lizenz       | Download-Link                                       |
|-----------------|--------------|--------------|-----------------------------------------------------|
| antlr           | 4.13.2       | BSD          | https://www.antlr.org/download.html                 |
| asn1c           | cci.20200522 | BSD 2-Clause | https://github.com/vlm/asn1c                        |
| boost           | 1.90.0       | Boost        | https://github.com/boostorg/boost/releases          |
| botan           | 3.13.0       | BSD 2-Clause | https://github.com/randombit/botan                  |
| date            | 3.0.5        | MIT          | https://github.com/HowardHinnant/date/releases      |
| fmt             | 12.2.0       | MIT          | https://github.com/fmtlib/fmt/releases              |
| glog            | 0.7.1        | BSD-3-Clause | https://github.com/google/glog/releases             |
| gramine         | 1.6          | LGPL 3.0     | https://github.com/gramineproject/gramine/releases  |
| gsl-lite        | 0.41.0       | MIT          | https://github.com/gsl-lite/gsl-lite/releases       |
| gtest           | 1.17.0       | BSD-3-Clause | https://github.com/google/googletest/releases       |
| hiredis         | 1.3.0        | BSD-3-Clause | https://github.com/redis/hiredis/releases           |
| libpq           | 16.14        | PostgreSQL   | https://github.com/postgres/postgres/tags           |
| libpqxx         | 7.10.5       | BSD-3-Clause | https://github.com/jtv/libpqxx/releases             |
| libunwind       | 1.8.3        | MIT          | https://github.com/libunwind/libunwind/releases     |
| libxml2         | 2.15.3       | MIT          | https://github.com/GNOME/libxml2/tags               |
| magic_enum      | 0.9.8        | MIT          | https://github.com/Neargye/magic_enum/releases      |
| openssl*        | 3.5.8        | OpenSSL      | https://github.com/openssl/openssl/releases         |
| prometheus-cpp  | 1.3.0        | MIT          | https://github.com/jupp0r/prometheus-cpp/releases   |
| rapidjson       | cci.20250205 | MIT          | https://github.com/Tencent/rapidjson                |
| redis-plus-plus | 1.3.15       | Apache-2.0   | https://github.com/sewenew/redis-plus-plus/releases |
| xmlsec          | 1.3.12       | MIT          | https://github.com/lsh123/xmlsec/releases           |
| zlib            | 1.3.2        | Zlib License | https://github.com/madler/zlib/releases             |
| zstd            | 1.5.7        | BSD-3-Clause | https://github.com/facebook/zstd/releases           |

\* openssl 3.5.7: The openssl 3.5.7 is based on the version provided by conan-center (https://conan.io/center/)
additionally a patch has been applied to access the embedded OCSP response. The changed package recipes and the patch
are located in the subfolder `conan-recipes/openssl`

\* openssl 3.5.7: Die verwendete openssl 3.5.7 basiert auf der auf conan-center (https://conan.io/center/) verfügbaren.
zusätzlich wird ein weiterer Patch angewandt, der den Zugriff auf die eingebettete OCSP-Response ermöglicht. Das
geänderte Conan-Rezept und der Patch befinden sich im Unterordner `conan-recipes/openssl`
