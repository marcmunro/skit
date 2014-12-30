-- Columns for a table, including details of inherited columns

with recursive inheritence_set(
    reloid, tablename, 
    schemaname, seq_no) as
(
  select t.oid, t.relname, s.nspname, 0
    from pg_catalog.pg_class t
   inner join pg_catalog.pg_namespace s
      on s.oid = t.relnamespace
   where t.oid = :1
  union all
  select t.oid, t.relname, s.nspname, i.inhseqno
    from inheritence_set iset
   inner join pg_catalog.pg_inherits i
      on i.inhrelid = iset.reloid
   inner join pg_catalog.pg_class t
      on t.oid = i.inhparent
   inner join pg_catalog.pg_namespace s
      on s.oid = t.relnamespace
)
select a.attnum as colnum,
       a.attname as name,
       iset.tablename, 
       iset.schemaname,
       iset.seq_no > 0 as is_inherited,
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
       pg_catalog.pg_get_expr(d.adbin, d.adrelid) as default,
       nullif(a.attstattarget, -1) as stats_target,
       col.collname as collation_name,
       coln.nspname as collation_schema,
       quote_literal(col_description(a.attrelid, a.attnum)) as comment
  from inheritence_set iset
 inner join pg_catalog.pg_attribute a
    on a.attrelid = iset.reloid
   and a.attname not in ('xmin', 'xmax', 'cmin', 'cmax', 'ctid', 'tableoid')
   and not a.attisdropped
 inner join pg_catalog.pg_type t
    on t.oid = a.atttypid
 inner join pg_catalog.pg_namespace n2 
    on n2.oid = t.typnamespace
  left outer join (pg_catalog.pg_collation col
        inner join pg_catalog.pg_namespace coln
           on (coln.oid = col.collnamespace))
    on col.oid = a.attcollation
  left outer join pg_catalog.pg_attrdef d
    on (    d.adrelid = a.attrelid
        and d.adnum = a.attnum)
  left outer join 
       (pg_catalog.pg_type t2
        inner join pg_catalog.pg_namespace n3
           on (n3.oid = t2.typnamespace))
    on (    t2.oid = t.typelem
        and t.typlen < 0)
 where a.attnum > 0
 order by iset.seq_no, a.attnum;
