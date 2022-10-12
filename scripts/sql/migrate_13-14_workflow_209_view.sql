---------------------------------------------
-- (C) Copyright IBM Deutschland GmbH 2022
-- (C) Copyright IBM Corp. 2022
---------------------------------------------


BEGIN;

CALL erp.expect_version('13');
CALL erp.set_version('14');

CREATE OR REPLACE VIEW erp.task_view AS (
        SELECT
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
                160::smallint AS prescription_type
            FROM erp.task
    UNION
        SELECT
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
                169::smallint AS prescription_type
            FROM erp.task_169
    UNION
        SELECT
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
                200::smallint AS prescription_type
            FROM erp.task_200
    UNION
        SELECT
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
                209::smallint AS prescription_type
            FROM erp.task_209
);

alter view erp.task_view owner to role_proc_admin;
revoke all on erp.task_view from role_proc_admin;
revoke all on erp.task_view from role_proc_user;
grant select on erp.task_view to role_proc_admin, role_proc_user;

COMMIT;
