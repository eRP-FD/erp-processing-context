/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('30');
CALL erp.set_version('31');

--
-- Name: f_create_delete_task_event; Type: FUNCTION; Schema: erp_event; Owner: -
--

CREATE OR REPLACE FUNCTION erp_event.f_create_delete_task_event()
    RETURNS trigger
    LANGUAGE plpgsql
    AS
    $$
        DECLARE 
            v_id bigint;
            v_usecase erp_event.usecase_type;
            v_prescription_type smallint;
        BEGIN
            IF(TG_NARGS = 1) THEN
                v_prescription_type = TG_ARGV[0];
            ELSE
                v_prescription_type = 160;
            END IF;
          
            IF (OLD.status = 1) THEN
                /* A_26264 */
                v_usecase = 'cancelPrescription';
            ELSIF (OLD.status = 2) THEN
                /* A_26264 */
                v_usecase = 'cancelPrescription';
            ELSE
                v_usecase = NULL;
            END IF;

            IF v_usecase IS NOT NULL THEN
            -- create event from task
                INSERT INTO
                    erp_event.task_event (
                        prescription_id,
                        kvnr,
                        kvnr_hashed,
                        last_modified,
                        authored_on,
                        expiry_date,
                        accept_date,
                        status,
                        task_key_blob_id,
                        salt,
                        access_code,
                        secret,
                        healthcare_provider_prescription,
                        receipt,
                        when_handed_over,
                        when_prepared,
                        performer,
                        medication_dispense_blob_id,
                        medication_dispense_bundle,
                        last_medication_dispense,
                        medication_dispense_salt,
                        usecase,
                        prescription_type,
                        doctor_identity,
                        pharmacy_identity                     
                    )
                VALUES
                    (
                        OLD.prescription_id,
                        OLD.kvnr,
                        OLD.kvnr_hashed,
                        OLD.last_modified,
                        OLD.authored_on,
                        OLD.expiry_date,
                        OLD.accept_date,
                        OLD.status,
                        OLD.task_key_blob_id,
                        OLD.salt,
                        OLD.access_code,
                        OLD.secret,
                        OLD.healthcare_provider_prescription,
                        OLD.receipt,
                        OLD.when_handed_over,
                        OLD.when_prepared,
                        OLD.performer,
                        OLD.medication_dispense_blob_id,
                        OLD.medication_dispense_bundle,
                        OLD.last_medication_dispense,
                        OLD.medication_dispense_salt,
                        v_usecase,
                        v_prescription_type,
                        OLD.doctor_identity,
                        OLD.pharmacy_identity
                    ) RETURNING id INTO v_id;

                DELETE FROM erp_event.task_event
                where
                    id = v_id;

            END IF;
            RETURN NEW;
        END;
    $$;


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

COMMIT;