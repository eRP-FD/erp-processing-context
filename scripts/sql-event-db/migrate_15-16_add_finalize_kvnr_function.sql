/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp_event.expect_version('15');
CALL erp_event.set_version('16');

--
-- Name: f_finalize_kvnr; Type: FUNCTION: Schema: erp_event; Owner: -
--
/* Create or update the function for finish processing a kvnr*/
CREATE OR REPLACE FUNCTION erp_event.f_finalize_kvnr(kvnr_hashed bytea, assigned_epa varchar)
    RETURNS integer
    LANGUAGE plpgsql
    AS
    $$
        DECLARE
            updated_rows integer;
        BEGIN
            UPDATE erp_event.kvnr AS kvnr
            SET state = CASE WHEN kvnr.state = 'processing' THEN 'processed' ELSE kvnr.state END,
                retry_count = 0,
                assigned_epa = f_finalize_kvnr.assigned_epa,
                last_consent_check = NOW(),
                next_export = CASE WHEN kvnr.state = 'pending' THEN NOW() + (3 * interval '1 second') ELSE NULL END
            WHERE kvnr.kvnr_hashed = f_finalize_kvnr.kvnr_hashed
              AND kvnr.state IN ('processing', 'pending');
            GET DIAGNOSTICS updated_rows = ROW_COUNT;
            RETURN updated_rows;
        END;
    $$;

COMMIT;