select f.fdwname as name,
       o.rolname as owner,
       f.fdwoptions as options,
       v.proname as validator_proc,
       vs.nspname as validator_schema,
       f.fdwacl as privs,
       x.extname as extension
  from pg_catalog.pg_foreign_data_wrapper f
 inner join pg_catalog.pg_roles o
    on o.oid = f.fdwowner 
  left outer join pg_catalog.pg_proc v
    on v.oid = f.fdwvalidator
  left outer join pg_catalog.pg_namespace vs
    on vs.oid = v.pronamespace
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = f.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_foreign_data_wrapper'
order by 1;
