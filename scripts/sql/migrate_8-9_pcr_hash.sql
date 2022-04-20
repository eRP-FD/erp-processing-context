--
-- (C) Copyright IBM Deutschland GmbH 2022
-- (C) Copyright IBM Corp. 2022
--
BEGIN;
CALL erp.expect_version('8');
CALL erp.set_version('9');

-- Add new column
ALTER TABLE erp.blob ADD COLUMN IF NOT EXISTS pcr_hash bytea DEFAULT NULL;

END;
