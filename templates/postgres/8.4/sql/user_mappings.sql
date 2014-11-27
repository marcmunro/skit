select u.rolname as user,
       s.srvname as server,
       m.umoptions as options
  from pg_catalog.pg_user_mapping m
  left outer join pg_catalog.pg_roles u
    on u.oid = m.umuser
 inner join pg_catalog.pg_foreign_server s
    on s.oid = m.umserver;
