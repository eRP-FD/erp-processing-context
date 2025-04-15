/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('16');
CALL erp.set_version('17');

DROP INDEX IF EXISTS consent_idx;
CREATE INDEX IF NOT EXISTS consent_kvnr_idx ON erp.consent USING hash (kvnr_hashed);

COMMIT;
