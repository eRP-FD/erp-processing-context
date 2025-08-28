/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('35');
CALL erp.set_version('36');

GRANT DELETE ON erp.eu_access_permission to role_proc_user;

GRANT UPDATE ON TABLE erp.task_view TO role_proc_user;

COMMIT;
