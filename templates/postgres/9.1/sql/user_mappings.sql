select u.rolname as user,
       s.srvname as server,
       m.umoptions as options,
       x.extname as extension
  from pg_catalog.pg_user_mapping m
  left outer join pg_catalog.pg_roles u
    on u.oid = m.umuser
 inner join pg_catalog.pg_foreign_server s
    on s.oid = m.umserver
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = m.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_user_mapping';
