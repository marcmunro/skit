-- List all text search configurations: 
select c.oid as oid,
       c.cfgname as name,
       n.nspname as schema,
       o.rolname as owner,
       p.prsname as parser_name,
       np.nspname as parser_schema,
       quote_literal(obj_description(c.oid, 'pg_ts_config')) as comment
  from pg_catalog.pg_ts_config c
 inner join pg_catalog.pg_namespace n
     on (n.oid = c.cfgnamespace)
 inner join pg_catalog.pg_authid o
     on (o.oid = c.cfgowner)
 inner join pg_catalog.pg_ts_parser p
     on (p.oid = c.cfgparser)
 inner join pg_catalog.pg_namespace np
     on (np.oid = p.prsnamespace)
  where n.nspname not in ('pg_catalog')
  order by n.nspname, c.cfgname;

