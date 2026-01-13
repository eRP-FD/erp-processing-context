/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('8');
CALL erp_event.set_version('9');

COMMIT;

--
-- Name: kvnr_last_consent_check_idx; Type: INDEX; Schema: erp_event; Owner: -
--

CREATE INDEX CONCURRENTLY kvnr_last_consent_check_idx ON erp_event.kvnr USING btree (last_consent_check);