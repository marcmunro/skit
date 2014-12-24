-- List all base types
select t.oid as oid,
       t.typname as name,
       n.nspname as schema,
       o.rolname as owner,
       'range' as type,
       t2.typname as subtype_name,
       n2.nspname as subtype_schema,
       col.collname as collation_name,
       coln.nspname as collation_schema,
       opc.opcname as opclass_name,
       opcn.nspname as opclass_schema,
       pc.proname as canonical_name,
       pcn.nspname as canonical_schema,
       r.rngcanonical::integer as canonical_oid,
       pd.proname as subdiff_name,
       pdn.nspname as subdiff_schema,
       r.rngsubdiff::integer as subdiff_oid,
       quote_literal(obj_description(t.oid, 'pg_type')) as comment,
       x.extname as extension
  from pg_catalog.pg_type t
 inner join pg_catalog.pg_namespace n
    on n.oid = t.typnamespace
 inner join pg_catalog.pg_authid o
     on o.oid = t.typowner
 inner join pg_catalog.pg_range r
    on r.rngtypid = t.oid
 inner join pg_catalog.pg_type t2
    on t2.oid = r.rngsubtype
 inner join pg_catalog.pg_namespace n2
    on n2.oid = t2.typnamespace
  left outer join (pg_catalog.pg_collation col
          inner join pg_catalog.pg_namespace coln
	     on coln.oid = col.collnamespace)
    on col.oid = rngcollation
  left outer join (pg_catalog.pg_opclass opc
          inner join pg_catalog.pg_namespace opcn
	     on opcn.oid = opc.opcnamespace)
    on opc.oid = r.rngsubopc
  left outer join (pg_catalog.pg_proc pc
          inner join pg_catalog.pg_namespace pcn
	     on pcn.oid = pc.pronamespace)
    on pc.oid = r.rngcanonical
  left outer join (pg_catalog.pg_proc pd
          inner join pg_catalog.pg_namespace pdn
	     on pdn.oid = pd.pronamespace)
    on pd.oid = r.rngsubdiff
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = t.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_type'
where  (
         n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
  and    t.typtype in ('r')
  and    not exists (  -- Eliminate automatically created array types
  	  select 1
	  from   pg_catalog.pg_type t3
	  where  t3.oid = t.typelem
	  and    '_' || t3.typname = t.typname
         )
       ) or not t.typisdefined
order by n.nspname, t.typname;




