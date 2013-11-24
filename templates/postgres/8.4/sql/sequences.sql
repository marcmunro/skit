-- List all sequences in non-system schemas.

select c.oid::oid as oid,
       n.nspname as schema,
       c.relname as name,
       r.rolname as owner,
       t.spcname as tablespace,
       -- Explicitly show default privs if they are not given.
       -- Defaults are select, update and usage to owner.
       case when c.relacl is null 
       then '{' || r.rolname || '=rwU/' || r.rolname || '}'
       else c.relacl::text end as privs,
       quote_literal(obj_description(c.oid, 'pg_class')) as comment
from   pg_catalog.pg_class c  
    inner join pg_catalog.pg_roles r 
        on  r.oid = c.relowner
    inner join pg_catalog.pg_namespace n 
        on n.oid = c.relnamespace
    left outer join pg_catalog.pg_tablespace t
         on t.oid = c.reltablespace
where  c.relkind = 'S'
and    n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
order by 1, 2;
