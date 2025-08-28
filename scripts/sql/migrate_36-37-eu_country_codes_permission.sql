/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('36');
CALL erp.set_version('37');

DROP ROLE IF EXISTS role_proc_external_data_updater;

CREATE ROLE role_proc_external_data_updater WITH NOLOGIN NOINHERIT;

GRANT USAGE ON SCHEMA erp TO role_proc_external_data_updater;
GRANT INSERT, SELECT, UPDATE, DELETE ON erp.eu_country_codes TO role_proc_external_data_updater;

REVOKE INSERT, UPDATE ON erp.eu_country_codes FROM role_proc_user;

COMMIT;