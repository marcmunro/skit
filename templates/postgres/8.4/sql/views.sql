-- List all views

-- List all tables in non-system schemas.

select c.oid::oid as oid,
       d.objid as rewrite_oid,  -- The oid of the pg_rewrite rule for
       	       	  		-- this view will give us our dependencies
       n.nspname as schema,
       c.relname as name,
       v.definition as definition,
       v.viewowner as owner,
       c.relacl::text as privs,
       quote_literal(obj_description(c.oid, 'pg_class')) as comment
from   pg_catalog.pg_class c
inner join pg_catalog.pg_namespace n 
    on n.oid = c.relnamespace
inner join pg_catalog.pg_views v
  on  v.schemaname = n.nspname
  and v.viewname = c.relname
inner join pg_catalog.pg_depend d
  on  d.refobjid = c.oid
  and d.refclassid = 'pg_class'::regclass
  and d.classid = 'pg_rewrite'::regclass
  and d.deptype = 'i'
where  c.relkind = 'v'
and    n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
order by 1, 2;
