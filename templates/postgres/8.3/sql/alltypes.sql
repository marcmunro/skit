-- List all types by oid to get name and schema for use in dependencies.
-- If the type is an array type, the dependency will on the underlying
-- type, not the array type itself, so in this case we convert the name
-- to that of the base type for the array.

select t.oid as oid,
       coalesce(tb.typname, t.typname) as name,
       coalesce(nb.nspname, n.nspname) as schema
from   pg_catalog.pg_type t
inner join 
       pg_catalog.pg_namespace n
    on n.oid = t.typnamespace
left outer join (
           pg_catalog.pg_type tb
    inner join 
           pg_catalog.pg_namespace nb
        on nb.oid = tb.typnamespace)
    on tb.typarray = t.oid
where t.typname like '%threestr';




