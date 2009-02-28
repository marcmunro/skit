-- List all tablespaces
select t.spcname as name,
       r.rolname as owner,
       t.spclocation as location,
       t.spcacl as privs
from   pg_tablespace t
    inner join pg_catalog.pg_roles r 
        on  r.oid = t.spcowner
where  t.spcname not in ('pg_default', 'pg_global')
order by 1;
