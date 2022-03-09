--
-- (C) Copyright IBM Deutschland GmbH 2022
-- (C) Copyright IBM Corp. 2022
--
BEGIN;
CALL erp.expect_version('7');
CALL erp.set_version('8');

-- Add new column
ALTER TABLE erp.blob ADD COLUMN IF NOT EXISTS vau_certificate varchar;

END;
