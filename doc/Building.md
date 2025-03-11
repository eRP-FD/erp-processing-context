# Building (public repository)

This section describes how to build the `erp-processing-context` from the public github repository.
The build is performed using mock implementations of HSM and TPM, building against the production HSM and TPM
libraries is also possible, but requires additional dependencies, that are not in the scope of this how-to.

Make sure to use the patched openssl, provided in this repository.
Export the recipe, that will apply the patch, from within the folder `conan-recipes/openssl`:
```
conan export --user erp --channel stable-1 --version 3.1.5 .
```

Afterwards enter the erp-processing-context folder, and roughly follow these steps:
```sh
conan build .\
  -s '&:build_type=Debug' \
  -o '&:with_hsm_tpm_production=False' \
  -o '&:with_hsm_mock=True' \
  --build missing \
  -c tools.cmake.cmaketoolchain:generator=Ninja
```

The in the subfolder `build/Debug` following executables should be generated:
- `bin/erp-processing-context`: The eRP server process.
- `bin/erp-exporter`: The eRP to ePA prescription exporter
- `bin/erp-test`: Unit Tests with compiled-in erp-processing-context
- `bin/exporter-test`: Unit Tests for `erp-exporter`
- `bin/fhirtools-test`: Unit Tests for FHIR-Validator and Converter
- `bin/erp-integration-test`: Tests, that can be executed against a running erp-processing-context

Running the tests should work out of the box, starting the erp-processing-context needs two postgres database
up and running.
The database tables can be initialized using the scripts:
 * `scripts/reset_database.sh`
 * `reset_event_database.sh`

Adapt configuration according to your database setup and run:
```sh
 DEBUG_ENABLE_MOCK_TSL_MANAGER=true \
 DEBUG_ENABLE_HSM_MOCK=true \
 DEBUG_DISABLE_REGISTRATION=true \
 DEBUG_DISABLE_DOS_CHECK=true \
 ERP_SERVER_HOST=127.0.0.1 \
 ./erp-processing-context
 ```

_NOTE: It is not a development priority to support runing the project outside the target infrastructure, therefore it might not be possible to run all services properly._

# Building (IBM internal)
First set up an SSH key for the local github repository
  - The process is explained here: https://docs.github.com/en/github/authenticating-to-github/adding-a-new-ssh-key-to-your-github-account
  - The keys have to be added here: https://github.ibmgcloud.net/settings/keys

As some of the conan packages reside on a local Nexus, pull in local git repositories as follows:
- get an account for the local nexus `https://artifactory-cpp-ce.ihc-devops.net/`. Ask Devops.
- generate a token for the conan command line tool in your account settings or by selecting the desired repository under _Arrtifactory -> Artefacts_ and using the _Set Me Up_ button on the top right.
- add the remote conan servers with
```sh
conan remote add erp-conan-2 https://artifactory-cpp-ce.ihc-devops.net/artifactory/api/conan/erp-conan-2
conan remote add erp-conan-internal https://artifactory-cpp-ce.ihc-devops.net/artifactory/api/conan/erp-conan-internal
conan remote auth erp-conan-2
conan remote auth erp-conan-internal
```

When you start in the top level directory of the project it should be a simple:
```
conan build .\
  -s '&:build_type=Debug' \
  -o '&:with_hsm_mock=True' \
  --build missing \
  -c tools.cmake.cmaketoolchain:generator=Ninja
```

This will create a subfolder `build/Debug` and build the project there.
