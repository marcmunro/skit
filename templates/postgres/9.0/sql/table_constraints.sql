with my_constraints as (
  select c.oid,
       c.conname as name,
       n.nspname as schema,
       c.contype,
       case c.contype when 'p' then 'primary key' 
       when 'u' then 'unique' 
       when 'f' then 'foreign key' 
       when 't' then 'trigger'
       when 'x' then 'exclusion'
       else 'check' end as constraint_type,
       c.condeferrable as deferrable,
       c.condeferred as deferred,
       regexp_replace(c.conkey::text, '{(.*)}', E'\\1') as columns,
       regexp_replace(c.confkey::text, '{(.*)}', E'\\1') as refcolumns,
       c.conindid as index_oid,
       cr.oid as refoid, 
       cr.relname as reftable,
       nr.nspname as refschema,
       case c.contype when 'f' then c.confmatchtype 
       else null end as confmatchtype,
       case c.contype when 'f' then c.confupdtype 
       else null end as confupdtype,
       case c.contype when 'f' then c.confdeltype 
       else null end as confdeltype,
       c.conislocal as is_local,
       c.conexclop,
       c.conkey,
       c.conrelid as table_oid
    from pg_catalog.pg_constraint c
   inner join pg_catalog.pg_namespace n
      on n.oid = c.connamespace
    left outer join 
   (         pg_catalog.pg_class cr             -- referenced table for fk
       inner join pg_catalog.pg_namespace nr    -- schema of ref table for fk
           on nr.oid = cr.relnamespace
    ) on cr.oid = c.confrelid
   --where c.conrelid = 17057
   where c.conrelid = :1
     and c.contype != 't'
),
  indexinfo as (
  select c.oid, ic.relname as refindexname,
         n.nspname as refindexschema, 
         case t.spcname when 'pg_default' then null 
         else t.spcname end as tablespace,
	 r.rolname as owner,
	 am.amname as access_method,
	 case when i.indpred is not null 
	 then regexp_replace(pg_catalog.pg_get_indexdef(ic.oid),
	                     '(.*) WHERE .*', E'\\1')
         else pg_catalog.pg_get_indexdef(ic.oid) 
	 end as indexdef,
	 i.indclass as opclasses,
	 case when i.indpred is not null 
	 then regexp_replace(pg_catalog.pg_get_indexdef(ic.oid),
	                     '.* WHERE ', 'where (') || ')'
	 else null end as predicate,
         regexp_replace(ic.reloptions::text, '{(.*)}', E'\\1') as options,
	 refc.conname as refconstraintname
    from my_constraints c
   inner join pg_catalog.pg_class ic
      on ic.oid = c.index_oid
   inner join pg_catalog.pg_index i
      on i.indexrelid = ic.oid
   inner join  pg_catalog.pg_namespace n
      on n.oid = ic.relnamespace
    left outer join pg_catalog.pg_tablespace t
      on t.oid = ic.reltablespace
   inner join pg_catalog.pg_roles r
      on r.oid = ic.relowner
   inner join pg_catalog.pg_am am
      on am.oid = ic.relam
    left outer join pg_catalog.pg_constraint refc
      on refc.conindid = c.index_oid
     and c.contype = 'f'
     and refc.contype in ('p', 'u')
),
  colinfo as (
  select oid, table_oid, conkey[i] as colno, 
         conexclop[i] as opno, opclasses[i-1] as opclass_oid
    from (
      select c.oid, generate_series(1, array_upper(c.conkey, 1)) as i, 
             c.conkey, c.conexclop, c.table_oid, i.opclasses
        from my_constraints c
       inner join indexinfo i
          on i.oid = c.oid) t
),
  coldetails as (
  select c.oid, c.colno, c.opno,
         a.attname as colname, o.oprname, c.opclass_oid,
	 oc.opcname as opclass_name, ocn.nspname as opclass_schema
    from colinfo c
   inner join pg_catalog.pg_attribute a
      on a.attrelid = c.table_oid
     and a.attnum = c.colno
    left outer join pg_catalog.pg_operator o
      on o.oid = c.opno
    left outer join pg_catalog.pg_opclass oc
      on oc.oid = c.opclass_oid
    left outer join pg_catalog.pg_namespace ocn
      on ocn.oid = oc.opcnamespace
),  colstrs as (
  select oid, 
         string_agg(quote_ident(colname), ', ') as colnames,
	 string_agg(quote_ident(colname) || ' with ' || 
	                        oprname, ', ') as colexprs,
	 string_agg(oprname, ',') as operators,
	 string_agg(opclass_oid::text, ', ') as opclasses,
	 string_agg('"' || opclass_name || '"', ',') as opclass_names,
	 string_agg('"' || opclass_schema|| '"', ',') as opclass_schemata
    from coldetails
   group by oid
)
select c.oid, c.name, c.schema,
       c.constraint_type, c.deferrable, c.deferred,
       c.columns, c.refcolumns, i.options,
       i.tablespace, i.access_method, i.owner,
       c.refoid, c.reftable, c.refschema,
       i.refindexname, i.refindexschema, i.refconstraintname, 
       c.confmatchtype, c.confupdtype, c.confdeltype, 
       c.is_local,
           case c.constraint_type 
           when 'exclusion' 
           then regexp_replace(i.indexdef, '.* WITH ', '')
           else i.indexdef
           end as indexdef, i.predicate, 
       cs.colexprs, cs.opclass_names, cs.opclass_schemata,
       cs.operators,
       pg_catalog.pg_get_constraintdef(c.oid) as source,
           quote_literal(obj_description(c.oid, 'pg_constraint')) as comment
  from my_constraints c
  left outer join indexinfo i
    on i.oid = c.oid
  left outer join colstrs cs
    on cs.oid = c.oid
 order by c.oid, c.schema, c.name;