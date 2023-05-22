/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('9');
CALL erp.set_version('10');

\ir create_partitions_func.sql

---------------------------------------------------------------------
-- Create Tables for tasks charge items with workflow type 200/209 --
---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS erp.charge_item (
prescription_type smallint NOT NULL,
prescription_id bigint NOT NULL,
enterer bytea NOT NULL,
entered_date timestamp with time zone NOT NULL,
last_modified timestamp with time zone NOT NULL,
marking_flag bytea,
blob_id integer NOT NULL,
salt bytea NOT NULL,
access_code bytea NOT NULL,
kvnr bytea NOT NULL,
kvnr_hashed bytea NOT NULL,
prescription bytea NOT NULL,
prescription_json bytea NOT NULL,
receipt_xml bytea NOT NULL,
receipt_json bytea NOT NULL,
billing_data bytea NOT NULL,
billing_data_json bytea NOT NULL

CONSTRAINT prescription_type_pkv CHECK (prescription_type = 200::SMALLINT OR prescription_type = 209::SMALLINT)
)
PARTITION BY RANGE (prescription_id);

ALTER TABLE erp.charge_item ADD CONSTRAINT charge_item_pkey PRIMARY KEY (prescription_type, prescription_id);
ALTER TABLE erp.charge_item ADD CONSTRAINT fk_charge_item_blob_id FOREIGN KEY (blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

ALTER TABLE erp.charge_item ALTER COLUMN prescription_type SET STORAGE PLAIN;
ALTER TABLE erp.charge_item ALTER COLUMN prescription_id SET STORAGE PLAIN;
ALTER TABLE erp.charge_item ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;

CREATE INDEX IF NOT EXISTS charge_item_kvnr_idx ON erp.charge_item USING hash (kvnr_hashed);
CREATE INDEX IF NOT EXISTS charge_item_entered_date_idx ON erp.charge_item USING btree (entered_date);
CREATE INDEX IF NOT EXISTS charge_item_last_modified_idx ON erp.charge_item USING btree (last_modified);

CALL pg_temp.create_task_partitions('erp.charge_item');
CALL pg_temp.set_task_partitions_permissions('erp.charge_item');

REVOKE ALL ON erp.charge_item from role_proc_user;
ALTER TABLE erp.charge_item owner to role_proc_admin;
GRANT INSERT, SELECT, UPDATE, DELETE ON erp.charge_item to role_proc_user;

------------------------------------------------------
-- Remove charge-item-related columns in task table --
------------------------------------------------------
ALTER TABLE erp.task_200 DROP COLUMN charge_item_enterer;
ALTER TABLE erp.task_200 DROP COLUMN charge_item_entered_date;
ALTER TABLE erp.task_200 DROP COLUMN charge_item;
ALTER TABLE erp.task_200 DROP COLUMN dispense_item;

COMMIT;
