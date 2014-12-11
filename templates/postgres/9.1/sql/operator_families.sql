select opf.oid as oid,
       opf.opfname as name,
       n.nspname as schema,
       r.rolname as owner,
       am.amname as method,
       coalesce(d.deptype = 'a', false) as auto_generated,
       quote_literal(obj_description(opf.oid, 'pg_opfamily')) as comment,
       x.extname as extension
from   pg_catalog.pg_opfamily opf
inner join pg_catalog.pg_namespace n
  on n.oid = opf.opfnamespace
inner join pg_catalog.pg_roles r
  on r.oid = opf.opfowner
inner join pg_catalog.pg_am am
  on am.oid = opf.opfmethod
left outer join (pg_catalog.pg_depend d
  inner join pg_catalog.pg_class dn
    on dn.oid = d.classid 
  inner join pg_catalog.pg_class rn
    on rn.oid = d.refclassid 
  )
  on  d.refobjid = opf.oid
  and dn.relname = 'pg_opclass'
  and rn.relname = 'pg_opfamily'
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = opf.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_opfamily'
where n.nspname != 'pg_catalog';
