select s.srvname as name,
       o.rolname as owner,
       w.fdwname as foreign_data_wrapper,
       s.srvtype as type,
       s.srvversion as version,
       s.srvacl as privs,
       s.srvoptions as options
  from pg_catalog.pg_foreign_server s
 inner join pg_catalog.pg_roles o
    on o.oid = s.srvowner
 inner join pg_catalog.pg_foreign_data_wrapper w
    on w.oid = s.srvfdw;
