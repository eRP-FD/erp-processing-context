/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp_event.expect_version('12');
CALL erp_event.set_version('13');

CREATE TYPE erp_event.push_notification_channel AS ENUM (
    'erp.task.activate',
    'erp.task.accept',
    'erp.task.reject',
    'erp.task.close',
    'erp.task.dispense',
    'erp.task.abort',
    'erp.communication.new',
    'erp.task.vertreter',
    'erp.chargeitem.create',
    'erp.chargeitem.update',
    'erp.eu.prescription.get',
    'erp.eu.prescription.redeem',
    'erp.eu.prescription.close'
);

CREATE TYPE erp_event.push_notification_event_state_type AS ENUM (
    'pending',
    'processing'
);

CREATE SEQUENCE IF NOT EXISTS erp_event.push_notification_event_id_seq;

CREATE TABLE IF NOT EXISTS erp_event.push_notification_event (
    id BIGINT NOT NULL
        DEFAULT nextval('erp_event.push_notification_event_id_seq')
        PRIMARY KEY,
    kvnr_hashed BYTEA NOT NULL,
    next_export TIMESTAMP WITH TIME ZONE NOT NULL,
    created TIMESTAMP WITH TIME ZONE NOT NULL,
    prescription_id BIGINT NOT NULL,
    prescription_type SMALLINT NOT NULL,
    channel_id erp_event.push_notification_channel NOT NULL,
    notification_identifier UUID NOT NULL,
    retry_count INTEGER NOT NULL DEFAULT 0,
    state erp_event.push_notification_event_state_type NOT NULL DEFAULT 'pending'
);

ALTER TYPE erp_event.push_notification_channel OWNER TO role_proc_admin;
GRANT USAGE ON TYPE erp_event.push_notification_channel TO role_proc_user;

ALTER TYPE erp_event.push_notification_event_state_type OWNER TO role_proc_admin;
GRANT USAGE ON TYPE erp_event.push_notification_event_state_type TO role_proc_user;

ALTER SEQUENCE erp_event.push_notification_event_id_seq OWNER TO role_proc_admin;
GRANT ALL ON SEQUENCE erp_event.push_notification_event_id_seq TO role_proc_user;

ALTER TABLE erp_event.push_notification_event OWNER TO role_proc_admin;
GRANT INSERT, SELECT, UPDATE, DELETE ON erp_event.push_notification_event TO role_proc_user;

ALTER SEQUENCE erp_event.push_notification_event_id_seq OWNED BY erp_event.push_notification_event.id;

CREATE INDEX IF NOT EXISTS push_notification_event_next_export_idx ON erp_event.push_notification_event(next_export);

COMMIT;
