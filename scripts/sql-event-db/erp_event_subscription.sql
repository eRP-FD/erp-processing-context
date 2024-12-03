/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

-- You may call this SQL script from the command prompt by invoking:
-- psql -U <UserName> -h <HostName> -d <DBName> -f <PathToScript> -v connection='hostname=<hostnameMainDB> port=<portMainDB> user=<userMainDB> password=<passwordMainDB> sslmode=require'
-- e.g.:  psql -U erp_admin -h localhost -p 5433 -d erp_event -f scripts/sql-event-db/erp_event_subscription.sql -v connection='dbname=erp_processing_context host=host.containers.internal port=5432 user=erp_admin password=local_TEST sslmode=require'

DROP SUBSCRIPTION IF EXISTS task_event_sub;
BEGIN;

CREATE SUBSCRIPTION task_event_sub
    CONNECTION :'connection'
    PUBLICATION task_event_pub
    WITH(create_slot = false, slot_name = 'task_event_slot', copy_data = false);

COMMIT;
