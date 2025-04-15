/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('27');
CALL erp.set_version('28');

ALTER TABLE erp.task
    ALTER COLUMN owner SET STORAGE PLAIN;
ALTER TABLE erp.task
    ALTER COLUMN medication_dispense_salt SET STORAGE PLAIN;
ALTER TABLE erp.task
    ALTER COLUMN pharmacy_identity SET STORAGE PLAIN;
ALTER TABLE erp.task
    ALTER COLUMN doctor_identity SET STORAGE PLAIN;

ALTER TABLE erp.task_169
    ALTER COLUMN owner SET STORAGE PLAIN;
ALTER TABLE erp.task_169
    ALTER COLUMN medication_dispense_salt SET STORAGE PLAIN;
ALTER TABLE erp.task_169
    ALTER COLUMN pharmacy_identity SET STORAGE PLAIN;
ALTER TABLE erp.task_169
    ALTER COLUMN doctor_identity SET STORAGE PLAIN;

ALTER TABLE erp.task_200
    ALTER COLUMN owner SET STORAGE PLAIN;
ALTER TABLE erp.task_200
    ALTER COLUMN medication_dispense_salt SET STORAGE PLAIN;
ALTER TABLE erp.task_200
    ALTER COLUMN pharmacy_identity SET STORAGE PLAIN;
ALTER TABLE erp.task_200
    ALTER COLUMN doctor_identity SET STORAGE PLAIN;

ALTER TABLE erp.task_209
    ALTER COLUMN owner SET STORAGE PLAIN;
ALTER TABLE erp.task_209
    ALTER COLUMN medication_dispense_salt SET STORAGE PLAIN;
ALTER TABLE erp.task_209
    ALTER COLUMN pharmacy_identity SET STORAGE PLAIN;
ALTER TABLE erp.task_209
    ALTER COLUMN doctor_identity SET STORAGE PLAIN;

ALTER TABLE erp_event.task_event
    ALTER COLUMN medication_dispense_salt SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event
    ALTER COLUMN pharmacy_identity SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event
    ALTER COLUMN doctor_identity SET STORAGE PLAIN;

COMMIT;