-- List all tables in non-system schemas.

select c.oid::oid as oid,
       dep.objid as rewrite_oid,  -- The oid of the pg_rewrite rule for
       	       	  		  -- this view will give us our dependencies
       n.nspname as schema,
       c.relname as name,
       r.rolname as owner,
       case when t.spcname is null then td.spcname
       else t.spcname end as tablespace, 
       t.spcname is null as tablespace_is_default,
       regexp_replace(pg_catalog.pg_get_viewdef(c.oid, true),
                      ';$', '') as viewdef,
       c.relacl::text as privs,
       c.relispopulated as is_populated,
       regexp_replace(c.reloptions::text, '{(.*)}', E'\\1') as options,
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
inner join pg_catalog.pg_depend dep
  on  dep.refobjid = c.oid
  and dep.refclassid = 'pg_class'::regclass
  and dep.classid = 'pg_rewrite'::regclass
  and dep.deptype = 'i'
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = c.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_class'
 where c.relkind = 'm'
   and n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
  order by 1, 2;
