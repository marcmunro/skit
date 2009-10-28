-- List all operator family operators.
select am.amopfamily as family_oid,
       d.refobjid as class_oid,
       am.amopstrategy as strategy,
       op.oprname as name,
       nop.nspname as schema,
       am.amopreqcheck as recheck,
       lt.typname as leftarg_type,
       ltn.nspname as leftarg_schema,
       rt.typname as rightarg_type,
       rtn.nspname as rightarg_schema
from   pg_catalog.pg_amop am
inner join pg_catalog.pg_operator op
  on  op.oid = am.amopopr
inner join pg_catalog.pg_namespace nop
  on nop.oid = op.oprnamespace
inner join pg_catalog.pg_opfamily opf
  on  opf.oid = am.amopfamily
inner join pg_catalog.pg_namespace nf
  on nf.oid = opf.opfnamespace
left outer join (pg_catalog.pg_type lt
      inner join pg_catalog.pg_namespace ltn
      on  ltn.oid = lt.typnamespace
) on lt.oid = op.oprleft
left outer join (pg_catalog.pg_type rt
      inner join pg_catalog.pg_namespace rtn
      on  rtn.oid = rt.typnamespace
) on rt.oid = op.oprright
left outer join (pg_catalog.pg_depend d
   inner join pg_catalog.pg_class dn
     on dn.oid = d.classid 
   inner join pg_catalog.pg_class rn
     on rn.oid = d.refclassid 
   )
  on  d.objid = am.oid
  and rn.relname = 'pg_opclass'
  and dn.relname = 'pg_amop'
where (   nop.nspname != 'pg_catalog'
       or nf.nspname != 'pg_catalog')
