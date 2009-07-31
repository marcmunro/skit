-- List all types by oid to get name and schema
select t.oid as oid,
       t.typname as name,
       n.nspname as schema
from   pg_catalog.pg_type t
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = t.typnamespace);




