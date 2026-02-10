/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('39');
CALL erp.set_version('40');

ALTER TABLE erp.charge_item DROP CONSTRAINT IF EXISTS prescription_type_pkv;

CREATE TABLE IF NOT EXISTS erp.task_166 (
    LIKE erp.task INCLUDING ALL
)
PARTITION BY RANGE (prescription_id);

ALTER TABLE erp.task_166 DROP CONSTRAINT IF EXISTS task_166_pkey;
ALTER TABLE erp.task_166 ADD CONSTRAINT task_166_pkey PRIMARY KEY (prescription_id);

ALTER TABLE erp.task_166 ALTER COLUMN kvnr SET STORAGE PLAIN;
ALTER TABLE erp.task_166 ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp.task_166 ALTER COLUMN salt SET STORAGE PLAIN;
ALTER TABLE erp.task_166 ALTER COLUMN access_code SET STORAGE PLAIN;
ALTER TABLE erp.task_166 ALTER COLUMN secret SET STORAGE PLAIN;
ALTER TABLE erp.task_166 ALTER COLUMN performer SET STORAGE PLAIN;
ALTER TABLE erp.task_166 ALTER COLUMN is_pkv SET DEFAULT NULL;

CREATE TABLE IF NOT EXISTS erp.task_166000005000000 PARTITION OF erp.task_166 FOR VALUES FROM ('0') TO ('5000000');
ALTER TABLE erp.task_166000005000000 OWNER TO role_proc_maintenance;
GRANT INSERT, SELECT, UPDATE ON erp.task_166000005000000 TO role_proc_user;

CREATE TABLE IF NOT EXISTS erp.task_166_default PARTITION OF erp.task_166 default;
ALTER TABLE erp.task_166_default OWNER TO role_proc_maintenance;
GRANT INSERT, SELECT, UPDATE ON erp.task_166_default TO role_proc_user;

-- create own counter for this table
CREATE SEQUENCE IF NOT EXISTS erp.task_166_taskid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;

ALTER SEQUENCE erp.task_166_taskid_seq OWNED BY erp.task_166.prescription_id;
ALTER TABLE erp.task_166 ALTER COLUMN prescription_id SET DEFAULT nextval('erp.task_166_taskid_seq'::regclass);



-- create foreign key constraints for table
ALTER TABLE erp.task_166 DROP CONSTRAINT IF EXISTS fk_task_166_key_blob_id;
ALTER TABLE erp.task_166 DROP CONSTRAINT IF EXISTS fk_medication_dispense_166_blob_id;
ALTER TABLE erp.task_166 ADD CONSTRAINT fk_task_166_key_blob_id FOREIGN KEY (task_key_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;
ALTER TABLE erp.task_166 ADD CONSTRAINT fk_medication_dispense_166_blob_id FOREIGN KEY (medication_dispense_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

-- create indicies
CREATE INDEX IF NOT EXISTS task_166_kvnr_idx ON erp.task_166 USING hash (kvnr_hashed);
CREATE INDEX IF NOT EXISTS task_166_last_modified_idx ON erp.task_166 USING btree (last_modified);


-- Updating tasks is required as the task progresses
revoke all on  erp.task_166 from role_proc_user;
alter table erp.task_166 owner to role_proc_maintenance;
grant insert, select, update on erp.task_166 to role_proc_user;
alter sequence erp.task_166_taskid_seq owner to role_proc_maintenance;
grant ALL ON SEQUENCE erp.task_166_taskid_seq TO role_proc_user;

-- add 166 to the view
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
       eu_redeemable_patient,
       is_pkv
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
       eu_redeemable_patient,
       is_pkv
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
       166::smallint AS prescription_type,
       owner,
       last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity,
       last_status_update,
       eu_redeemable,
       eu_redeemable_patient,
       is_pkv
FROM erp.task_166
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
       eu_redeemable_patient,
       is_pkv
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
       eu_redeemable_patient,
       is_pkv
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
       eu_redeemable_patient,
       is_pkv
FROM erp.task_209
    );


COMMIT;
