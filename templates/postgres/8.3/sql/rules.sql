-- List all rules

select rw.oid as oid, 
       r.rulename as name,
       r.tablename as table,
       r.schemaname as schema,
       r.definition as definition,
       quote_literal(obj_description(rw.oid, 'pg_rewrite')) as comment
from   pg_catalog.pg_rules r,
       pg_catalog.pg_rewrite rw
where  rw.rulename = r.rulename
and    r.schemaname not in ('pg_catalog', 'pg_toast', 'information_schema')
order by 1, 2;

