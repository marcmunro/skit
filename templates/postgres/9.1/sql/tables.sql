-- List all tables in non-system schemas.

select c.oid::oid as oid,
       n.nspname as schema,
       c.relname as name,
       r.rolname as owner,
       case when t.spcname is null then td.spcname
       else t.spcname end as tablespace, 
       c.relacl::text as privs,
       quote_literal(obj_description(c.oid, 'pg_class')) as comment,
       x.extname as extension
  from pg_catalog.pg_class c
 inner join pg_catalog.pg_database d
    on d.datname = current_database()
 inner join pg_catalog.pg_roles r 
    on r.oid = c.relowner
 inner join pg_catalog.pg_namespace n 
    on n.oid = c.relnamespace
 inner join pg_catalog.pg_tablespace td
    on td.oid = d.dattablespace
  left outer join pg_catalog.pg_tablespace t
    on t.oid = c.reltablespace
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = c.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_class'
 where c.relkind = 'r'
   and n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
  order by 1, 2;
