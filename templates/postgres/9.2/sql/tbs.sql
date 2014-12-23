-- List all tablespaces
select t.spcname as name,
       r.rolname as owner,
       pg_catalog.pg_tablespace_location(t.oid) as location,
       t.spcoptions as options,
       t.spcacl::text as privs,
       quote_literal(shobj_description(t.oid, 'pg_tablespace')) as comment
from   pg_tablespace t
    inner join pg_catalog.pg_roles r 
        on  r.oid = t.spcowner
where  t.spcname not in ('pg_global')
order by 1;

