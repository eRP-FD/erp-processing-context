--
-- (C) Copyright IBM Deutschland GmbH 2021
-- (C) Copyright IBM Corp. 2021
--
BEGIN;
CALL erp.expect_version('3');
CALL erp.set_version('4');

-- Define new enum type
CREATE TYPE cmac_type_enum AS ENUM ('user', 'telematic');

-- Add new column
ALTER TABLE erp.vau_cmac ADD COLUMN IF NOT EXISTS cmac_type cmac_type_enum DEFAULT 'user';

-- Change primary key to valid_date.cmac_type
ALTER TABLE erp.vau_cmac DROP CONSTRAINT IF EXISTS erp_vau_cmac_pkey;
ALTER TABLE erp.vau_cmac ADD CONSTRAINT erp_vau_cmac_pkey PRIMARY KEY (valid_date, cmac_type);

END;
