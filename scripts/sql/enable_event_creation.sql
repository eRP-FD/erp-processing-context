/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

--
-- Name: tg_insert_event; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event
    AFTER UPDATE ON erp.task
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(160);


--
-- Name: tg_insert_event_169; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_169
    AFTER UPDATE ON erp.task_169
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(169);


--
-- Name: tg_insert_event_200; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_200
    AFTER UPDATE ON erp.task_200
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(200);


--
-- Name: tg_insert_event_209; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_209
    AFTER UPDATE ON erp.task_209
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(209);
