/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

BEGIN;

CALL erp.expect_version('10');
CALL erp.set_version('11');

-- Include new blob type in constraint
ALTER TABLE erp.blob DROP CONSTRAINT blob_type_range;
ALTER TABLE erp.blob ADD CONSTRAINT blob_type_range CHECK (type > 0 AND type <= 12);

COMMIT;