#Building
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
- get an account for the local nexus `https://nexus.epa-dev.net/repository/erp-conan-internal`. Ask Bogdan.
- add the remote conan servers with
    ```
    conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy
    conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal
    ```
- set up an SSH key for the local github repository
  - The process is explained here: https://docs.github.com/en/github/authenticating-to-github/adding-a-new-ssh-key-to-your-github-account
  - The keys have to be added here: https://github.ibmgcloud.net/settings/keys

You may also have to configure some values in you conan profile:
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
