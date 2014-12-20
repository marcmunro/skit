select c.oid as oid,
       collname as name,
       n.nspname as schema,
       o.rolname as owner,
       c.collencoding as encoding,
       c.collcollate as lc_collate,
       c.collctype as lc_ctype,
       quote_literal(obj_description(c.oid, 'pg_collation')) as comment,
       x.extname as extension
  from pg_catalog.pg_collation c
 inner join pg_catalog.pg_namespace n
    on n.oid = c.collnamespace
 inner join pg_catalog.pg_roles o
    on o.oid = c.collowner
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = c.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_collation'
 where n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema');