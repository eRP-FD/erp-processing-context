/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('26');
CALL erp.set_version('27');

ALTER TABLE erp.task
    ADD COLUMN IF NOT EXISTS doctor_identity bytea;
ALTER TABLE erp.task
    ADD COLUMN IF NOT EXISTS pharmacy_identity bytea;

ALTER TABLE erp.task_169
    ADD COLUMN IF NOT EXISTS doctor_identity bytea;
ALTER TABLE erp.task_169
    ADD COLUMN IF NOT EXISTS pharmacy_identity bytea;

ALTER TABLE erp.task_200
    ADD COLUMN IF NOT EXISTS doctor_identity bytea;
ALTER TABLE erp.task_200
    ADD COLUMN IF NOT EXISTS pharmacy_identity bytea;

ALTER TABLE erp.task_209
    ADD COLUMN IF NOT EXISTS doctor_identity bytea;
ALTER TABLE erp.task_209
    ADD COLUMN IF NOT EXISTS pharmacy_identity bytea;

ALTER TABLE  erp_event.task_event
    ADD COLUMN IF NOT EXISTS doctor_identity bytea;
ALTER TABLE  erp_event.task_event
    ADD COLUMN IF NOT EXISTS pharmacy_identity bytea;

CREATE OR REPLACE VIEW erp.task_view AS
(
SELECT prescription_id,
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
       160::smallint AS prescription_type,
       owner,
	   last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity
FROM erp.task
UNION ALL
SELECT prescription_id,
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
       169::smallint AS prescription_type,
       owner,
	   last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity
FROM erp.task_169
UNION ALL
SELECT prescription_id,
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
       200::smallint AS prescription_type,
       owner,
	   last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity
FROM erp.task_200
UNION ALL
SELECT prescription_id,
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
       209::smallint AS prescription_type,
       owner,
	   last_medication_dispense,
       medication_dispense_salt,
       doctor_identity,
       pharmacy_identity
FROM erp.task_209
    );


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
            v_healthcare_provider_prescription bytea;
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
            v_healthcare_provider_prescription = NEW.healthcare_provider_prescription;
            IF (OLD.status = 0 AND NEW.status = 1) THEN
                /* UC 2.3 activate */
                IF(NEW.doctor_identity IS NOT NULL) THEN
                    /* Only create the event when the task has the data from the ACCESS_TOKEN of the doctor */
                    v_usecase = 'providePrescription';
                END IF;
            ELSIF (OLD.status = 1 AND NEW.status = 4) THEN
                /* UC 2.5 / UC 3.2 abort */
                v_usecase = 'cancelPrescription';
                /* Take these values from the old task because they are null in the new task but needed in the event */
                v_kvnr = OLD.kvnr;
                v_task_key_blob_id = OLD.task_key_blob_id;
                v_salt = OLD.salt;
                /* The healthcare_provider_prescription is needed from the old taks because the authoredOn from the prescription is needed*/
                v_healthcare_provider_prescription = OLD.healthcare_provider_prescription;
            ELSIF (OLD.status = 2 AND NEW.status = 4) THEN
                /* UC 4.3 abort */
                v_usecase = 'cancelPrescription';
                /* Take these values from the old task because they are null in the new task but needed in the event */
                v_kvnr = OLD.kvnr;
                v_task_key_blob_id = OLD.task_key_blob_id;
                v_salt = OLD.salt;
                /* The healthcare_provider_prescription is needed from the old taks because the authoredOn from the prescription is needed*/
                v_healthcare_provider_prescription = OLD.healthcare_provider_prescription;
            ELSIF (OLD.status = 2 AND NEW.status = 3) THEN
                /* UC 4.4 close */
                IF (NEW.doctor_identity IS NOT NULL AND NEW.pharmacy_identity IS NOT NULL) THEN
                    /* Only create the event when the task has the data from the ACCESS_TOKEN of the doctor and from the pharmacy */
                    v_usecase = 'provideDispensation';
                END IF;
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
                        v_healthcare_provider_prescription,
                        NEW.receipt,
                        NEW.when_handed_over,
                        NEW.when_prepared,
                        NEW.performer,
                        NEW.medication_dispense_blob_id,
                        NEW.medication_dispense_bundle,
                        NEW.last_medication_dispense,
                        NEW.medication_dispense_salt,
                        v_usecase,
                        v_prescription_type,
                        NEW.doctor_identity,
                        NEW.pharmacy_identity
                    ) RETURNING id INTO v_id;

                DELETE FROM erp_event.task_event
                where
                    id = v_id;

            END IF;
            RETURN NEW;
        END;
    $$;

COMMIT;
