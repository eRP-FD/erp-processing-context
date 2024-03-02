/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */


BEGIN;

CALL erp.expect_version('2');
CALL erp.set_version('3');

\ir create_partitions_func.sql

ALTER TABLE erp.auditevent ADD COLUMN IF NOT EXISTS prescription_type smallint DEFAULT 160;


ALTER TABLE erp.communication ADD COLUMN IF NOT EXISTS prescription_type smallint NOT NULL DEFAULT 160;


--
-- Name: task_169; Type: TABLE; Schema: erp; Owner: -
--

CREATE TABLE IF NOT EXISTS erp.task_169 (
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
    medication_dispense_bundle bytea
)
PARTITION BY RANGE (prescription_id);
ALTER TABLE erp.task_169 ALTER COLUMN kvnr SET STORAGE PLAIN;
ALTER TABLE erp.task_169 ALTER COLUMN kvnr_hashed SET STORAGE PLAIN;
ALTER TABLE erp.task_169 ALTER COLUMN salt SET STORAGE PLAIN;
ALTER TABLE erp.task_169 ALTER COLUMN access_code SET STORAGE PLAIN;
ALTER TABLE erp.task_169 ALTER COLUMN secret SET STORAGE PLAIN;
ALTER TABLE erp.task_169 ALTER COLUMN performer SET STORAGE PLAIN;

CALL pg_temp.create_task_partitions('erp.task_169');

--
-- Name: task_169_taskid_seq; Type: SEQUENCE; Schema: erp; Owner: -
--

CREATE SEQUENCE IF NOT EXISTS erp.task_169_taskid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: task_169_taskid_seq; Type: SEQUENCE OWNED BY; Schema: erp; Owner: -
--

ALTER SEQUENCE erp.task_169_taskid_seq OWNED BY erp.task_169.prescription_id;


--
-- Name: task_169 prescription_id; Type: DEFAULT; Schema: erp; Owner: -
--

ALTER TABLE erp.task_169 ALTER COLUMN prescription_id SET DEFAULT nextval('erp.task_169_taskid_seq'::regclass);


--
-- Name: task_169 task_169_pkey; Type: CONSTRAINT; Schema: erp; Owner: -
--
ALTER TABLE erp.task_169 DROP CONSTRAINT IF EXISTS task_169_pkey;
ALTER TABLE erp.task_169
    ADD CONSTRAINT task_169_pkey PRIMARY KEY (prescription_id);

-- create foreign key constraints for table task
ALTER TABLE erp.task_169 DROP CONSTRAINT IF EXISTS fk_task_169_key_blob_id;
ALTER TABLE erp.task_169 DROP CONSTRAINT IF EXISTS fk_medication_dispense_169_blob_id;
ALTER TABLE erp.task_169 ADD CONSTRAINT fk_task_169_key_blob_id FOREIGN KEY (task_key_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;
ALTER TABLE erp.task_169 ADD CONSTRAINT fk_medication_dispense_169_blob_id FOREIGN KEY (medication_dispense_blob_id) REFERENCES erp.blob (blob_id) ON DELETE RESTRICT;



--
-- Name: auditevent_prescription_type_idx; Type: INDEX; Schema: erp; Owner: -
--

CREATE INDEX IF NOT EXISTS auditevent_prescription_type_idx ON erp.auditevent USING btree (prescription_type);


--
-- Name: communication_prescription_type_idx; Type: INDEX; Schema: erp; Owner: -
--

CREATE INDEX IF NOT EXISTS communication_prescription_type_idx ON erp.communication USING btree (prescription_type);


--
-- Name: task_169_kvnr_idx; Type: INDEX; Schema: erp; Owner: -
--

CREATE INDEX IF NOT EXISTS task_169_kvnr_idx ON erp.task_169 USING hash (kvnr_hashed);


--
-- Name: task_169_last_modified_idx; Type: INDEX; Schema: erp; Owner: -
--

CREATE INDEX IF NOT EXISTS task_169_last_modified_idx ON erp.task_169 USING btree (last_modified);


-- Updating tasks is required as the task progresses
GRANT UPDATE ON TABLE erp.task_169 TO role_proc_user;

COMMIT;
