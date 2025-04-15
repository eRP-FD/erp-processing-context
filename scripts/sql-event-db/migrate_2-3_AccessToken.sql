/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('2');
CALL erp_event.set_version('3');

ALTER TABLE  erp_event.task_event
    ADD COLUMN IF NOT EXISTS doctor_identity bytea;
ALTER TABLE  erp_event.task_event
    ADD COLUMN IF NOT EXISTS pharmacy_identity bytea;

COMMIT;
