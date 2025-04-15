/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

begin;
CALL erp.expect_version('19');
CALL erp.set_version('20');

DROP INDEX erp.auditevent_kvnr_idx;
DROP INDEX erp.auditevent_prescription_id_idx;
DROP INDEX erp.auditevent_prescription_type_idx;

ALTER INDEX erp.auditevent_kvnrid_idx RENAME TO auditevent_kvnr_hashed_id_idx;

end;
