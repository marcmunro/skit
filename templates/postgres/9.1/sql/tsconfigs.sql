-- List all text search configurations: 
select c.oid as oid,
       c.cfgname as name,
       n.nspname as schema,
       o.rolname as owner,
       p.prsname as parser_name,
       np.nspname as parser_schema,
       quote_literal(obj_description(c.oid, 'pg_ts_config')) as comment,
       x.extname as extension
  from pg_catalog.pg_ts_config c
 inner join pg_catalog.pg_namespace n
     on (n.oid = c.cfgnamespace)
 inner join pg_catalog.pg_authid o
     on (o.oid = c.cfgowner)
 inner join pg_catalog.pg_ts_parser p
     on (p.oid = c.cfgparser)
 inner join pg_catalog.pg_namespace np
     on (np.oid = p.prsnamespace)
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = c.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_ts_config'
  where n.nspname not in ('pg_catalog')
  order by n.nspname, c.cfgname;

