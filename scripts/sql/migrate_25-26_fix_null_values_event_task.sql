/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('25');
CALL erp.set_version('26');

--
-- Name: f_create_task_event; Type: FUNCTION; Schema: erp_event; Owner: -
--

CREATE OR REPLACE FUNCTION erp_event.f_create_task_event()
    RETURNS trigger
    LANGUAGE plpgsql
    AS
    $$
        DECLARE 
            v_id bigint;
            v_usecase erp_event.usecase_type;
            v_prescription_type smallint;
            v_kvnr bytea;
            v_task_key_blob_id integer;
            v_salt bytea;
        BEGIN
            IF(TG_NARGS = 1) THEN
                v_prescription_type = TG_ARGV[0];
            ELSE
                v_prescription_type = 160;
            END IF;
          
            /* Set the intital values for these variables to the values from the new task */
            v_kvnr = NEW.kvnr;
            v_task_key_blob_id = NEW.task_key_blob_id;
            v_salt = NEW.salt;

            IF (OLD.status = 0 AND NEW.status = 1) THEN
                /* UC 2.3 activate */
                v_usecase = 'providePrescription';
            ELSIF (OLD.status = 1 AND NEW.status = 4) THEN
                /* UC 2.5 / UC 3.2 abort */
                v_usecase = 'cancelPrescription';
                /* Take these values from the old task because they are null in the new task but needed in the event */
                v_kvnr = OLD.kvnr;
                v_task_key_blob_id = OLD.task_key_blob_id;
                v_salt = OLD.salt;               
            ELSIF (OLD.status = 2 AND NEW.status = 4) THEN
                /* UC 4.3 abort */
                v_usecase = 'cancelPrescription';
                /* Take these values from the old task because they are null in the new task but needed in the event */
                v_kvnr = OLD.kvnr;
                v_task_key_blob_id = OLD.task_key_blob_id;
                v_salt = OLD.salt;  
            ELSIF (OLD.status = 2 AND NEW.status = 3) THEN
                /* UC 4.4 close */ 
                v_usecase = 'provideDispensation';
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
                        prescription_type                        
                    )
                VALUES
                    (
                        NEW.prescription_id,
                        v_kvnr,
                        NEW.kvnr_hashed,
                        NEW.last_modified,
                        NEW.authored_on,
                        NEW.expiry_date,
                        NEW.accept_date,
                        NEW.status,
                        v_task_key_blob_id,
                        v_salt,
                        NEW.access_code,
                        NEW.secret,
                        NEW.healthcare_provider_prescription,
                        NEW.receipt,
                        NEW.when_handed_over,
                        NEW.when_prepared,
                        NEW.performer,
                        NEW.medication_dispense_blob_id,
                        NEW.medication_dispense_bundle,
                        NEW.last_medication_dispense,
                        NEW.medication_dispense_salt,
                        v_usecase,
                        v_prescription_type
                    ) RETURNING id INTO v_id;

                DELETE FROM erp_event.task_event
                where
                    id = v_id;

            END IF;
            RETURN NEW;
        END;
    $$;

COMMIT;
