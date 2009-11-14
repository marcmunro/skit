-- List all tables in non-system schemas.

select a.attnum as colnum,
       a.attname as name,
       /*case when a.attislocal then 'loc.' || a.attname
       else 'inh.' || a.attname end as matchname, */
       case when t2.typname is null 
       then t.typname else t2.typname end as type,
       n2.nspname as type_schema,
       nullif(regexp_replace(
	  pg_catalog.format_type(a.atttypid, a.atttypmod),
          E'(.*\\(([0-9]*).*)|.*', E'\\2'), '') as size,
       nullif(regexp_replace(
	  pg_catalog.format_type(a.atttypid, a.atttypmod),
          E'(.*\\([0-9]*,([0-9]*).*)|.*', E'\\2'), '') as precision,
       case when a.attnotnull then 'no' else 'yes' end as nullable,
       case when a.attndims != 0
       then substr('[][][][][][][][][][][][]', 1, a.attndims*2)
       else null end as dimensions,
       case when a.attstorage = t.typstorage
       then null else a.attstorage end as storage_policy,
       a.attislocal as is_local,
       quote_literal(pg_catalog.pg_get_expr(d.adbin, d.adrelid)) as default,
       nullif(a.attstattarget, -1) as stats_target,
       quote_literal(col_description(a.attrelid, a.attnum)) as comment
from   pg_catalog.pg_attribute a
    inner join pg_catalog.pg_type t
        on t.oid = a.atttypid
    inner join pg_catalog.pg_namespace n2 
        on n2.oid = t.typnamespace
    left outer join pg_catalog.pg_attrdef d
        on (    d.adrelid = a.attrelid
            and d.adnum = a.attnum)
    left outer join 
       (pg_catalog.pg_type t2
	inner join pg_catalog.pg_namespace n3
	    on (n3.oid = t2.typnamespace))
        on (    t2.oid = t.typelem
            and t.typlen < 0)
where  a.attrelid = :1
and    a.attname not in ('xmin', 'xmax', 'cmin', 'cmax', 'ctid', 'tableoid')
and    not a.attisdropped
order by colnum;
