/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('43');
CALL erp.set_version('44');

CREATE INDEX IF NOT EXISTS app_registrations_kvnr_idx ON erp.app_registrations USING hash (kvnr_hashed);

COMMIT;
