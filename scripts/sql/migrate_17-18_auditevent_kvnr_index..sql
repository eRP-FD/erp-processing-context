/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('17');
CALL erp.set_version('18');

DROP INDEX IF EXISTS auditevent_kvnr_idx;
CREATE INDEX IF NOT EXISTS auditevent_kvnrid_idx ON erp.auditevent (kvnr_hashed,id);

COMMIT;
