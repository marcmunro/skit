-- List all non-empty, non-system schemata.
select n.nspname as name,
       s.rolname as owner,
       n.nspacl as privs,
       quote_literal(obj_description(n.oid, 'pg_namespace')) as comment
from   pg_catalog.pg_namespace n
       inner join pg_catalog.pg_authid s
               on s.oid = n.nspowner
where  n.nspname NOT IN ('pg_catalog', 'pg_toast', 
			 'information_schema')
and    n.nspname !~ '^pg_temp_'
order by s.rolname;
