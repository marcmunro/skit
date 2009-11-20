-- List all pk and uk constraints
select c.oid as oid,
       c.conname as name,
       n.nspname as schema,
       case c.contype when 'p' then 'primary key' 
       when 'u' then 'unique' else 'unknown' end as constraint_type,
       c.condeferrable as deferrable,
       c.condeferred as deferred,
       regexp_replace(c.conkey::text, '{(.*)}', E'\\1') as columns,
       pg_catalog.pg_get_constraintdef(c.oid) as source,
       regexp_replace(cl.reloptions::text, '{(.*)}', E'\\1') as options,
       case t.spcname when 'pg_default' then null 
       else t.spcname end as tablespace, 
       am.amname as access_method, au.rolname as owner
from   pg_catalog.pg_constraint c
inner join  pg_catalog.pg_namespace n
  on  n.oid = c.connamespace
inner join (pg_catalog.pg_depend d
   inner join pg_catalog.pg_class dn
     on dn.oid = d.classid 
   inner join pg_catalog.pg_class rn
     on rn.oid = d.refclassid 
   )
  on  d.refobjid = c.oid
  and dn.relname = 'pg_class'
  and rn.relname = 'pg_constraint'
inner join pg_catalog.pg_class cl
  on cl.oid = d.objid
left outer join pg_catalog.pg_tablespace t
  on t.oid = cl.reltablespace
inner join pg_catalog.pg_am am
  on am.oid = cl.relam
inner join pg_catalog.pg_authid au
  on au.oid = cl.relowner
where   c.conrelid = :1
order by c.contype, n.nspname, c.conname;


