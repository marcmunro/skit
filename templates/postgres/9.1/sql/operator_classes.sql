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
       oc.opcdefault as is_default,
       t.typname as type_name,
       tn.nspname as type_schema,
       d.deptype = 'a' as generates_opfamily,
       quote_literal(obj_description(oc.oid, 'pg_opclass')) as comment,
       x.extname as extension
  from pg_catalog.pg_opclass oc
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
  left outer join (pg_catalog.pg_type t
      inner join pg_catalog.pg_namespace tn
         on tn.oid = t.typnamespace
     )
    on t.oid = oc.opckeytype
  left outer join (pg_catalog.pg_depend d
      inner join pg_catalog.pg_class dn
         on dn.oid = d.classid 
      inner join pg_catalog.pg_class rn
         on rn.oid = d.refclassid 
     )
    on d.objid = oc.oid
   and d.refobjid = f.oid
   and dn.relname = 'pg_opclass'
   and rn.relname = 'pg_opfamily'
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = oc.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_opclass'
 where n.nspname not in ('pg_catalog', 'information_schema')
 order by n.nspname, oc.opcname;

