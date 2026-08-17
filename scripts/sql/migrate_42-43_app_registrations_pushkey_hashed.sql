/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('42');
CALL erp.set_version('43');

ALTER TABLE erp.app_registrations
    DROP CONSTRAINT app_registrations_pkey;

ALTER TABLE erp.app_registrations
    ADD COLUMN pushkey_hashed BYTEA,
    ADD COLUMN app_id_hashed BYTEA;

ALTER TABLE erp.app_registrations
    ALTER COLUMN pushkey_hashed SET NOT NULL,
    ALTER COLUMN app_id_hashed SET NOT NULL;

ALTER TABLE erp.app_registrations
    DROP COLUMN pushkey,
    DROP COLUMN app_id,
    DROP COLUMN key_identifier;

ALTER TABLE erp.app_registrations
    ADD PRIMARY KEY (pushkey_hashed, app_id_hashed, kvnr_hashed);

COMMIT;
