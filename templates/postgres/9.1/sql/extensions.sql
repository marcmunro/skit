select e.oid as oid,
       e.extname as name,
       o.rolname as owner,
       n.nspname as schema,
       extrelocatable as relocatable,
       extversion as version,
       extconfig as config,
       extcondition as condition,
       quote_literal(obj_description(e.oid, 'pg_extension')) as comment
  from pg_catalog.pg_extension e
 inner join pg_catalog.pg_roles o
    on o.oid = e.extowner
 inner join pg_catalog.pg_namespace n
    on n.oid = e.extnamespace;