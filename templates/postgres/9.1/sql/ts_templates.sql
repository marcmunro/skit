-- Get information about dictionaries.
select t.tmplname as name,
       ts.nspname as schema,
       i.proname as init_proc,
       ins.nspname as init_schema,
       l.proname as lexize_proc,
       ls.nspname as lexize_schema,
       quote_literal(obj_description(t.oid, 'pg_ts_template')) as comment,
       x.extname as extension
  from pg_catalog.pg_ts_template t
 inner join pg_catalog.pg_namespace ts
    on ts.oid = t.tmplnamespace
  left outer join pg_catalog.pg_proc i
    on i.oid = t.tmplinit
  left outer join pg_catalog.pg_namespace ins
    on ins.oid = i.pronamespace
 inner join pg_catalog.pg_proc l
    on l.oid = t.tmpllexize
 inner join pg_catalog.pg_namespace ls
    on ls.oid = l.pronamespace
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = t.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_ts_template'
  where ts.nspname not in ('pg_catalog')
order by ts.nspname, t.tmplname;

