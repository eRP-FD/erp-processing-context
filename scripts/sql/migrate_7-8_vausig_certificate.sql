/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */
BEGIN;
CALL erp.expect_version('7');
CALL erp.set_version('8');

-- Add new column
ALTER TABLE erp.blob ADD COLUMN IF NOT EXISTS vau_certificate varchar;

END;
