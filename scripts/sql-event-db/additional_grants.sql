/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

-- schema permissions
GRANT USAGE ON SCHEMA erp_event TO role_proc_user;
GRANT ALL ON SCHEMA erp_event TO role_proc_maintenance;

-- default privileges on tables for users - applies to all newly created tables
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp_event GRANT SELECT, INSERT ON TABLES TO role_proc_user;
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp_event GRANT SELECT, INSERT, UPDATE, DELETE, TRUNCATE ON TABLES TO role_proc_maintenance;

-- default privileges on sequences for users - applies to all newly created sequences
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp_event GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO role_proc_user;
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp_event GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO role_proc_maintenance;

-- role_proc_user permissions
-- Apply default permissions of INSERT and SELECT for processing context role
GRANT SELECT,INSERT ON ALL TABLES IN SCHEMA erp_event TO role_proc_user;

-- Apply default permissions for sequences for processing context role
GRANT USAGE, SELECT, UPDATE ON ALL SEQUENCES IN SCHEMA erp_event TO role_proc_user;


-- Updating kvnr is required as the task progresses
GRANT UPDATE ON TABLE erp_event.kvnr TO role_proc_user;

-- Updating kvnr is required as the task progresses
GRANT UPDATE, DELETE ON TABLE erp_event.task_event TO role_proc_user;

-- Do not allow the use of public schema
REVOKE ALL ON SCHEMA public FROM PUBLIC;
