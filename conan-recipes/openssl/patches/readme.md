Porting patches to new OpenSSL 3.1.* Versions
====
The patch files can be ported to newer openssl-3.1.* version for example this way:
- clone openssl github: https://github.com/openssl/openssl
- checkout 3.1.* branch: `openssl-3.1`
- apply patch files in correct order, e.g.  `git am <DIR>/erp-processing-context/conan-recipes/openssl/patches/000*.patch` or `git apply` + `git commit`
- create new patch files: `git format-patch origin/openssl-3.1`

Updating conanfile.py and conandata.yml from upstream
====
- The original files come from https://github.com/conan-io/conan-center-index/tree/master/recipes/openssl/3.x.x
