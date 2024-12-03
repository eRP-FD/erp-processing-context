/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023, 2024
 * (C) Copyright IBM Corp. 2021, 2023, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

BEGIN;

CALL erp.expect_version('21');
CALL erp.set_version('22');


DROP SCHEMA IF EXISTS erp_event CASCADE;

--
-- Name: erp_event; Type: SCHEMA; Schema: -; Owner: -
--

CREATE SCHEMA erp_event AUTHORIZATION role_proc_admin;

GRANT USAGE ON SCHEMA erp_event TO role_proc_user;

--
-- Name: usecase_type; Type: TYPE; Schema: erp_event; Owner: -
--

CREATE TYPE erp_event.usecase_type AS ENUM ('providePrescription', 'cancelPrescription', 'provideDispensation', 'cancelDispensation');

--
-- Name: task_event; Type: TABLE; Schema: erp_event; Owner: -
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
    usecase erp_event.usecase_type NOT NULL,
    prescription_type smallint NOT NULL
);
ALTER TABLE erp_event.task_event ALTER COLUMN kvnr SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN salt SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN access_code SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN secret SET STORAGE PLAIN;
ALTER TABLE erp_event.task_event ALTER COLUMN performer SET STORAGE PLAIN;

-- Allow delete for processing context role on task_event
GRANT INSERT, SELECT, DELETE ON erp_event.task_event TO role_proc_user;

--
-- Name: task_event_id_seq; Type: SEQUENCE; Schema: erp_event; Owner: -
--

CREATE SEQUENCE erp_event.task_event_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;

--
-- Name: task_event_id_seq; Type: SEQUENCE OWNED BY; Schema: erp_event; Owner: -
--

ALTER SEQUENCE erp_event.task_event_id_seq OWNED BY erp_event.task_event.id;

-- Apply default permissions for sequences for processing context role
GRANT USAGE, SELECT, UPDATE ON ALL SEQUENCES IN SCHEMA erp_event TO role_proc_user;

--
-- Name: event_id_seq; Type: DEFAULT; Schema: erp_event; Owner: -
--

ALTER TABLE erp_event.task_event ALTER COLUMN id SET DEFAULT nextval('erp_event.task_event_id_seq'::regclass);

COMMIT;