-- List all sequences in non-system schemas.

select c.oid::oid as oid,
       n.nspname as schema,
       c.relname as name,
       r.rolname as owner,
       t.spcname as tablespace,
       c.relacl::text as privs,
       n2.nspname as owned_by_schema,
       c2.relname as owned_by_table,
       a.attname as owned_by_column,
       quote_literal(obj_description(c.oid, 'pg_class')) as comment
  from pg_catalog.pg_class c  
 inner join pg_catalog.pg_roles r 
    on r.oid = c.relowner
 inner join pg_catalog.pg_namespace n 
    on n.oid = c.relnamespace
  left outer join pg_catalog.pg_tablespace t
    on t.oid = c.reltablespace
  left outer join 
       (
            pg_catalog.pg_depend d 
      inner join pg_catalog.pg_class c2 
         on c2.oid = d.refobjid
      inner join pg_catalog.pg_attribute a 
         on a.attrelid = c2.oid
        and a.attnum = d.refobjsubid
      inner join pg_catalog.pg_namespace n2 
         on n2.oid = c2.relnamespace
       )
    on d.objid = c.oid
   and d.deptype = 'a'
 where c.relkind = 'S'
   and n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
 order by 1, 2;

