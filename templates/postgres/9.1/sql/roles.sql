-- List all roles
select a.rolname as name,
       case when a.rolsuper then 'y' else 'n' end as superuser,
       case when a.rolinherit then 'y' else 'n' end as inherit,
       case when a.rolcreaterole then 'y' else 'n' end as createrole,
       case when a.rolcreatedb then 'y' else 'n' end as createdb,
       --case when a.rolcatupdate then 'y' else 'n' end as catalog,
       case when a.rolcanlogin then 'y' else 'n' end as login,
       a.rolconnlimit as max_connections,
       a.rolpassword as password,
       a.rolvaliduntil as expires,
       u.useconfig as config,
       quote_literal(shobj_description(a.oid, 'pg_authid')) as comment,
       x.extname as extension
  from pg_catalog.pg_authid a
  left outer join pg_catalog.pg_user u
    on u.usesysid = a.oid
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = 17131
   and dx.deptype = 'e'
   and cx.relname = 'pg_roles'
order by a.rolname;
