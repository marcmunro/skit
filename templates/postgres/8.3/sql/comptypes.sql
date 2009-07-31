-- List all Composite types
select t.typname as name,
       n.nspname as schema,
       'composite' as type,
       o.rolname as owner,
       t.typrelid as reloid,
       quote_literal(obj_description(t.oid, 'pg_type')) as comment
from   pg_catalog.pg_type t
       inner join
          pg_catalog.pg_class c
       on (c.oid = t.typrelid)
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = t.typnamespace)
       inner join
          pg_catalog.pg_authid o
       on (o.oid = t.typowner)
where   t.typisdefined
  and   n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
  and   t.typtype = 'c'
  and   c.relkind = 'c'
order by n.nspname, t.typname;




