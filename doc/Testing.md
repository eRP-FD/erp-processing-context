#Testing

Tests come (at the moment) in three different flavors. Unit tests, functional (?) tests and workflow tests.
- Unit tests focus on the implementation of a single class.
- Functional tests typically test a single endpoint, including VAU encryption and, depending on configuration, either with 
a mock database or a PostgreSQL database.
- Workflow tests simulate typical work flows that involve multiple endpoint calls.

#Configuration

Functional tests use the MockDatabase by default but can easily be configured to use a locally 
running Postgres DB. For this work 
- Either set environment variable `TEST_USE_POSTGRES` to "1", or "on", or "true" (all case insensitive) or alternatively
  modify `resources/test/02_development.config.json` so that `/test/use-postgres` is set to `"true"` (with double quotes).
  Recompilation is not necessary.
- If your locally running db does not support SSL connections then modify `resources/test/02_development.config.json` so
that `erp/postgres/useSsl` is set `"false"` and that `erp/postgres/certificatePath` is empty. Alternatively you can set 
  environment variables `ERP_POSTGRES_USESSL` and `ERP_POSTGRES_CERTIFICATE_PATH` respectively.
  
This configuration can be overruled in test classes by calling `ServerTestBase(bool forceMockDatabase = false)`
with a `true` argument.

## Manual derivation key update tests
This test verifies both the workflow and the handling of derivation key updates.

It should be run against a cloud installation, i. e. `ERP_SERVER_HOST`, `ERP_SERVER_PORT`, `ERP_IDP_REGISTERED_FD_URI`
and `TEST_QES_PEM_FILE_NAME` must be set. The test itself can be run with `erp-integration-test
--gtest_also_run_disabled_tests --gtest_filter="DISABLED_KeyUpdateManual/DerivationKeyUpdateTest.TaskWorkflows/Manual`
and will prompt for any manual actions that need be done. You will also need to be able to enrol and delete derivation keys.
