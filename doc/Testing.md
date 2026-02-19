Testing
=======
There are the following Test-Executables located in `bin` of your build folder:

| Executable | Description |
| ---------- | ------------|
| erp-test   | tests functionality of `erp-processing-context` |
| exporter-test | tests functionality of `erp-medication-exporter` |
| fhirtools-test | tests functions of `fhirtools` library located in `src/fhirtrools` |
| erp-integration-test | This executable is mainly used by Jenkins integrationtest (see: [Jenkins > eRp > Integration](https://jenkins.epa-dev.net/job/eRp/job/Integration/))

Running `erp-test`
------------------
Most tests from `erp-test` already run as standalone tests. However the Database and HSM tests are disabled.
To enhance testing you will have to set-up the _docker-compose environment_ as described in [docker-compose/README.md](../docker-compose/README.md).

Running `exporter-test`
-----------------------
As most of the core components have not been mocked, the exporter test is highly dependent on setting up the _docker-compose environment_. Make sure to set it up a described in [docker-compose/README.md](../docker-compose/README.md).

Running `fhirtools-test`
------------------------
None of the test need extra support from external components. All tests should pass when running the executable.

Running `erp-integration-test`
-------------------------------
This test executable is composed of a subset of tests from `erp-test`. It is focuse on running tests on a  full orchestration of the eRP, which also includes the tls-proxy component.
The integration test is invoked from the Jenkins pipeline [Jenkins > eRp > Integration](https://jenkins.epa-dev.net/job/eRp/job/Integration/). 

## Manual derivation key update tests
This test verifies both the workflow and the handling of derivation key updates.

It should be run against a cloud installation, i.e. `ERP_SERVER_HOST`, `ERP_SERVER_PORT`, `ERP_IDP_REGISTERED_FD_URI`
and `TEST_QES_PEM_FILE_NAME` must be set. The test itself can be run with `erp-integration-test
--gtest_also_run_disabled_tests --gtest_filter="DISABLED_KeyUpdateManual/DerivationKeyUpdateTest.TaskWorkflows/Manual`
and will prompt for any manual actions that need be done. You will also need to be able to enrol and delete derivation keys.


## Run Test with HSM Simulator and TPM

To run the unit test for HSM simulator and the TPM server, you need to configure the build with
`-DERP_WITH_HSM_TPM_PRODUCTION=ON` and then you can start the HSM simulator manually or via the provided
docker-compose stack found in the `docker-compose/` directory in the project root.

Adjust the test-configuration such that the HSM simulator, e.g. by adding a separate configuration file
`03_hsm.config.json` to the same directory as the test binary with the following content:

```json
{
  "erp": {
    "hsm": {
      "device": "3001@localhost",
      "username": "ERP_WORK",
      "password": "password",
      "max-session-count" : "5",
      "tee-token" :{
          "update-seconds" : "1200",
          "retry-seconds" : "30"
      }
    }
  }
}
```

The TPM can only be configured by environment variables. If the TPM server is started from
the provided docker compose stack, the required variables are:

```
TPM_INTERFACE_TYPE="socsim"
TPM_COMMAND_PORT="9002"
TPM_SERVER_TYPE="mssim"
TPM_SERVER_NAME="localhost"
TPM_PLATFORM_PORT="9003"
```

With the given configuration you can now execute HSM simulator tests.