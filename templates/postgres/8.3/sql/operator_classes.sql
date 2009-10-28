-- List all operator classes.
select oc.opcname as name,
       n.nspname as schema,
       o.rolname as owner,
       oc.oid::oid as oid,
       ti.typname as intype_name,
       ni.nspname as intype_schema,
       am.amname as method, 
       f.opfname as family,
       nf.nspname as family_schema,
       oc.opcdefault as is_default
from   pg_catalog.pg_opclass oc
inner join pg_catalog.pg_namespace n
  on n.oid = oc.opcnamespace
inner join pg_catalog.pg_authid o
  on o.oid = oc.opcowner
inner join pg_catalog.pg_type ti
  on ti.oid = oc.opcintype
inner join pg_catalog.pg_namespace ni
  on ni.oid = ti.typnamespace
inner join pg_catalog.pg_am am
  on am.oid = oc.opcmethod
inner join pg_catalog.pg_opfamily f
  on f.oid = oc.opcfamily
inner join pg_catalog.pg_namespace nf
  on nf.oid = f.opfnamespace
where  n.nspname not in ('pg_catalog', 'information_schema')
order by n.nspname, oc.opcname;

