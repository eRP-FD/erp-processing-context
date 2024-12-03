/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('28');
CALL erp.set_version('29');

ALTER TABLE erp.task
    ADD COLUMN IF NOT EXISTS last_status_update timestamp with time zone;

ALTER TABLE erp.task_169
    ADD COLUMN IF NOT EXISTS last_status_update timestamp with time zone;

ALTER TABLE erp.task_200
    ADD COLUMN IF NOT EXISTS last_status_update timestamp with time zone;

ALTER TABLE erp.task_209
    ADD COLUMN IF NOT EXISTS last_status_update timestamp with time zone;

CREATE OR REPLACE VIEW erp.task_view AS
(
SELECT prescription_id,
       kvnr,
       kvnr_hashed,
       last_modified,
       authored_on,
       expiry_date,
       accept_date,
       status,
       task_key_blob_id,
       salt,
       access_code,
       secret,
       healthcare_provider_prescription,
       receipt,
       when_handed_over,
       when_prepared,
       performer,
       medication_dispense_blob_id,
       medication_dispense_bundle,
       160::smallint AS prescription_type,
       owner,
       last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity,
       last_status_update
FROM erp.task
UNION ALL
SELECT prescription_id,
       kvnr,
       kvnr_hashed,
       last_modified,
       authored_on,
       expiry_date,
       accept_date,
       status,
       task_key_blob_id,
       salt,
       access_code,
       secret,
       healthcare_provider_prescription,
       receipt,
       when_handed_over,
       when_prepared,
       performer,
       medication_dispense_blob_id,
       medication_dispense_bundle,
       169::smallint AS prescription_type,
       owner,
       last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity,
       last_status_update
FROM erp.task_169
UNION ALL
SELECT prescription_id,
       kvnr,
       kvnr_hashed,
       last_modified,
       authored_on,
       expiry_date,
       accept_date,
       status,
       task_key_blob_id,
       salt,
       access_code,
       secret,
       healthcare_provider_prescription,
       receipt,
       when_handed_over,
       when_prepared,
       performer,
       medication_dispense_blob_id,
       medication_dispense_bundle,
       200::smallint AS prescription_type,
       owner,
       last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity,
       last_status_update
FROM erp.task_200
UNION ALL
SELECT prescription_id,
       kvnr,
       kvnr_hashed,
       last_modified,
       authored_on,
       expiry_date,
       accept_date,
       status,
       task_key_blob_id,
       salt,
       access_code,
       secret,
       healthcare_provider_prescription,
       receipt,
       when_handed_over,
       when_prepared,
       performer,
       medication_dispense_blob_id,
       medication_dispense_bundle,
       209::smallint AS prescription_type,
       owner,
       last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity,
       last_status_update
FROM erp.task_209
    );

COMMIT;
