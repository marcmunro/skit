with catalogs as (
  select c.relname as catname, c.oid,
         case c.relname 
	 when 'pg_language' then 'language'
	 when 'pg_type' then 'type'
	 when 'pg_proc' then 'function'
	 when 'pg_operator' then 'operator'
	 when 'pg_opfamily' then 'operator_family'
	 when 'pg_opclass' then 'operator_class'
	 else 'unhandled: ' || c.relname end as objtype
    from pg_namespace n
   inner join pg_catalog.pg_class c
      on c.relnamespace = n.oid
   where n.nspname = 'pg_catalog'
     --and c.relname in ('pg_language', 'pg_extension')
)
select e.oid as extension_oid,
       e.extname as extension_name,
       refcat.objtype as type,
       coalesce(l.lanname, t.typname, o.oprname, 
                p.proname, of.opfname, oc.opcname,
		'unknown') as name,
       coalesce(tn.nspname, pn.nspname) as schema,
       d.objid as oid
  from pg_catalog.pg_extension e
 inner join catalogs cat
    on cat.catname = 'pg_extension'
 inner join pg_catalog.pg_depend d
    on d.refobjid = e.oid
   and d.refclassid = cat.oid
   and d.deptype = 'e'
 inner join catalogs refcat
    on refcat.oid = d.classid
  left outer join pg_catalog.pg_language l
    on l.oid = d.objid
   and refcat.catname = 'pg_language'
  left outer join pg_catalog.pg_operator o
    on o.oid = d.objid
   and refcat.catname = 'pg_operator'
  left outer join pg_catalog.pg_opfamily of
    on of.oid = d.objid
   and refcat.catname = 'pg_opfamily'
  left outer join pg_catalog.pg_opclass oc
    on oc.oid = d.objid
   and refcat.catname = 'pg_opclass'
  left outer join (
           pg_catalog.pg_type t
     inner join pg_catalog.pg_namespace tn
        on tn.oid = t.typnamespace)
    on t.oid = d.objid
   and refcat.catname = 'pg_type'
  left outer join (
           pg_catalog.pg_proc p
     inner join pg_catalog.pg_namespace pn
        on pn.oid = p.pronamespace)
    on p.oid = d.objid
   and refcat.catname = 'pg_proc'