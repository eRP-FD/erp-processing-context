/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('10');
CALL erp_event.set_version('11');

ALTER TABLE erp_event.trezept_event OWNER TO role_proc_admin;

-- Allow insert, select, delete for processing context role on trezept_event
GRANT INSERT, SELECT, UPDATE, DELETE ON erp_event.trezept_event TO role_proc_user;

-- Allow select for maintenacne role on trezept_event for statistics
GRANT SELECT ON erp_event.trezept_event TO role_proc_maintenance;


/* Rename the trigger to reflect the new functionality for creating a new T-Rezept event  */
ALTER TRIGGER tg_insert_update_kvnr ON erp_event.task_event RENAME TO tg_on_insert_task_event;

--
-- Name: f_insert_update_kvnr; Type: FUNCTION: Schema: erp_event; Owner: -
--
/* Create a new function for creating kvnr entries and T-Rezept events  */
CREATE FUNCTION erp_event.f_on_insert_task_event()
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
                    NOW()
                );
        END IF;
        RETURN NEW;
        END; 
    $$;

/* Update the trigger to call the new function  */
CREATE OR REPLACE TRIGGER tg_on_insert_task_event
    AFTER INSERT ON erp_event.task_event
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_on_insert_task_event();

ALTER TABLE erp_event.task_event ENABLE REPLICA TRIGGER tg_on_insert_task_event;

/* Delete the old function */
DROP FUNCTION IF EXISTS erp_event.f_insert_update_kvnr;

COMMIT;

/* Create index for trezept_event */
CREATE INDEX CONCURRENTLY IF NOT EXISTS trezept_event_id_idx ON erp_event.trezept_event(id);
CREATE INDEX CONCURRENTLY trezept_event_next_export_idx ON erp_event.trezept_event(next_export);