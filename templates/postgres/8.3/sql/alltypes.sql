-- List all types by oid to get name and schema
-- Todo: add a basename field.  For array types, this will be the name
-- of the underlying object type, for others it will be the same as name.
select t.oid as oid,
       t.typname as name,
       n.nspname as schema
from   pg_catalog.pg_type t
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = t.typnamespace);




