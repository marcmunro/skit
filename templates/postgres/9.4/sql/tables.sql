-- List all tables in non-system schemas.

select c.oid::oid as oid,
       n.nspname as schema,
       c.relname as name,
       r.rolname as owner,
       case when c.relhasoids then 't' else null end as with_oids,
       case when t.spcname is null then td.spcname
       else t.spcname end as tablespace, 
       t.spcname is null as tablespace_is_default,
       c.relacl::text as privs,
       regexp_replace(c.reloptions::text, '{(.*)}', E'\\1') as options,
       case when c.relpersistence = 'u' then true else null end as is_unlogged,
       c.relreplident as replica_ident,
       ic.relname as replica_index,
       c.relkind = 'f' as is_foreign,
       fs.srvname as foreign_server_name,
       ft.ftoptions as foreign_table_options,
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
  left outer join (pg_catalog.pg_index i
    inner join pg_catalog.pg_class ic
       on ic.oid = i.indexrelid)
    on i.indrelid = c.oid
   and i.indisreplident
   and c.relreplident = 'i'
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = c.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_class'
  left outer join (
            pg_catalog.pg_foreign_table ft
      inner join pg_catalog.pg_foreign_server fs
         on fs.oid = ft.ftserver)
    on ft.ftrelid = c.oid
 where c.relkind in ('r', 'f')
   and n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
  order by 1, 2;
