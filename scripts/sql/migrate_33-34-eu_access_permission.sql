/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('33');
CALL erp.set_version('34');

CREATE TABLE IF NOT EXISTS erp.eu_country_codes
(
    iso_3166_alpha_2 character(2) PRIMARY KEY NOT NULL,
    last_modified timestamp with time zone NOT NULL DEFAULT now()
);

revoke all on erp.eu_country_codes from role_proc_user;
alter table erp.eu_country_codes owner to role_proc_admin;
grant insert, select, update on erp.eu_country_codes to role_proc_user;

CREATE TABLE IF NOT EXISTS erp.eu_access_permission
(
    kvnr_hashed bytea PRIMARY KEY NOT NULL,
    country_code character(2) NOT NULL,
    access_code bytea NOT NULL,
    blob_id integer NOT NULL,
    salt bytea NOT NULL,
    expires timestamp with time zone NOT NULL
);

ALTER TABLE erp.eu_access_permission ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp.eu_access_permission ALTER COLUMN access_code SET STORAGE PLAIN;

revoke all on erp.eu_access_permission from role_proc_user;
alter table erp.eu_access_permission owner to role_proc_admin;
grant insert, select, update on erp.eu_access_permission to role_proc_user;
grant select, delete on erp.eu_access_permission to role_proc_maintenance;


COMMIT;