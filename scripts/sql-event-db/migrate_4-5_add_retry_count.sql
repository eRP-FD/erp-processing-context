/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('4');
CALL erp_event.set_version('5');

ALTER TABLE  erp_event.task_event
    ADD COLUMN IF NOT EXISTS retry_count integer;
ALTER TABLE  erp_event.kvnr
    ADD COLUMN IF NOT EXISTS retry_count integer;

--
-- Name: f_insert_update_kvnr; Type: FUNCTION: Schema: erp_event; Owner: -
--

CREATE OR REPLACE FUNCTION erp_event.f_insert_update_kvnr()
    RETURNS trigger
    LANGUAGE plpgsql
    AS
    $$
        BEGIN
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
        RETURN NEW;
        END; 
    $$;

COMMIT;