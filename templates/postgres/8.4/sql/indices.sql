-- List all non pk and uk indexes
select c.oid as oid,
       c.relname as name,
       n.nspname as schema,
       r.rolname as owner,
       case when t.spcname is null then td.spcname
       else t.spcname end as tablespace, 
       am.amname as index_am, 
       i.indisunique as unique,
       i.indisclustered as clustered,
       i.indisvalid as valid,
       i.indkey as colnums,
       --'1978 86328' as operator_classes, 
       i.indclass as operator_classes,
       pg_catalog.pg_get_indexdef(i.indexrelid) as indexdef,
       quote_literal(obj_description(c.oid, 'pg_class')) as comment
       --indpred, indexprs
from   pg_catalog.pg_index i
inner join pg_catalog.pg_class c on c.oid = i.indexrelid
inner join pg_catalog.pg_namespace n on n.oid = c.relnamespace
inner join pg_catalog.pg_namespace cat  -- pg_catalog schema
  on cat.nspname = 'pg_catalog'
inner join pg_catalog.pg_class cclass   -- pg_class relation
  on  cclass.relname = 'pg_class'
  and cclass.relnamespace = cat.oid
inner join pg_catalog.pg_class ccons    -- pg_constraint relation
  on  ccons.relname = 'pg_constraint'	
  and ccons.relnamespace = cat.oid
left outer join pg_catalog.pg_depend d  -- join to contstraint implemented
  on  d.objid = c.oid                   -- by this index
  and d.classid = cclass.oid
  and d.refclassid = ccons.oid
inner join pg_catalog.pg_roles r 
  on  r.oid = c.relowner
inner join pg_catalog.pg_am am
  on am.oid = c.relam
left outer join pg_catalog.pg_tablespace t
  on t.oid = c.reltablespace
inner join pg_catalog.pg_database dat
  on dat.datname = current_database()
inner join pg_catalog.pg_tablespace td
  on td.oid = dat.dattablespace
where  i.indrelid = :1
--where  i.indrelid = 88436
and    d.objid is null                  -- eliminate indexes for constraints
order by n.nspname, c.relname;