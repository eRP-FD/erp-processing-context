---------------------------------------------
-- (C) Copyright IBM Deutschland GmbH 2023
-- (C) Copyright IBM Corp. 2023
---------------------------------------------


BEGIN;

CALL erp.expect_version('14');
CALL erp.set_version('15');

CREATE TABLE IF NOT EXISTS erp.vsdm_key_blob
(
    operator char(1),
    version char(1),
    data bytea NOT NULL,
    generation integer NOT NULL,
    created_date_time timestamp with time zone NOT NULL,
    PRIMARY KEY (operator, version)
);

ALTER TABLE erp.vsdm_key_blob owner to role_proc_admin;
GRANT INSERT, SELECT, UPDATE, DELETE ON erp.vsdm_key_blob to role_proc_user;

COMMIT;