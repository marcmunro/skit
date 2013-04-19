-- Get details of installed procedural languages
select l.lanname as name,
       case when l.lanpltrusted then 'yes' else 'no' end as trusted,
       hn.nspname as handler_schema,
       h.proname as handler_function,
       hv.nspname as validator_schema,
       v.proname as validator_function,
       l.lanacl as privs,
       o.rolname as owner,
       quote_literal(obj_description(l.oid, 'pg_language')) as comment,
       x.extname as extension
  from pg_catalog.pg_language l 
 inner join pg_catalog.pg_authid o
    on (o.oid = l.lanowner)
 inner join (pg_catalog.pg_proc h 
         inner join pg_catalog.pg_namespace hn
  	    on (hn.oid = h.pronamespace)
                )
    on  (h.oid = l.lanplcallfoid)
   left outer join (pg_catalog.pg_proc v 
          inner join  pg_catalog.pg_namespace hv
	     on (hv.oid = v.pronamespace)
                   )
     on  (v.oid = l.lanvalidator)
   left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
     on dx.objid = l.oid
    and dx.deptype = 'e'
    and cx.relname = 'pg_language'
  where l.lanispl;

