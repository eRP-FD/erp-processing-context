/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('34');
CALL erp.set_version('35');

DO $$
DECLARE
  tbls text[] := ARRAY[
    'task',
    'task_162',
    'task_169',
    'task_200',
    'task_209'
  ];
  tbl text;
BEGIN
  FOREACH tbl IN ARRAY tbls LOOP
    EXECUTE format(
      'ALTER TABLE erp.%I ADD COLUMN IF NOT EXISTS eu_redeemable BOOLEAN DEFAULT FALSE;',
      tbl
    );
    EXECUTE format(
      'ALTER TABLE erp.%I ADD COLUMN IF NOT EXISTS eu_redeemable_patient BOOLEAN DEFAULT FALSE;',
      tbl
    );
  END LOOP;
END
$$;

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
       last_status_update,
       eu_redeemable,
       eu_redeemable_patient
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
       162::smallint AS prescription_type,
       owner,
       last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity,
       last_status_update,
       eu_redeemable,
       eu_redeemable_patient
FROM erp.task_162
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
       last_status_update,
       eu_redeemable,
       eu_redeemable_patient
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
       last_status_update,
       eu_redeemable,
       eu_redeemable_patient
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
       last_status_update,
       eu_redeemable,
       eu_redeemable_patient
FROM erp.task_209
    );

COMMIT;
