/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('9');
CALL erp_event.set_version('10');

CREATE TYPE erp_event.trezept_event_state_type AS ENUM ('pending', 'deadLetterQueue', 'processing');

CREATE TABLE
    erp_event.trezept_event (LIKE erp_event.task_event INCLUDING ALL);

ALTER TABLE
    erp_event.trezept_event
ADD COLUMN next_export timestamp with time zone DEFAULT CURRENT_TIMESTAMP::timestamp with time zone NOT NULL;

ALTER TABLE
    erp_event.trezept_event
ALTER COLUMN state DROP DEFAULT,
ALTER COLUMN state TYPE erp_event.trezept_event_state_type USING state::text::erp_event.trezept_event_state_type,
ALTER COLUMN state SET DEFAULT 'pending';

COMMIT;
