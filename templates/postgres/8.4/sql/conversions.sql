-- List all conversions: 
select conname as name,
       n.nspname as schema,
       o.rolname as owner,
       pg_catalog.pg_encoding_to_char(c.conforencoding) as source,
       pg_catalog.pg_encoding_to_char(c.contoencoding) as destination,
       p.proname as function_name,
       pn.nspname as function_schema,
       c.conproc::oid as procoid,
       c.condefault as is_default,       
       quote_literal(obj_description(c.oid, 'pg_conversion')) as comment
from   pg_catalog.pg_conversion c
inner join pg_catalog.pg_namespace n
  on (n.oid = c.connamespace)
inner join pg_catalog.pg_authid o
  on (o.oid = c.conowner)
inner join pg_catalog.pg_proc p
  on (p.oid = c.conproc)
inner join pg_catalog.pg_namespace pn
  on (pn.oid = p.pronamespace)
where  n.nspname not in ('pg_catalog', 'information_schema')
order by n.nspname, c.conname;

