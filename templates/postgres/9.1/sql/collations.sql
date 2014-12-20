select c.oid as oid,
       collname as name,
       n.nspname as schema,
       o.rolname as owner,
       c.collencoding as encoding,
       c.collcollate as lc_collate,
       c.collctype as lc_ctype,
       quote_literal(obj_description(c.oid, 'pg_collation')) as comment
  from pg_catalog.pg_collation c
 inner join pg_catalog.pg_namespace n
    on n.oid = c.collnamespace
 inner join pg_catalog.pg_roles o
    on o.oid = c.collowner
 where n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema');