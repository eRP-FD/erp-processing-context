/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

--
-- Name: task_event_pub; Type: PULICATION; Schema: erp; Owner: -
--

ALTER TABLE erp_event.task_event REPLICA IDENTITY FULL; 

DROP PUBLICATION IF EXISTS task_event_pub;

CREATE PUBLICATION task_event_pub 
    FOR TABLE erp_event.task_event	
    WITH(publish='insert');

COMMIT;

SELECT pg_create_logical_replication_slot('task_event_slot', 'pgoutput');