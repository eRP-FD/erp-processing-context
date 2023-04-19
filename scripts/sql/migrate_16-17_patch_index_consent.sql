---------------------------------------------
-- (C) Copyright IBM Deutschland GmbH 2023
-- (C) Copyright IBM Corp. 2023
---------------------------------------------


BEGIN;

CALL erp.expect_version('16');
CALL erp.set_version('17');

DROP INDEX IF EXISTS consent_idx;
CREATE INDEX IF NOT EXISTS consent_kvnr_idx ON erp.consent USING hash (kvnr_hashed);

COMMIT;
