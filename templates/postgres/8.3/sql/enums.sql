-- List all enums
select t.oid as oid,
       t.typname as name,
       n.nspname as schema,
       o.rolname as owner,
       quote_literal(obj_description(t.oid, 'pg_type')) as comment
from   pg_catalog.pg_type t
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = t.typnamespace)
       inner join
          pg_catalog.pg_authid o
       on (o.oid = t.typowner)
       left outer join 
	  (pg_catalog.pg_type t2
	   inner join
	       pg_catalog.pg_namespace nt2
	   on (nt2.oid = t2.typnamespace)
	  )
	on (t2.oid = t.typbasetype)
where   t.typisdefined
  and   n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
  and   t.typtype = 'e'
order by n.nspname, t.typname;




