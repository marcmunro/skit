-- List all tablespaces
select t.spcname as name,
       r.rolname as owner,
       t.spclocation as location,
     -- Explicitly show default privs if they are not given.
       -- Defaults are create to owner.
       case when t.spcacl is null 
       then '{' || r.rolname || '=C/' || r.rolname || '}'
       else t.spcacl::text end as privs,
       quote_literal(shobj_description(t.oid, 'pg_tablespace')) as comment
from   pg_tablespace t
    inner join pg_catalog.pg_roles r 
        on  r.oid = t.spcowner
where  t.spcname not in ('pg_global')
order by 1;

