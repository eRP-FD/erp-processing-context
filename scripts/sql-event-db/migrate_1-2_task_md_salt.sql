/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

begin;
CALL erp_event.expect_version('1');
CALL erp_event.set_version('2');

ALTER TABLE erp_event.task_event
    ADD COLUMN IF NOT EXISTS medication_dispense_salt bytea;

COMMIT;