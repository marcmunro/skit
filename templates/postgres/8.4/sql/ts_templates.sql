-- Get information about dictionaries.
select t.tmplname as name,
       ts.nspname as schema,
       i.proname as init_proc,
       ins.nspname as init_schema,
       l.proname as lexize_proc,
       ls.nspname as lexize_schema,
       quote_literal(obj_description(t.oid, 'pg_ts_template')) as comment
  from pg_catalog.pg_ts_template t
 inner join pg_catalog.pg_namespace ts
    on ts.oid = t.tmplnamespace
 inner join pg_catalog.pg_proc i
    on i.oid = t.tmplinit
 inner join pg_catalog.pg_namespace ins
    on ins.oid = i.pronamespace
 inner join pg_catalog.pg_proc l
    on l.oid = t.tmpllexize
 inner join pg_catalog.pg_namespace ls
    on ls.oid = l.pronamespace
  where ts.nspname not in ('pg_catalog')
order by ts.nspname, t.tmplname;

