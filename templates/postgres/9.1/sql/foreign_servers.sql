select s.srvname as name,
       o.rolname as owner,
       w.fdwname as foreign_data_wrapper,
       s.srvtype as type,
       s.srvversion as version,
       s.srvacl as privs,
       s.srvoptions as options,
       x.extname as extension
  from pg_catalog.pg_foreign_server s
 inner join pg_catalog.pg_roles o
    on o.oid = s.srvowner
 inner join pg_catalog.pg_foreign_data_wrapper w
    on w.oid = s.srvfdw
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = s.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_foreign_server';
