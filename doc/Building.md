#Building (public repository)
This section describes how to build the `erp-processing-context` from the public github repository.
The build is performed using mock implementations of HSM and TPM, building against the production HSM and TPM
libraries is also possible, but requires additional dependencies, that are not in the scope of this how-to.

You may need to create a conan profile matching your machine and compiler configuration. The following profile is an
example, and is known to work under Ubuntu 20.10, with gcc-9 installed.
```
[settings]
os=Linux
os_build=Linux
arch=x86_64
arch_build=x86_64
compiler=gcc
compiler.version=9
compiler.libcxx=libstdc++11
compiler.cppstd=17
build_type=Debug
[options]
[build_requires]
[env]
CXX=g++-9
CC=gcc-9
```

Make sure to use the patched openssl, provided in this repository.
Export the recipe, that will apply the patch, from within the folder `conan-recipes/openssl`:
```
conan export . openssl/1.1.1l@erp/stable-1
```

Afterwards enter the erp-processing-context folder, and roughly follow these steps:
```
mkdir build
cd build
conan install .. --build missing -o with_tpmclient=False -o with_hsmclient=False [-pr <profile>]
cmake -DERP_WITH_HSM_MOCK=ON -DERP_WITH_HSM_TPM_PRODUCTION=OFF ..
make
```

The following output should be generated:
- `bin/erp-processing-context`: The ERP server process.
- `bin/erp-test`: Unit Tests with compiled-in erp-processing-context
- `bin/erp-integration-test`: Tests, that can be executed against a running erp-processing-context

running the erp-test test should work out of the box, starting the erp-processing-context needs a postgres database
up and running. The database tables can be initialized using the script `scripts/reset_database.sh`

You might encounter the following error, while starting the `erp-processing-context`, which comes from the fact, that
the Telematik-Infrastructure is needed at this point:
```
E1122 14:59:31.457191 10514 TslManager.cxx:36] ../src/erp/tsl/TslManager.cxx:88, can not initialize TslManager, TslError:
E1122 14:59:31.457212 10514 TslManager.cxx:37] {"id":268435458,"compType":"eRP:PKI","errorData":[{"errorCode":"TSL_INIT_ERROR","message":"TSL file is outdated and no update is possible."},{"errorCode":"TSL_DOWNLOAD_ERROR","message":"Can not download hash value of TrustServiceStatusList."}]}
E1122 14:59:31.457254 10514 ErpMain.cxx:410] Can not create TslManager, TslError: {"id":268435458,"compType":"eRP:PKI","errorData":[{"errorCode":"TSL_INIT_ERROR","message":"TSL file is outdated and no update is possible."},{"errorCode":"TSL_DOWNLOAD_ERROR","message":"Can not download hash value of TrustServiceStatusList."}]}
E1122 14:59:31.495510 10514 erp-main.cxx:110] Unexpected exception: Dynamic exception type: ExceptionWrapper<TslError>
std::exception::what: {"id":268435458,"compType":"eRP:PKI","errorData":[{"errorCode":"TSL_INIT_ERROR","message":"TSL file is outdated and no update is possible."},{"errorCode":"TSL_DOWNLOAD_ERROR","message":"Can not download hash value of TrustServiceStatusList."}]}
W1122 14:59:31.497331 10514 erp-main.cxx:113] exiting erp-processing-context with exit code 1
```
There sadly currently is no good workaround for this issue, other than commenting the `throw` in `ErpMain.cxx:411`

# Building (IBM internal)
We use conan as package manager, so the build should be as simple as
```
mkdir build
cd build
conan install .. --build missing
cmake ..
make
```
when you start in the top level directory of the project.

As some of the conan packages reside on a local Nexus and pull in local git repositories you first have to
- get an account for the local nexus `https://nexus.epa-dev.net/repository/erp-conan-internal`. Ask Devops.
- add the remote conan servers with
    ```
    conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy
    conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal
    ```
- set up an SSH key for the local github repository
  - The process is explained here: https://docs.github.com/en/github/authenticating-to-github/adding-a-new-ssh-key-to-your-github-account
  - The keys have to be added here: https://github.ibmgcloud.net/settings/keys

You may also have to configure some values in you conan profile (under ~/.conan/profiles/):
```
compiler.libcxx=libstdc++11
compiler.cppstd=17
```
## Outdated TSL
In case you are running into a problem with an outdated TSL you can download a new one with following command
```
curl https://download-ref.tsl.ti-dienste.de/ECC/ECC-RSA_TSL-ref.xml -o ECC-RSA_TSL-ref.xml
```
and simply replace the `./resources/test/tsl/TSL_valid.xml`.

## BNetzA-VL update
BNetzA-VL must be in sync with TSL. In some rare cases the BNetzA-VL is updated along with TSL,
in this case the new version of BNetzA-VL should also be downloaded with the following command
```
curl https://download-testref.tsl.ti-dienste.de/P-BNetzA/Pseudo-BNetzA-VL.xml -o Pseudo-BNetzA-VL.xml
```
and replace the file `./resources/test/tsl/BNA_valid.xml`.
