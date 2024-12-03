/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('24');
CALL erp.set_version('25');

ALTER TABLE  erp_event.task_event
    ADD COLUMN IF NOT EXISTS medication_dispense_salt bytea;

COMMIT;
