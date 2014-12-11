-- List all operators.

select op.oid,
       op.oprname as name,
       n.nspname as schema,
       o.rolname as owner,
       lt.typname as leftarg_type,
       ltn.nspname as leftarg_schema,
       rt.typname as rightarg_type,
       rtn.nspname as rightarg_schema,
       res.typname as result_type,
       resn.nspname as result_schema,
       p.proname as procedure,
       pn.nspname as procedure_schema,
       cop.oprname as commutator,
       cn.nspname as commutator_schema,
       nop.oprname as negator,
       nn.nspname as negator_schema,
       pr.proname as restrict_proc,
       prn.nspname as restrict_proc_schema,
       pj.proname as join_proc,
       pjn.nspname as join_proc_schema,
       case op.oprcanhash when true then 'yes' else null end as hashes,
       case op.oprcanmerge when true then 'yes' else null end as merges,
       '(' || ltn.nspname || '.' || lt.typname || ',' ||
       rtn.nspname || '.' || rt.typname || ')' as params,
       quote_literal(obj_description(op.oid, 'pg_operator')) as comment,
       x.extname as extension
  from pg_catalog.pg_operator op
 inner join pg_catalog.pg_namespace n
    on n.oid = op.oprnamespace
 inner join pg_catalog.pg_authid o
    on o.oid = op.oprowner
  left outer join
       (     pg_catalog.pg_proc p
       inner join pg_catalog.pg_namespace pn
          on pn.oid = p.pronamespace
       )
    on p.oid = op.oprcode
  left outer join
       (     pg_catalog.pg_type lt
       inner join pg_catalog.pg_namespace ltn
          on ltn.oid = lt.typnamespace
       )
    on lt.oid = op.oprleft
  left outer join
       (     pg_catalog.pg_type rt
       inner join pg_catalog.pg_namespace rtn
          on rtn.oid = rt.typnamespace
       )
    on rt.oid = op.oprright
  left outer join
       (     pg_catalog.pg_type res
       inner join pg_catalog.pg_namespace resn
          on resn.oid = res.typnamespace
       )
    on res.oid = op.oprresult
  left outer join
       (      pg_catalog.pg_operator cop
       inner join pg_catalog.pg_namespace cn
          on cn.oid = cop.oprnamespace
       )
    on cop.oid = op.oprcom
 left outer join
      (      pg_catalog.pg_operator nop
      inner join pg_catalog.pg_namespace nn
          on nn.oid = nop.oprnamespace
      )
   on nop.oid = op.oprnegate
 left outer join
      (      pg_catalog.pg_proc pr
      inner join pg_catalog.pg_namespace prn
         on prn.oid = pr.pronamespace
      )
   on pr.oid = op.oprrest
 left outer join
      (      pg_catalog.pg_proc pj
      inner join pg_catalog.pg_namespace pjn
         on pjn.oid = pj.pronamespace
      )
   on pj.oid = op.oprjoin
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = op.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_operator'
 where n.nspname not in ('pg_catalog')
   and op.oprcode != 0::oid	-- Ignore incompletely defined operators
 order by n.nspname, op.oprname;


