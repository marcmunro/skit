-- List all non-empty, non-system schemata.
select n.nspname as name,
       s.rolname as owner,
       -- Explicitly show default privs if they are not given.
       -- Defaults are create and usage to owner.
       case when n.nspacl is null 
       then '{' || s.rolname || '=UC/' || s.rolname || '}'
       else n.nspacl::text end as privs,
       quote_literal(obj_description(n.oid, 'pg_namespace')) as comment
from   pg_catalog.pg_namespace n
       inner join pg_catalog.pg_authid s
               on s.oid = n.nspowner
where  n.nspname NOT IN ('pg_catalog', 'pg_toast', 
			 'information_schema')
and    n.nspname !~ '^pg_temp_'
and    n.nspname !~ '^pg_toast_'
order by s.rolname;
