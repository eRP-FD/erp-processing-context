---------------------------------------------
-- (C) Copyright IBM Deutschland GmbH 2022
-- (C) Copyright IBM Corp. 2022
---------------------------------------------


BEGIN;

CALL erp.expect_version('12');
CALL erp.set_version('13');

\ir create_partitions_func.sql


CREATE TABLE IF NOT EXISTS erp.task_209 (
    LIKE erp.task_169 INCLUDING ALL
)
PARTITION BY RANGE (prescription_id);

ALTER TABLE erp.task_209 DROP CONSTRAINT IF EXISTS task_209_pkey;
ALTER TABLE erp.task_209 ADD CONSTRAINT task_209_pkey PRIMARY KEY (prescription_id);

ALTER TABLE erp.task_209 ALTER COLUMN kvnr SET STORAGE PLAIN;
ALTER TABLE erp.task_209 ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp.task_209 ALTER COLUMN salt SET STORAGE PLAIN;
ALTER TABLE erp.task_209 ALTER COLUMN access_code SET STORAGE PLAIN;
ALTER TABLE erp.task_209 ALTER COLUMN secret SET STORAGE PLAIN;
ALTER TABLE erp.task_209 ALTER COLUMN performer SET STORAGE PLAIN;

CALL pg_temp.create_task_partitions('erp.task_209');

-- create own counter for this table
CREATE SEQUENCE IF NOT EXISTS erp.task_209_taskid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;

ALTER SEQUENCE erp.task_209_taskid_seq OWNED BY erp.task_209.prescription_id;
ALTER TABLE erp.task_209 ALTER COLUMN prescription_id SET DEFAULT nextval('erp.task_209_taskid_seq'::regclass);



-- create foreign key constraints for table
ALTER TABLE erp.task_209 DROP CONSTRAINT IF EXISTS fk_task_209_key_blob_id;
ALTER TABLE erp.task_209 DROP CONSTRAINT IF EXISTS fk_medication_dispense_209_blob_id;
ALTER TABLE erp.task_209 ADD CONSTRAINT fk_task_209_key_blob_id FOREIGN KEY (task_key_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;
ALTER TABLE erp.task_209 ADD CONSTRAINT fk_medication_dispense_209_blob_id FOREIGN KEY (medication_dispense_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

-- create indicies
CREATE INDEX IF NOT EXISTS task_209_kvnr_idx ON erp.task_209 USING hash (kvnr_hashed);
CREATE INDEX IF NOT EXISTS task_209_last_modified_idx ON erp.task_209 USING btree (last_modified);


-- Updating tasks is required as the task progresses
GRANT UPDATE ON TABLE erp.task_209 TO role_proc_user;
revoke all on  erp.task_209 from role_proc_user;
alter table erp.task_209 owner to role_proc_admin;
grant insert, select, update on erp.task_209 to role_proc_user;
CALL pg_temp.set_task_partitions_permissions('erp.task_209');
alter sequence erp.task_209_taskid_seq owner to role_proc_admin;
grant ALL ON SEQUENCE erp.task_209_taskid_seq TO role_proc_user;


COMMIT;
