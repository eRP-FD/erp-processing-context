/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

BEGIN;

CALL erp.expect_version('5');
CALL erp.set_version('6');

\ir create_partitions_func.sql

----------------------------------------------------
-- Create Tables for tasks with workflow type 200 --
----------------------------------------------------
CREATE TABLE IF NOT EXISTS erp.task_200 (
    LIKE erp.task INCLUDING ALL,
    charge_item_enterer bytea,
    charge_item_entered_date timestamp with time zone,
    charge_item bytea,
    dispense_item bytea,
    CONSTRAINT fk_task_200_key_blob_id FOREIGN KEY (task_key_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT
) PARTITION BY RANGE (prescription_id);

CREATE SEQUENCE IF NOT EXISTS erp.task_200_taskid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;

REVOKE ALL ON  erp.task_200 from role_proc_user;
ALTER TABLE erp.task_200 owner to role_proc_admin;
GRANT INSERT, SELECT, UPDATE ON erp.task_200 to role_proc_user;
ALTER SEQUENCE erp.task_200_taskid_seq owner to role_proc_admin;
GRANT ALL ON SEQUENCE erp.task_200_taskid_seq TO role_proc_user;

ALTER TABLE erp.task_200 ALTER COLUMN kvnr SET STORAGE PLAIN;
ALTER TABLE erp.task_200 ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp.task_200 ALTER COLUMN salt SET STORAGE PLAIN;
ALTER TABLE erp.task_200 ALTER COLUMN access_code SET STORAGE PLAIN;
ALTER TABLE erp.task_200 ALTER COLUMN secret SET STORAGE PLAIN;
ALTER TABLE erp.task_200 ALTER COLUMN performer SET STORAGE PLAIN;
ALTER TABLE erp.task_200 ALTER COLUMN charge_item_enterer SET STORAGE PLAIN;

CREATE INDEX IF NOT EXISTS task_200_kvnr_idx ON erp.task_200 USING hash (kvnr_hashed);
CREATE INDEX IF NOT EXISTS task_200_last_modified_idx ON erp.task_200 USING btree (last_modified);
CREATE INDEX IF NOT EXISTS task_200_charge_item_enterer_idx ON erp.task_200 USING hash (charge_item_enterer);
CREATE INDEX IF NOT EXISTS task_200_charge_item_entered_date_idx ON erp.task_200 USING btree (charge_item_entered_date);

ALTER SEQUENCE erp.task_200_taskid_seq OWNED BY erp.task_200.prescription_id;
ALTER TABLE erp.task_200 ALTER COLUMN prescription_id SET DEFAULT nextval('erp.task_200_taskid_seq'::regclass);

CALL pg_temp.create_task_partitions('erp.task_200');
CALL pg_temp.set_task_partitions_permissions('erp.task_200');

----------------------------------------
-- Create a view over all task tables --
----------------------------------------

CREATE OR REPLACE VIEW erp.task_view AS (
        SELECT
                prescription_id,
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
                160::smallint AS prescription_type
            FROM erp.task
    UNION
        SELECT
                prescription_id,
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
                169::smallint AS prescription_type
            FROM erp.task_169
    UNION
        SELECT
                prescription_id,
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
                200::smallint AS prescription_type
            FROM erp.task_200
);


COMMIT;
