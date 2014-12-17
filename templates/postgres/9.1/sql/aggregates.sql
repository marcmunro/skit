-- List all aggregate functions
select p.proname as name,
       n.nspname as schema,
       r.rolname as owner,
       pt.proname as trans_func_name,
       nt.nspname as trans_func_schema,
       pf.proname as final_func_name,
       nf.nspname as final_func_schema,
       op.oprname as sort_op_name,
       nop.nspname as sort_op_schema,
       quote_literal(a.agginitval) as initcond,
       tt.typname as trans_type_name,
       ttn.nspname as trans_type_schema,
       bt.typname as basetype_name,
       bn.nspname as basetype_schema,
       p.proname || '(' || 
       case when bt.typname is null then '*'
       else bn.nspname || '.' || bt.typname end || ')' as signature,
       quote_literal(obj_description(a.aggfnoid, 'pg_proc')) as comment,
       x.extname as extension
  from pg_catalog.pg_aggregate a
 inner join pg_catalog.pg_proc p
    on p.oid = a.aggfnoid
 inner join pg_catalog.pg_namespace n
    on n.oid = p.pronamespace
 inner join pg_catalog.pg_proc pt
    on pt.oid = a.aggtransfn
 inner join pg_catalog.pg_namespace nt
    on nt.oid = pt.pronamespace
 inner join pg_catalog.pg_roles r
    on r.oid = p.proowner
  left outer join
    (        pg_catalog.pg_proc pf
    inner join pg_catalog.pg_namespace nf
      on nf.oid = pf.pronamespace
    )
    on pf.oid = a.aggfinalfn
  left outer join
     (     pg_catalog.pg_operator op
     inner join pg_catalog.pg_namespace nop
       on nop.oid = op.oprnamespace
     )
    on op.oid = a.aggsortop
  left outer join
     (    pg_catalog.pg_type tt
     inner join pg_catalog.pg_namespace ttn
       on ttn.oid = tt.typnamespace
     )
    on tt.oid = a.aggtranstype
  left outer join
     (    pg_catalog.pg_type bt
     inner join pg_catalog.pg_namespace bn
       on bn.oid = bt.typnamespace
     )
    on bt.oid = p.proargtypes[0]
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = p.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_proc'
where  n.nspname not in ('pg_catalog', 'information_schema');

