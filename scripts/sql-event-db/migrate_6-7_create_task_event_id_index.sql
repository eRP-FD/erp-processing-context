/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('6');
CALL erp_event.set_version('7');

COMMIT;
--
-- Name: task_event_id_idx; Type: INDEX; Schema: erp_event; Owner: -
--

CREATE INDEX CONCURRENTLY IF NOT EXISTS task_event_id_idx ON erp_event.task_event(id);
