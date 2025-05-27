/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('7');
CALL erp_event.set_version('8');

ALTER TABLE erp_event.kvnr ALTER COLUMN next_export DROP NOT NULL;

DROP INDEX IF EXISTS erp_event.kvnr_state_next_export_idx;

COMMIT;

--
-- Name: kvnr_next_export_idx; Type: INDEX; Schema: erp_event; Owner: -
--

CREATE INDEX CONCURRENTLY kvnr_next_export_idx ON erp_event.kvnr USING btree (next_export);