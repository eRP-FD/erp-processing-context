/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

CREATE OR REPLACE PROCEDURE pg_temp.create_task_partitions(table_name regclass)
    LANGUAGE plpgsql AS $$
    DECLARE part integer;
    DECLARE partition_name text;
    DECLARE create_part_query text;
    BEGIN
        FOR part in 0..59 LOOP
            partition_name = table_name || lpad(((part + 1) * 5000000)::text, 12, '0');

            create_part_query = 'CREATE TABLE IF NOT EXISTS ' || partition_name
                    || ' PARTITION OF ' || table_name
                    || ' FOR VALUES FROM (' || (part * 5000000) || ') TO (' || ( (part + 1) * 5000000) || ');';
            EXECUTE create_part_query;
        END LOOP;
    END
$$;

CREATE OR REPLACE PROCEDURE pg_temp.set_task_partitions_permissions(table_name regclass)
    LANGUAGE plpgsql AS $$
    DECLARE partition_name text;
    BEGIN
        FOR part in 0..59 LOOP
            partition_name = table_name || lpad(((part + 1) * 5000000)::text, 12, '0');
            EXECUTE 'REVOKE ALL ON ' || partition_name || ' from role_proc_user';
            EXECUTE 'ALTER TABLE ' || partition_name || ' owner to role_proc_admin';
            EXECUTE 'GRANT INSERT, SELECT, UPDATE ON ' || partition_name || ' to role_proc_user';
        END LOOP;
    END
$$;

CREATE OR REPLACE FUNCTION pg_temp.create_partitions(
    tablename  text,
    start_date date,
    end_date   date
)
RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE
    current    DATE := start_date;
    next_month DATE;
    part_name TEXT;
BEGIN
    WHILE current < end_date LOOP
        next_month := current + interval '1 month';
        part_name := tablename || to_char(current, 'YYYYMM');

        EXECUTE format($stmt$
            CREATE TABLE erp.%I
            PARTITION OF erp.%I
            FOR VALUES FROM (
                erp.gen_suuid_low('%s 00:00:00+01')
            ) TO (
                erp.gen_suuid_low('%s 00:00:00+01')
            )
        $stmt$, part_name, tablename, current, next_month);

        current := next_month;
    END LOOP;
END;
$$;
