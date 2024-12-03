/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('3');
CALL erp_event.set_version('4');

ALTER TABLE erp_event.task_event
    ALTER COLUMN medication_dispense_salt SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event
    ALTER COLUMN pharmacy_identity SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event
    ALTER COLUMN doctor_identity SET STORAGE PLAIN;

COMMIT;