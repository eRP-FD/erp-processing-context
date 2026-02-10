/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
--
-- Name: tg_insert_event; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_task_event ON erp.task;

--
-- Name: tg_insert_event_169; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_task_event_169 ON erp.task_169;


--
-- Name: tg_insert_event_200; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_task_event_200 ON erp.task_200;


--
-- Name: tg_insert_event_209; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_task_event_209 ON erp.task_209;

--
-- Name: tg_insert_event_166; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_task_event_166 ON erp.task_166;

--
-- Name: tg_create_delete_task_event; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_delete_task_event ON erp.task;

--
-- Name: tg_create_delete_task_event_169; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_delete_task_event_169 ON erp.task_169;


--
-- Name: tg_create_delete_task_event_200; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_delete_task_event_200 ON erp.task_200;


--
-- Name: tg_create_delete_task_event_209; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_delete_task_event_209 ON erp.task_209;

--
-- Name: tg_create_delete_task_event_166; Type: TRIGGER; Schema: erp; Owner: -
--

DROP TRIGGER IF EXISTS tg_create_delete_task_event_166 ON erp.task_166;

COMMIT;