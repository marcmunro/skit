-- List columns for a specific type

select a.attnum as id,
       a.attname as name,
       case when t2.typname is null 
       then t.typname else t2.typname end as type,
       case when t2.typname is null
       then n2.nspname else n3.nspname end as type_schema,
       nullif(regexp_replace(
	  pg_catalog.format_type(a.atttypid, a.atttypmod),
          E'(.*\\(([0-9]*).*)|.*', E'\\2'), '') as size,
       nullif(regexp_replace(
	  pg_catalog.format_type(a.atttypid, a.atttypmod),
          E'(.*\\([0-9]*,([0-9]*).*)|.*', E'\\2'), '') as precision,
       case when a.attnotnull then 'no' else 'yes' end as nullable,
       nullif(a.attndims, 0) as dimensions,
       a.attstorage as storage_policy,
       col.collname as collation_name,
       coln.nspname as collation_schema,
       pg_catalog.pg_get_expr(d.adbin, d.adrelid) as default,
       quote_literal(col_description(c.oid, a.attnum)) as comment
  from pg_catalog.pg_class c  
 inner join pg_catalog.pg_namespace n 
    on n.oid = c.relnamespace
 inner join pg_catalog.pg_attribute a
    on a.attrelid = c.oid
 inner join pg_catalog.pg_type t
    on t.oid = a.atttypid
 inner join pg_catalog.pg_namespace n2 
    on n2.oid = t.typnamespace
  left outer join pg_catalog.pg_attrdef d
    on (    d.adrelid = a.attrelid
        and d.adnum = a.attnum)
  left outer join (pg_catalog.pg_collation col
        inner join pg_catalog.pg_namespace coln
           on (coln.oid = col.collnamespace))
    on col.oid = a.attcollation
  left outer join 
       (pg_catalog.pg_type t2
	inner join pg_catalog.pg_namespace n3
	    on (n3.oid = t2.typnamespace))
    on (    t2.oid = t.typelem
        and t.typlen < 0)
 where c.oid = :1
   and a.attname not in ('xmin', 'xmax', 'cmin', 'cmax', 'ctid', 'tableoid')
   and not a.attisdropped
 order by a.attnum;
