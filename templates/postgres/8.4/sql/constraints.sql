-- List all constraints
-- start with check constraints only - these are needed for domains
select c.oid as oid,
       c.conname as name,
       n.nspname as schema,
       c.contype as constraint_type,
       c.conislocal as is_local,
       pg_catalog.pg_get_constraintdef(c.oid) as source
from   pg_catalog.pg_constraint c
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = c.connamespace)
where   c.contype = 'c'
  and   n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
order by n.nspname, c.conname;




