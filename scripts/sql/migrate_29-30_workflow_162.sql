/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('29');
CALL erp.set_version('30');

\ir create_partitions_func.sql

CREATE TABLE IF NOT EXISTS erp.task_162 (
    LIKE erp.task INCLUDING ALL
)
PARTITION BY RANGE (prescription_id);

ALTER TABLE erp.task_162 DROP CONSTRAINT IF EXISTS task_162_pkey;
ALTER TABLE erp.task_162 ADD CONSTRAINT task_162_pkey PRIMARY KEY (prescription_id);

ALTER TABLE erp.task_162 ALTER COLUMN kvnr SET STORAGE PLAIN;
ALTER TABLE erp.task_162 ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp.task_162 ALTER COLUMN salt SET STORAGE PLAIN;
ALTER TABLE erp.task_162 ALTER COLUMN access_code SET STORAGE PLAIN;
ALTER TABLE erp.task_162 ALTER COLUMN secret SET STORAGE PLAIN;
ALTER TABLE erp.task_162 ALTER COLUMN performer SET STORAGE PLAIN;

CALL pg_temp.create_task_partitions('erp.task_162');

-- create own counter for this table
CREATE SEQUENCE IF NOT EXISTS erp.task_162_taskid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;

ALTER SEQUENCE erp.task_162_taskid_seq OWNED BY erp.task_162.prescription_id;
ALTER TABLE erp.task_162 ALTER COLUMN prescription_id SET DEFAULT nextval('erp.task_162_taskid_seq'::regclass);



-- create foreign key constraints for table
ALTER TABLE erp.task_162 DROP CONSTRAINT IF EXISTS fk_task_162_key_blob_id;
ALTER TABLE erp.task_162 DROP CONSTRAINT IF EXISTS fk_medication_dispense_162_blob_id;
ALTER TABLE erp.task_162 ADD CONSTRAINT fk_task_162_key_blob_id FOREIGN KEY (task_key_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;
ALTER TABLE erp.task_162 ADD CONSTRAINT fk_medication_dispense_162_blob_id FOREIGN KEY (medication_dispense_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

-- create indicies
CREATE INDEX IF NOT EXISTS task_162_kvnr_idx ON erp.task_162 USING hash (kvnr_hashed);
CREATE INDEX IF NOT EXISTS task_162_last_modified_idx ON erp.task_162 USING btree (last_modified);


-- Updating tasks is required as the task progresses
revoke all on  erp.task_162 from role_proc_user;
alter table erp.task_162 owner to role_proc_admin;
grant insert, select, update on erp.task_162 to role_proc_user;
CALL pg_temp.set_task_partitions_permissions('erp.task_162');
alter sequence erp.task_162_taskid_seq owner to role_proc_admin;
grant ALL ON SEQUENCE erp.task_162_taskid_seq TO role_proc_user;


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
       162::smallint AS prescription_type,
       owner,
       last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity,
       last_status_update
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