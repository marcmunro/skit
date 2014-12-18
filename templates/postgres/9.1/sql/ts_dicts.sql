-- Get information about dictionaries.
select d.dictname as name, 
       ds.nspname as schema,
       o.rolname as owner, 
       t.tmplname as template_name,
       ts.nspname as template_schema,
       d.dictinitoption as init_options,
       quote_literal(obj_description(d.oid, 'pg_ts_dict')) as comment,
       x.extname as extension
  from pg_catalog.pg_ts_dict d
 inner join pg_catalog.pg_namespace ds
    on ds.oid = d.dictnamespace
 inner join pg_catalog.pg_authid o
     on o.oid = d.dictowner
 inner join pg_catalog.pg_ts_template t
    on t.oid = d.dicttemplate
 inner join pg_catalog.pg_namespace ts
    on ts.oid = t.tmplnamespace
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = d.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_ts_dict'
  where ds.nspname not in ('pg_catalog')
order by ds.nspname, t.tmplname

