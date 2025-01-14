/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('5');
CALL erp_event.set_version('6');

DROP INDEX erp_event.kvnr_next_export_idx;

--
-- Name: kvnr_state_next_export_idx; Type: INDEX; Schema: erp_event; Owner: -
--

CREATE INDEX kvnr_state_next_export_idx ON erp_event.kvnr USING btree (state, next_export);

COMMIT;