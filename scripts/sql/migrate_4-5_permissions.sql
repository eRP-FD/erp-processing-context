-- to run as postgres

-- related to: migrate_0-1_db_schema_versioning.sql

begin;
CALL erp.expect_version('4');
CALL erp.set_version('5');

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

alter table erp.task_169000005000000 owner to role_proc_admin;
alter table erp.task_169000010000000 owner to role_proc_admin;
alter table erp.task_169000015000000 owner to role_proc_admin;
alter table erp.task_169000020000000 owner to role_proc_admin;
alter table erp.task_169000025000000 owner to role_proc_admin;
alter table erp.task_169000030000000 owner to role_proc_admin;
alter table erp.task_169000035000000 owner to role_proc_admin;
alter table erp.task_169000040000000 owner to role_proc_admin;
alter table erp.task_169000045000000 owner to role_proc_admin;
alter table erp.task_169000050000000 owner to role_proc_admin;
alter table erp.task_169000055000000 owner to role_proc_admin;
alter table erp.task_169000060000000 owner to role_proc_admin;
alter table erp.task_169000065000000 owner to role_proc_admin;
alter table erp.task_169000070000000 owner to role_proc_admin;
alter table erp.task_169000075000000 owner to role_proc_admin;
alter table erp.task_169000080000000 owner to role_proc_admin;
alter table erp.task_169000085000000 owner to role_proc_admin;
alter table erp.task_169000090000000 owner to role_proc_admin;
alter table erp.task_169000095000000 owner to role_proc_admin;

alter table erp.task_169000100000000 owner to role_proc_admin;
alter table erp.task_169000105000000 owner to role_proc_admin;
alter table erp.task_169000110000000 owner to role_proc_admin;
alter table erp.task_169000115000000 owner to role_proc_admin;
alter table erp.task_169000120000000 owner to role_proc_admin;
alter table erp.task_169000125000000 owner to role_proc_admin;
alter table erp.task_169000130000000 owner to role_proc_admin;
alter table erp.task_169000135000000 owner to role_proc_admin;
alter table erp.task_169000140000000 owner to role_proc_admin;
alter table erp.task_169000145000000 owner to role_proc_admin;
alter table erp.task_169000150000000 owner to role_proc_admin;
alter table erp.task_169000155000000 owner to role_proc_admin;
alter table erp.task_169000160000000 owner to role_proc_admin;
alter table erp.task_169000165000000 owner to role_proc_admin;
alter table erp.task_169000170000000 owner to role_proc_admin;
alter table erp.task_169000175000000 owner to role_proc_admin;
alter table erp.task_169000180000000 owner to role_proc_admin;
alter table erp.task_169000185000000 owner to role_proc_admin;
alter table erp.task_169000190000000 owner to role_proc_admin;
alter table erp.task_169000195000000 owner to role_proc_admin;


alter table erp.task_169000200000000 owner to role_proc_admin;
alter table erp.task_169000205000000 owner to role_proc_admin;
alter table erp.task_169000210000000 owner to role_proc_admin;
alter table erp.task_169000215000000 owner to role_proc_admin;
alter table erp.task_169000220000000 owner to role_proc_admin;
alter table erp.task_169000225000000 owner to role_proc_admin;
alter table erp.task_169000230000000 owner to role_proc_admin;
alter table erp.task_169000235000000 owner to role_proc_admin;
alter table erp.task_169000240000000 owner to role_proc_admin;
alter table erp.task_169000245000000 owner to role_proc_admin;
alter table erp.task_169000250000000 owner to role_proc_admin;
alter table erp.task_169000255000000 owner to role_proc_admin;
alter table erp.task_169000260000000 owner to role_proc_admin;
alter table erp.task_169000265000000 owner to role_proc_admin;
alter table erp.task_169000270000000 owner to role_proc_admin;
alter table erp.task_169000275000000 owner to role_proc_admin;
alter table erp.task_169000280000000 owner to role_proc_admin;
alter table erp.task_169000285000000 owner to role_proc_admin;
alter table erp.task_169000290000000 owner to role_proc_admin;
alter table erp.task_169000295000000 owner to role_proc_admin;
alter table erp.task_169000300000000 owner to role_proc_admin;



revoke all on erp.task_169000005000000  from role_proc_user;
revoke all on erp.task_169000010000000  from role_proc_user;
revoke all on erp.task_169000015000000  from role_proc_user;
revoke all on erp.task_169000020000000  from role_proc_user;
revoke all on erp.task_169000025000000  from role_proc_user;
revoke all on erp.task_169000030000000  from role_proc_user;
revoke all on erp.task_169000035000000  from role_proc_user;
revoke all on erp.task_169000040000000  from role_proc_user;
revoke all on erp.task_169000045000000  from role_proc_user;
revoke all on erp.task_169000050000000  from role_proc_user;
revoke all on erp.task_169000055000000  from role_proc_user;
revoke all on erp.task_169000060000000  from role_proc_user;
revoke all on erp.task_169000065000000  from role_proc_user;
revoke all on erp.task_169000070000000  from role_proc_user;
revoke all on erp.task_169000075000000  from role_proc_user;
revoke all on erp.task_169000080000000  from role_proc_user;
revoke all on erp.task_169000085000000  from role_proc_user;
revoke all on erp.task_169000090000000  from role_proc_user;
revoke all on erp.task_169000095000000  from role_proc_user;

revoke all on erp.task_169000100000000  from role_proc_user;
revoke all on erp.task_169000105000000  from role_proc_user;
revoke all on erp.task_169000110000000  from role_proc_user;
revoke all on erp.task_169000115000000  from role_proc_user;
revoke all on erp.task_169000120000000  from role_proc_user;
revoke all on erp.task_169000125000000  from role_proc_user;
revoke all on erp.task_169000130000000  from role_proc_user;
revoke all on erp.task_169000135000000  from role_proc_user;
revoke all on erp.task_169000140000000  from role_proc_user;
revoke all on erp.task_169000145000000  from role_proc_user;
revoke all on erp.task_169000150000000  from role_proc_user;
revoke all on erp.task_169000155000000  from role_proc_user;
revoke all on erp.task_169000160000000  from role_proc_user;
revoke all on erp.task_169000165000000  from role_proc_user;
revoke all on erp.task_169000170000000  from role_proc_user;
revoke all on erp.task_169000175000000  from role_proc_user;
revoke all on erp.task_169000180000000  from role_proc_user;
revoke all on erp.task_169000185000000  from role_proc_user;
revoke all on erp.task_169000190000000  from role_proc_user;
revoke all on erp.task_169000195000000  from role_proc_user;


revoke all on erp.task_169000200000000  from role_proc_user;
revoke all on erp.task_169000205000000  from role_proc_user;
revoke all on erp.task_169000210000000  from role_proc_user;
revoke all on erp.task_169000215000000  from role_proc_user;
revoke all on erp.task_169000220000000  from role_proc_user;
revoke all on erp.task_169000225000000  from role_proc_user;
revoke all on erp.task_169000230000000  from role_proc_user;
revoke all on erp.task_169000235000000  from role_proc_user;
revoke all on erp.task_169000240000000  from role_proc_user;
revoke all on erp.task_169000245000000  from role_proc_user;
revoke all on erp.task_169000250000000  from role_proc_user;
revoke all on erp.task_169000255000000  from role_proc_user;
revoke all on erp.task_169000260000000  from role_proc_user;
revoke all on erp.task_169000265000000  from role_proc_user;
revoke all on erp.task_169000270000000  from role_proc_user;
revoke all on erp.task_169000275000000  from role_proc_user;
revoke all on erp.task_169000280000000  from role_proc_user;
revoke all on erp.task_169000285000000  from role_proc_user;
revoke all on erp.task_169000290000000  from role_proc_user;
revoke all on erp.task_169000295000000  from role_proc_user;
revoke all on erp.task_169000300000000  from role_proc_user;



grant insert, select, update on erp.task_169000005000000  to role_proc_user;
grant insert, select, update on erp.task_169000010000000  to role_proc_user;
grant insert, select, update on erp.task_169000015000000  to role_proc_user;
grant insert, select, update on erp.task_169000020000000  to role_proc_user;
grant insert, select, update on erp.task_169000025000000  to role_proc_user;
grant insert, select, update on erp.task_169000030000000  to role_proc_user;
grant insert, select, update on erp.task_169000035000000  to role_proc_user;
grant insert, select, update on erp.task_169000040000000  to role_proc_user;
grant insert, select, update on erp.task_169000045000000  to role_proc_user;
grant insert, select, update on erp.task_169000050000000  to role_proc_user;
grant insert, select, update on erp.task_169000055000000  to role_proc_user;
grant insert, select, update on erp.task_169000060000000  to role_proc_user;
grant insert, select, update on erp.task_169000065000000  to role_proc_user;
grant insert, select, update on erp.task_169000070000000  to role_proc_user;
grant insert, select, update on erp.task_169000075000000  to role_proc_user;
grant insert, select, update on erp.task_169000080000000  to role_proc_user;
grant insert, select, update on erp.task_169000085000000  to role_proc_user;
grant insert, select, update on erp.task_169000090000000  to role_proc_user;
grant insert, select, update on erp.task_169000095000000  to role_proc_user;

grant insert, select, update on erp.task_169000100000000  to role_proc_user;
grant insert, select, update on erp.task_169000105000000  to role_proc_user;
grant insert, select, update on erp.task_169000110000000  to role_proc_user;
grant insert, select, update on erp.task_169000115000000  to role_proc_user;
grant insert, select, update on erp.task_169000120000000  to role_proc_user;
grant insert, select, update on erp.task_169000125000000  to role_proc_user;
grant insert, select, update on erp.task_169000130000000  to role_proc_user;
grant insert, select, update on erp.task_169000135000000  to role_proc_user;
grant insert, select, update on erp.task_169000140000000  to role_proc_user;
grant insert, select, update on erp.task_169000145000000  to role_proc_user;
grant insert, select, update on erp.task_169000150000000  to role_proc_user;
grant insert, select, update on erp.task_169000155000000  to role_proc_user;
grant insert, select, update on erp.task_169000160000000  to role_proc_user;
grant insert, select, update on erp.task_169000165000000  to role_proc_user;
grant insert, select, update on erp.task_169000170000000  to role_proc_user;
grant insert, select, update on erp.task_169000175000000  to role_proc_user;
grant insert, select, update on erp.task_169000180000000  to role_proc_user;
grant insert, select, update on erp.task_169000185000000  to role_proc_user;
grant insert, select, update on erp.task_169000190000000  to role_proc_user;
grant insert, select, update on erp.task_169000195000000  to role_proc_user;


grant insert, select, update on erp.task_169000200000000  to role_proc_user;
grant insert, select, update on erp.task_169000205000000  to role_proc_user;
grant insert, select, update on erp.task_169000210000000  to role_proc_user;
grant insert, select, update on erp.task_169000215000000  to role_proc_user;
grant insert, select, update on erp.task_169000220000000  to role_proc_user;
grant insert, select, update on erp.task_169000225000000  to role_proc_user;
grant insert, select, update on erp.task_169000230000000  to role_proc_user;
grant insert, select, update on erp.task_169000235000000  to role_proc_user;
grant insert, select, update on erp.task_169000240000000  to role_proc_user;
grant insert, select, update on erp.task_169000245000000  to role_proc_user;
grant insert, select, update on erp.task_169000250000000  to role_proc_user;
grant insert, select, update on erp.task_169000255000000  to role_proc_user;
grant insert, select, update on erp.task_169000260000000  to role_proc_user;
grant insert, select, update on erp.task_169000265000000  to role_proc_user;
grant insert, select, update on erp.task_169000270000000  to role_proc_user;
grant insert, select, update on erp.task_169000275000000  to role_proc_user;
grant insert, select, update on erp.task_169000280000000  to role_proc_user;
grant insert, select, update on erp.task_169000285000000  to role_proc_user;
grant insert, select, update on erp.task_169000290000000  to role_proc_user;
grant insert, select, update on erp.task_169000295000000  to role_proc_user;
grant insert, select, update on erp.task_169000300000000  to role_proc_user;




-- Grants für erp.task_169_taskid_seq einrichten:
-- gleiche Rechte wie erp.task_taskid_seq
alter sequence erp.task_169_taskid_seq owner to role_proc_admin;
grant ALL ON SEQUENCE erp.task_169_taskid_seq TO role_proc_user;



end;


