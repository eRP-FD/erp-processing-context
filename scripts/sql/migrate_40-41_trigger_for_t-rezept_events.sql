/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('40');
CALL erp.set_version('41');

--
-- Name: tg_create_task_event_166; Type: TRIGGER; Schema: erp; Owner: -
--

CREATE OR REPLACE TRIGGER tg_create_task_event_166
    AFTER UPDATE ON erp.task_166
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_task_event(166);

--
-- Name: tg_create_delete_task_event_166; Type: TRIGGER; Schema: erp; Owner: -
--
/* A_26264 DELETE trigger for task_event_166 to create the delete task events   */
CREATE OR REPLACE TRIGGER tg_create_delete_task_event_166
    AFTER DELETE ON erp.task_166
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_create_delete_task_event(166);


COMMIT;