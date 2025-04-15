/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CREATE TABLE IF NOT EXISTS erp_event.config (
    parameter VARCHAR(31) PRIMARY KEY NOT NULL,
    value TEXT
);

INSERT INTO erp_event.config (parameter, value) VALUES ('schema_version', '1') ON CONFLICT DO NOTHING;

CREATE OR REPLACE PROCEDURE erp_event.expect_version(expected_version text)
    LANGUAGE plpgsql AS $$
    DECLARE current_version text;
    BEGIN
        SELECT value FROM erp_event.config WHERE parameter = 'schema_version' INTO current_version;
        IF current_version != expected_version THEN
            RAISE EXCEPTION 'Expected schema_version to be %, but current_version is %.', expected_version, current_version;
        END IF;
    END
$$;

CREATE OR REPLACE PROCEDURE erp_event.set_version(version text)
    LANGUAGE plpgsql AS $$
    DECLARE count integer;
    BEGIN
        UPDATE erp_event.config SET value = version WHERE parameter = 'schema_version';
        GET DIAGNOSTICS count = ROW_COUNT;
        IF count != 1 THEN
            RAISE EXCEPTION 'Updating schema_version failed.';
        END IF;
    END
$$;

CALL erp_event.expect_version('1');

END;
