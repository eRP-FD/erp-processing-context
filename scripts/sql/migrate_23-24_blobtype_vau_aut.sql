/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('23');
CALL erp.set_version('24');

-- Include new blob type in constraint
ALTER TABLE erp.blob DROP CONSTRAINT blob_type_range;
ALTER TABLE erp.blob ADD CONSTRAINT blob_type_range CHECK (type > 0 AND type <= 14);

COMMIT;