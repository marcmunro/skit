-- List all base types
select t.oid as oid,
       t.typname as name,
       n.nspname as schema,
       o.rolname as owner,
       'base' as type,
       t.typdelim as delimiter,
       t.typlen as typelen,
       case t.typbyval when true then 'yes' else null end as passbyval,
       t.typinput::oid as input_oid,
       t.typoutput::oid as output_oid,
       t.typisdefined as is_defined,
       nullif(0, t.typreceive::oid) as receive_oid,
       nullif(0, t.typsend::oid) as send_oid,
       nullif(0, t.typanalyze::oid) as analyze_oid,
       case t.typalign when 'c' then 'char' when 's' then 'int2'
	    when 'i' then 'int4' when 'd' then 'double' end as alignment,

       case t.typstorage when 'p' then 'plain' when 'x' then 'extended'
	    when 'e' then 'external' when 'm' then  'main' end as storage,
       t.typdefault as default,
       t.typcategory as type_category,
       t.typispreferred as is_preferred,
       case when t.typcollation != 0 then t.typcollation
       else null end as collation_oid,
       quote_literal(obj_description(t.oid, 'pg_type')) as comment,
       x.extname as extension
from   pg_catalog.pg_type t
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = t.typnamespace)
       inner join
          pg_catalog.pg_authid o
       on (o.oid = t.typowner)
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
  and    t.typtype in ('b')
  and    not exists (  -- Eliminate automatically created array types
  	  select 1
	  from   pg_catalog.pg_type t3
	  where  t3.oid = t.typelem
	  and    '_' || t3.typname = t.typname
         )
       ) or not t.typisdefined
order by n.nspname, t.typname;




