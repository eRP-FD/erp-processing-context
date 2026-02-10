/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


BEGIN;
--
-- Name: tg_create_task_event; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event
    AFTER UPDATE ON erp.task
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(160);


--
-- Name: tg_create_task_event_169; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_169
    AFTER UPDATE ON erp.task_169
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(169);


--
-- Name: tg_create_task_event_200; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_200
    AFTER UPDATE ON erp.task_200
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(200);


--
-- Name: tg_create_task_event_209; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_209
    AFTER UPDATE ON erp.task_209
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(209);


--
-- Name: tg_create_task_event_166; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_166
    AFTER UPDATE ON erp.task_166
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(166);



/* A_26264 DELETE trigger for tasks to create the delete task events   */
--
-- Name: tg_create_delete_task_event; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_delete_task_event
    AFTER DELETE ON erp.task
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_delete_task_event(160);


--
-- Name: tg_create_delete_task_event_169; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_delete_task_event_169
    AFTER DELETE ON erp.task_169
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_delete_task_event(169);


--
-- Name: tg_create_delete_task_event_200; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_delete_task_event_200
    AFTER DELETE ON erp.task_200
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_delete_task_event(200);


--
-- Name:tg_create_delete_task_event_209; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_delete_task_event_209
    AFTER DELETE ON erp.task_209
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_delete_task_event(209);

--
-- Name: tg_create_delete_task_event_166; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_delete_task_event_166
    AFTER DELETE ON erp.task_166
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_delete_task_event(166);


COMMIT;