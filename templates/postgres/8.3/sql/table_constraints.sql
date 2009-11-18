-- List all pk and uk constraints
select c.oid as oid,
       c.conname as name,
       n.nspname as schema,
       c.contype as constraint_type,
       c.condeferrable as deferrable,
       c.condeferred as deferred,
       regexp_replace(c.conkey::text, '{(.*)}', E'\\1') as columns,
       pg_catalog.pg_get_constraintdef(c.oid) as source
from   pg_catalog.pg_constraint c
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = c.connamespace)
where   c.conrelid = :1
order by c.contype, n.nspname, c.conname;




