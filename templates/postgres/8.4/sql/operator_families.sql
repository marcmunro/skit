select opf.oid as oid,
       opf.opfname as name,
       n.nspname as schema,
       r.rolname as owner,
       am.amname as method,
       d.deptype = 'a' as auto_generated,
       quote_literal(obj_description(opf.oid, 'pg_opfamily')) as comment
from   pg_catalog.pg_opfamily opf
inner join pg_catalog.pg_namespace n
  on n.oid = opf.opfnamespace
inner join pg_catalog.pg_roles r
  on r.oid = opf.opfowner
inner join pg_catalog.pg_am am
  on am.oid = opf.opfmethod
inner join (pg_catalog.pg_depend d
  inner join pg_catalog.pg_class dn
    on dn.oid = d.classid 
  inner join pg_catalog.pg_class rn
    on rn.oid = d.refclassid 
  )
  on  d.refobjid = opf.oid
  and dn.relname = 'pg_opclass'
  and rn.relname = 'pg_opfamily'
where n.nspname != 'pg_catalog';
