/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('41');
CALL erp.set_version('42');

CREATE TYPE erp.push_notification_channel AS ENUM (
    'erp.task.activate', 'erp.task.accept', 'erp.task.reject', 'erp.task.close', 'erp.task.dispense', 'erp.task.abort',
    'erp.communication.new', 'erp.task.vertreter', 'erp.chargeitem.create', 'erp.chargeitem.update',
    'erp.eu.prescription.get', 'erp.eu.prescription.redeem', 'erp.eu.prescription.close'
);

ALTER TYPE erp.push_notification_channel OWNER TO role_proc_admin;
GRANT USAGE ON TYPE erp.push_notification_channel TO role_proc_user;

CREATE TABLE IF NOT EXISTS erp.app_registrations (
    pushkey VARCHAR(512) NOT NULL,
    app_id VARCHAR(64) NOT NULL,
    kvnr_hashed BYTEA NOT NULL,
    blob_id INTEGER NOT NULL,
    salt BYTEA NOT NULL,
    payload BYTEA NOT NULL,
    url TEXT NOT NULL,
    encryption_key BYTEA NOT NULL,
    key_identifier TEXT NOT NULL,
    subscribed_channel erp.push_notification_channel[] DEFAULT '{}',
    time_created TIMESTAMP WITH TIME ZONE NOT NULL,
    last_modified TIMESTAMP WITH TIME ZONE NOT NULL,
    PRIMARY KEY (pushkey, app_id, kvnr_hashed)
);

ALTER TABLE erp.app_registrations ALTER COLUMN pushkey SET STORAGE PLAIN;
ALTER TABLE erp.app_registrations ALTER COLUMN app_id SET STORAGE PLAIN;
ALTER TABLE erp.app_registrations ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp.app_registrations ALTER COLUMN subscribed_channel SET STORAGE PLAIN;

REVOKE ALL ON erp.app_registrations FROM role_proc_user;
ALTER TABLE erp.app_registrations OWNER TO role_proc_admin;
GRANT INSERT, SELECT, UPDATE, DELETE ON erp.app_registrations TO role_proc_user;

COMMIT;