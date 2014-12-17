-- List all Composite types
select t.typname as name,
       n.nspname as schema,
       'composite' as type,
       o.rolname as owner,
       t.typrelid as reloid,
       x.extname as extension,
       quote_literal(obj_description(t.oid, 'pg_type')) as comment
  from pg_catalog.pg_type t
 inner join pg_catalog.pg_class c
    on c.oid = t.typrelid
 inner join pg_catalog.pg_namespace n
    on n.oid = t.typnamespace
 inner join pg_catalog.pg_authid o
    on o.oid = t.typowner
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = t.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_type'
 where t.typisdefined
   and n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
   and t.typtype = 'c'
   and c.relkind = 'c'
 order by n.nspname, t.typname;




