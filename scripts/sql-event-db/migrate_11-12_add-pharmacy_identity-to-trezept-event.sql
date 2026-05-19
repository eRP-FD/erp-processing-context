/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('11');
CALL erp_event.set_version('12');

--
-- Name: f_on_insert_task_event; Type: FUNCTION: Schema: erp_event; Owner: -
--
/* Update the function for creating kvnr entries and T-Rezept events to include the pharmacy_identity for T-Rezept  */
CREATE OR REPLACE FUNCTION erp_event.f_on_insert_task_event()
    RETURNS trigger
    LANGUAGE plpgsql
    AS
    $$
        BEGIN
         /* Create or update kvnr entry for ePA export  */
        INSERT INTO erp_event.kvnr(kvnr_hashed, next_export, state, retry_count)
            VALUES (NEW.kvnr_hashed, NOW(), 'pending', 0)
            ON CONFLICT (kvnr_hashed) DO UPDATE
            SET state = 'pending', 
                next_export = 
                CASE WHEN erp_event.kvnr.state = 'processed'
                    THEN NOW()
                    ELSE erp_event.kvnr.next_export
                END
                , retry_count = 
                CASE WHEN erp_event.kvnr.state = 'processed'
                    THEN 0
                    ELSE erp_event.kvnr.retry_count
                END
            WHERE erp_event.kvnr.state <> 'pending';

        /* Create a new T-Rezept event  */

        IF (NEW.prescription_type = 166 AND NEW.status = 3) THEN
             -- Copy task_event to trezept_event
            INSERT INTO
                erp_event.trezept_event (
                    id,
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
                    pharmacy_identity,
                    next_export
                )
                VALUES (
                    NEW.id,
                    NEW.prescription_id,
                    NEW.kvnr,
                    NEW.kvnr_hashed,
                    NEW.last_modified,
                    NEW.authored_on,
                    NEW.expiry_date,
                    NEW.accept_date,
                    NEW.status,
                    NEW.task_key_blob_id,
                    NEW.salt,
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
                    NEW.usecase,
                    NEW.prescription_type,
                    NEW.pharmacy_identity,
                    NOW()
                );
        END IF;
        RETURN NEW;
        END; 
    $$;

COMMIT;