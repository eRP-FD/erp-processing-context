/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

-- You may call this SQL script from the command prompt by invoking:
--   psql -U <UserName> -h <HostName> -d <DBName> -f <PathToScript>
--   e.g.: psql -U erp_admin -h localhost -d erp -f scripts/sql-event-db/erp_event.sql


SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

DROP SCHEMA IF EXISTS erp_event;

--
-- Name: erp_event; Type: SCHEMA; Schema: -; Owner: -
--

CREATE SCHEMA erp_event AUTHORIZATION role_proc_admin;

--
-- Name: plpython3u; Type: EXTENSION; Schema: -; Owner: -
--

CREATE EXTENSION IF NOT EXISTS plpython3u WITH SCHEMA pg_catalog;


--
-- Name: EXTENSION plpython3u; Type: COMMENT; Schema: -; Owner: -
--

COMMENT ON EXTENSION plpython3u IS 'PL/Python3U untrusted procedural language';


SET default_tablespace = '';

SET ROLE role_proc_admin;

SET default_table_access_method = heap;

--
-- Name: task_event_state_type; Type: TYPE; Schema: erp_event; Owner: -
--

CREATE TYPE erp_event.task_event_state_type AS ENUM ('pending', 'deadLetterQueue');

--
-- Name: usecase_type; Type: TYPE; Schema: erp_event; Owner: -
--

CREATE TYPE erp_event.usecase_type AS ENUM ('providePrescription', 'cancelPrescription', 'provideDispensation', 'cancelDispensation');

--
-- Name: event; Type: TABLE; Schema: erp_event; Owner: -
--

CREATE TABLE erp_event.task_event (
    id  bigint NOT NULL,
    prescription_id bigint NOT NULL,
    kvnr bytea,
    kvnr_hashed bytea,
    last_modified timestamp with time zone DEFAULT CURRENT_TIMESTAMP::timestamp with time zone NOT NULL,
    authored_on timestamp with time zone DEFAULT CURRENT_TIMESTAMP::timestamp with time zone NOT NULL,
    expiry_date date,
    accept_date date,
    status smallint NOT NULL,
    task_key_blob_id integer,
    salt bytea,
    access_code bytea,
    secret bytea,
    healthcare_provider_prescription bytea,
    receipt bytea,
    when_handed_over timestamp with time zone,
    when_prepared timestamp with time zone,
    performer bytea,
    medication_dispense_blob_id integer,
    medication_dispense_bundle bytea,
    last_medication_dispense timestamp with time zone,
    usecase erp_event.usecase_type,
    prescription_type smallint,
    state erp_event.task_event_state_type default 'pending'
);


ALTER TABLE erp_event.task_event ALTER COLUMN kvnr SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN salt SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN access_code SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN secret SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN performer SET STORAGE PLAIN;

--
-- Name: kvnr_state_type; Type: TYPE; Schema: erp_event; Owner: -
--

CREATE TYPE erp_event.kvnr_state_type AS ENUM ('pending', 'processing', 'processed');


--
-- Name: erp_event.kvnr; Type: TABLE; Schema: erp_event; Owner: -
--

CREATE TABLE erp_event.kvnr (
    kvnr_hashed bytea NOT NULL,
    next_export timestamp with time zone DEFAULT CURRENT_TIMESTAMP::timestamp with time zone NOT NULL,
    last_export timestamp with time zone,
    state erp_event.kvnr_state_type NOT NULL,
    last_consent_check timestamp with time zone,
    assigned_epa varchar,
	primary key(kvnr_hashed)
);

ALTER TABLE erp_event.kvnr ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;

--
-- Name: event_kvnr_idx; Type: INDEX; Schema: erp_event; Owner: -
--

CREATE INDEX event_kvnr_idx ON erp_event.task_event USING hash (kvnr_hashed);


--
-- Name: kvnr_next_export_idx; Type: INDEX; Schema: erp_event; Owner: -
--

CREATE INDEX kvnr_next_export_idx ON erp_event.kvnr USING btree (next_export);


DROP TRIGGER IF EXISTS tg_insert_update_kvnr ON erp_event.task_event;


BEGIN;
--
-- Name: f_insert_update_kvnr; Type: FUNCTION: Schema: erp_event; Owner: -
--

CREATE OR REPLACE FUNCTION erp_event.f_insert_update_kvnr()
    RETURNS trigger
    LANGUAGE plpgsql
    AS
    $$
        BEGIN
        INSERT INTO erp_event.kvnr(kvnr_hashed, next_export, state)
            VALUES (NEW.kvnr_hashed, NOW(), 'pending')
            ON CONFLICT (kvnr_hashed) DO UPDATE
            SET state = 'pending', next_export = 
                CASE WHEN erp_event.kvnr.state = 'processed'
                    THEN NOW()
                    ELSE erp_event.kvnr.next_export
                END
            WHERE erp_event.kvnr.state <> 'pending';
        RETURN NEW;
        END; 
    $$;

--
-- Name: tg_insert_update_kvnr; Type: TRIGGER: Schema: erp_event; Owner: -
--

CREATE TRIGGER tg_insert_update_kvnr
    AFTER INSERT ON erp_event.task_event
    FOR EACH ROW
    EXECUTE FUNCTION erp_event.f_insert_update_kvnr();

ALTER TABLE erp_event.task_event ENABLE REPLICA TRIGGER tg_insert_update_kvnr;

COMMIT;