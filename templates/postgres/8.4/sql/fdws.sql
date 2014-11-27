select f.fdwname as name,
       o.rolname as owner,
       f.fdwoptions as options,
       v.proname as validator_proc,
       vs.nspname as validator_schema,
       f.fdwacl as privs
  from pg_catalog.pg_foreign_data_wrapper f
 inner join pg_catalog.pg_roles o
    on o.oid = f.fdwowner 
  left outer join pg_catalog.pg_proc v
    on v.oid = f.fdwvalidator
  left outer join pg_catalog.pg_namespace vs
    on vs.oid = v.pronamespace
order by 1;
