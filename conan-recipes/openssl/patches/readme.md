Porting patches to new OpenSSL 1.1.1* Versions
====
The patch files can be ported to newer openssl-1.1.1* version for example this way:
- clone openssl github: https://github.com/openssl/openssl
- checkout 1.1.1 branch: `OpenSSL_1_1_1-stable`
- apply patch files in correct order, e.g.  `git am <DIR>/erp-processing-context/conan-recipes/openssl/patches/000*.patch` or `git apply` + `git commit`
- create new patch files: `git format-patch origin/OpenSSL_1_1_1-stable`

Updating conanfile.py and conandata.yml from upstream
====
- The files come from https://github.com/conan-io/conan-center-index/tree/master/recipes/openssl/1.x.x
- In the conanfile.py one line must be added: `autotools.make(target="update")` before the `autotools.make()` call.
  - see commit fb5dbae927de8708560da45ee7498d8e42936d34