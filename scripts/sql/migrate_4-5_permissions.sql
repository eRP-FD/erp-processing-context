-- to run as postgres

-- related to: migrate_0-1_db_schema_versioning.sql

begin;
CALL erp.expect_version('4');
CALL erp.set_version('5');

\ir create_partitions_func.sql

revoke all on erp.config from role_proc_user;
alter table erp.config owner to role_proc_admin;
grant SELECT, INSERT, DELETE, UPDATE on erp.config to role_proc_admin;
grant SELECT on erp.config to role_proc_maintenance ;
grant SELECT on erp.config to role_proc_user ;

grant execute on procedure erp.expect_version(text) to role_proc_admin, role_proc_user;
grant execute on procedure erp.set_version(text)    to role_proc_admin;

-- related to: migrate_2-3_workflow_169.sql
-- Grants für erp.task_169 (und partitions) einrichten:
-- gleiche Rechte wie erp.task
revoke all on  erp.task_169 from role_proc_user;
alter table erp.task_169 owner to role_proc_admin;
grant insert, select, update on erp.task_169 to role_proc_user;

CALL pg_temp.set_task_partitions_permissions('erp.task_169');

-- Grants für erp.task_169_taskid_seq einrichten:
-- gleiche Rechte wie erp.task_taskid_seq
alter sequence erp.task_169_taskid_seq owner to role_proc_admin;
grant ALL ON SEQUENCE erp.task_169_taskid_seq TO role_proc_user;



end;


