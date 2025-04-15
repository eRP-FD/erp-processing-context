/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('31');
CALL erp.set_version('32');

--
-- Add additional grants for the role_proc_maintenance
--
GRANT ALL ON SCHEMA erp_event TO role_proc_maintenance;
GRANT INSERT, SELECT, DELETE ON erp_event.task_event TO role_proc_maintenance;
GRANT USAGE, SELECT, UPDATE ON ALL SEQUENCES IN SCHEMA erp_event TO role_proc_maintenance;


COMMIT;