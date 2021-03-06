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
       quote_literal(shobj_description(a.oid, 'pg_authid')) as comment
  from pg_catalog.pg_authid a
  left outer join pg_catalog.pg_user u
    on u.usesysid = a.oid
order by a.rolname;
