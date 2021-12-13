--
-- (C) Copyright IBM Deutschland GmbH 2021
-- (C) Copyright IBM Corp. 2021
--
BEGIN;
CALL erp.expect_version('1');
CALL erp.set_version('2');

-- create foreign key constraints for table auditevent
ALTER TABLE erp.auditevent DROP CONSTRAINT IF EXISTS fk_blob_id;
ALTER TABLE erp.auditevent ADD CONSTRAINT fk_blob_id FOREIGN KEY (blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

-- create foreign key constraints for table communication
ALTER TABLE erp.communication DROP CONSTRAINT IF EXISTS fk_sender_blob_id;
ALTER TABLE erp.communication DROP CONSTRAINT IF EXISTS fk_recipient_blob_id;
ALTER TABLE erp.communication ADD CONSTRAINT fk_sender_blob_id FOREIGN KEY (sender_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;
ALTER TABLE erp.communication ADD CONSTRAINT fk_recipient_blob_id FOREIGN KEY (recipient_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

-- create foreign key constraints for table task
ALTER TABLE erp.task DROP CONSTRAINT IF EXISTS fk_task_key_blob_id;
ALTER TABLE erp.task DROP CONSTRAINT IF EXISTS fk_medication_dispense_blob_id;
ALTER TABLE erp.task ADD CONSTRAINT fk_task_key_blob_id FOREIGN KEY (task_key_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;
ALTER TABLE erp.task ADD CONSTRAINT fk_medication_dispense_blob_id FOREIGN KEY (medication_dispense_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

-- create foreign key constraints for table account
ALTER TABLE erp.account DROP CONSTRAINT IF EXISTS fk_blob_id;
ALTER TABLE erp.account ADD CONSTRAINT fk_blob_id FOREIGN KEY (blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;

END;
