-- List all domains
select t.typname as name,
       n.nspname as schema,
       o.rolname as owner,
       c.oid as conoid,
       t2.typname as basetype,
       nt2.nspname as basetype_schema,
       t.typdefault as default,
       case when t.typnotnull then 'no' else 'yes' end as nullable,
       quote_literal(obj_description(t.oid, 'pg_type')) as comment,
       x.extname as extension
  from pg_catalog.pg_type t
 inner join pg_catalog.pg_namespace n
    on n.oid = t.typnamespace
 inner join pg_catalog.pg_authid o
    on o.oid = t.typowner
  left outer join (
           pg_catalog.pg_type t2
     inner join pg_catalog.pg_namespace nt2
	on nt2.oid = t2.typnamespace
       )
    on t2.oid = t.typbasetype
  left outer join  pg_constraint c
    on c.contypid = t.oid
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = t.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_type'
where   t.typisdefined
  and   n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
  and   t.typtype = 'd'
order by n.nspname, t.typname;




