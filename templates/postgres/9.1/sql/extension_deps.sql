with catalogs as (
  select c.relname as catname, c.oid,
         case c.relname 
	 when 'pg_language' then 'language'
	 when 'pg_type' then 'type'
	 when 'pg_proc' then 'function'
	 when 'pg_operator' then 'operator'
	 when 'pg_opfamily' then 'operator_family'
	 when 'pg_opclass' then 'operator_class'
	 when 'pg_class' then 'table'
	 when 'pg_cast' then 'cast'
	 when 'pg_conversion' then 'conversion'
	 when 'pg_namespace' then 'schema'
	 when 'pg_ts_config' then 'text search configuration'
	 when 'pg_ts_template' then 'text search template'
	 when 'pg_ts_dict' then 'text search dictionary'
	 when 'pg_ts_parser' then 'text search parser'
	 when 'pg_foreign_data_wrapper' then 'foreign data wrapper'
	 when 'pg_foreign_server' then 'foreign server'
	 when 'pg_user_mapping' then 'user mapping'
	 else 'unhandled: ' || c.relname end as objtype
    from pg_namespace n
   inner join pg_catalog.pg_class c
      on c.relnamespace = n.oid
   where n.nspname = 'pg_catalog'
)
select e.oid as extension_oid,
       e.extname as extension_name,
       refcat.objtype as type,
       coalesce(l.lanname, t.typname, o.oprname, 
                p.proname, of.opfname, oc.opcname,
		c.relname, st.typname || ' as ' || tt.typname,
		con.conname, n.nspname,
		tsc.cfgname, tst.tmplname,
		tsd.dictname, tsp.prsname,
		fdw.fdwname, fs.srvname,
		umr.rolname || ' for ' || umfs.srvname) as name,
       coalesce(tn.nspname, pn.nspname, cn.nspname,
                conn.nspname, tscn.nspname, tstn.nspname,
		tsdn.nspname, tspn.nspname) as schema,
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
           pg_catalog.pg_class c
     inner join pg_catalog.pg_namespace cn
        on cn.oid = c.relnamespace)
    on c.oid = d.objid
   and refcat.catname = 'pg_class'
  left outer join (
           pg_catalog.pg_proc p
     inner join pg_catalog.pg_namespace pn
        on pn.oid = p.pronamespace)
    on p.oid = d.objid
   and refcat.catname = 'pg_proc'
  left outer join (
           pg_catalog.pg_cast ca
     inner join pg_catalog.pg_type st
        on st.oid = ca.castsource
     inner join pg_catalog.pg_type tt
        on tt.oid = ca.casttarget)
    on ca.oid = d.objid
   and refcat.catname = 'pg_cast'
  left outer join (
           pg_catalog.pg_conversion con
     inner join pg_catalog.pg_namespace conn
        on conn.oid = con.connamespace)
    on con.oid = d.objid
   and refcat.catname = 'pg_conversion'
  left outer join pg_catalog.pg_namespace n
    on n.oid = d.objid
   and refcat.catname = 'pg_namespace'
  left outer join (
           pg_catalog.pg_ts_config tsc
     inner join pg_catalog.pg_namespace tscn
        on tscn.oid = tsc.cfgnamespace)
    on tsc.oid = d.objid
   and refcat.catname = 'pg_ts_config'
  left outer join (
           pg_catalog.pg_ts_template tst
     inner join pg_catalog.pg_namespace tstn
        on tstn.oid = tst.tmplnamespace)
    on tst.oid = d.objid
   and refcat.catname = 'pg_ts_template'
  left outer join (
           pg_catalog.pg_ts_dict tsd
     inner join pg_catalog.pg_namespace tsdn
        on tsdn.oid = tsd.dictnamespace)
    on tsd.oid = d.objid
   and refcat.catname = 'pg_ts_dict'
  left outer join (
           pg_catalog.pg_ts_parser tsp
     inner join pg_catalog.pg_namespace tspn
        on tspn.oid = tsp.prsnamespace)
    on tsp.oid = d.objid
   and refcat.catname = 'pg_ts_parser'
  left outer join pg_catalog.pg_foreign_data_wrapper fdw
    on fdw.oid = d.objid
   and refcat.catname = 'pg_foreign_data_wrapper'
  left outer join pg_catalog.pg_foreign_server fs
    on fs.oid = d.objid
   and refcat.catname = 'pg_foreign_server'
  left outer join (
           pg_catalog.pg_user_mapping um
     inner join pg_catalog.pg_roles umr
        on umr.oid = um.umuser
     inner join pg_catalog.pg_foreign_server umfs
        on umfs.oid = um.umserver)
    on um.oid = d.objid
   and refcat.catname = 'pg_user_mapping'
where e.extname = 'skit_test'
