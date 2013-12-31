-- List all operator family functions.
select opf.oid as family_oid,
       d.refobjid as class_oid,
       ap.amprocnum as proc_num,
       ap.amproc::oid as proc_oid,
       p.proname as name,
       np.nspname as schema,
       case when p.proallargtypes is null then
         regexp_replace(p.proargtypes::varchar, ' ', ',', 'g') 
       else
         regexp_replace(p.proallargtypes::varchar, E'{\(.*\)}', E'\\1') 
       end as argtype_oids
from   pg_catalog.pg_amproc ap
inner join pg_catalog.pg_opfamily opf
  on  opf.oid = ap.amprocfamily
inner join pg_catalog.pg_namespace nf
  on nf.oid = opf.opfnamespace
inner join pg_catalog.pg_proc p
  on p.oid = ap.amproc
inner join pg_catalog.pg_namespace np
  on np.oid = p.pronamespace
left outer join (pg_catalog.pg_depend d
   inner join pg_catalog.pg_class dn
     on dn.oid = d.classid 
   inner join pg_catalog.pg_class rn
     on rn.oid = d.refclassid 
   )
  on  d.objid = ap.oid
  and rn.relname = 'pg_opclass'
  and dn.relname = 'pg_amproc'
where (   np.nspname != 'pg_catalog'
       or nf.nspname != 'pg_catalog');

