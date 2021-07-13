-- schema permissions
GRANT USAGE ON SCHEMA erp TO role_proc_user;
GRANT ALL ON SCHEMA erp TO role_proc_maintenance;
GRANT USAGE ON SCHEMA erp TO role_proc_readonly;

-- default privileges on tables for users - applies to all newly created tables
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp GRANT SELECT, INSERT ON TABLES TO role_proc_user;
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp GRANT SELECT, INSERT, UPDATE, DELETE, TRUNCATE ON TABLES TO role_proc_maintenance;
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp GRANT SELECT ON TABLES TO role_proc_readonly;

-- default privileges on sequences for users - applies to all newly created sequences
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO role_proc_user;
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO role_proc_maintenance;
ALTER DEFAULT PRIVILEGES FOR ROLE role_proc_admin IN SCHEMA erp GRANT USAGE, SELECT ON SEQUENCES TO role_proc_readonly;

-- role_proc_user permissions
-- Apply default permissions of INSERT and SELECT for processing context role
GRANT SELECT,INSERT ON ALL TABLES IN SCHEMA erp TO role_proc_user;

-- Apply default permissions for sequences for processing context role
GRANT USAGE, SELECT, UPDATE ON ALL SEQUENCES IN SCHEMA erp TO role_proc_user;

-- Allow update for VAU cmac
GRANT UPDATE ON erp.vau_cmac TO role_proc_user;

-- Blobs may be updated and deleted by the processing context
GRANT UPDATE, DELETE ON erp.blob TO role_proc_user;

-- Updating tasks is required as the task progresses
GRANT UPDATE ON TABLE erp.task TO role_proc_user;

-- Communications may be deleted by users, processing context needs delete permissions for this
GRANT UPDATE, DELETE ON TABLE erp.communication TO role_proc_user;
