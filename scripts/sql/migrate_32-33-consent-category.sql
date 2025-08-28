/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;
CALL erp.expect_version('32');
CALL erp.set_version('33');

CREATE TYPE consent_category_enum AS ENUM ('CHARGCONS', 'EUDISPCONS');

ALTER TABLE erp.consent ADD COLUMN IF NOT EXISTS category consent_category_enum NOT NULL DEFAULT 'CHARGCONS';

ALTER TABLE erp.consent DROP CONSTRAINT IF EXISTS consent_pkey;
ALTER TABLE erp.consent ADD CONSTRAINT consent_pkey PRIMARY KEY (kvnr_hashed, category);

COMMIT;