---------------------------------------------
-- (C) Copyright IBM Deutschland GmbH 2021 --
-- (C) Copyright IBM Corp. 2021            --
---------------------------------------------
BEGIN;

CALL erp.expect_version('6');
CALL erp.set_version('7');

CREATE TABLE IF NOT EXISTS erp.consent (
    kvnr_hashed bytea NOT NULL PRIMARY KEY,
    date_time timestamp with time zone NOT NULL
);

ALTER TABLE erp.consent ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
CREATE INDEX IF NOT EXISTS consent_idx ON erp.task_200 USING hash (kvnr_hashed);

REVOKE ALL ON erp.consent from role_proc_user;
ALTER TABLE erp.consent owner to role_proc_admin;
GRANT INSERT, SELECT, UPDATE, DELETE ON erp.consent to role_proc_user;

COMMIT;
