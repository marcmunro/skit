-- Get details of installed procedural languages
select l.lanname as name,
       case when l.lanpltrusted then 'yes' else 'no' end as trusted,
       hn.nspname as handler_schema,
       h.proname as handler_function,
       hv.nspname as validator_schema,
       v.proname as validator_function,
       -- Explicitly show default privs if they are not given.
       -- Defaults are usage to public and to owner.
       case when l.lanacl is null 
       then '{=U/' || o.rolname || ',' || o.rolname || 
              '=U/' || o.rolname || '}'
       else l.lanacl::text end as privs,
       o.rolname as owner,
       quote_literal(obj_description(l.oid, 'pg_language')) as comment
from   pg_catalog.pg_language l 
       inner join
          pg_catalog.pg_authid o
       on (o.oid = l.lanowner)
       inner join 
          (pg_catalog.pg_proc h 
           inner join 
              pg_catalog.pg_namespace hn
	   on (hn.oid = h.pronamespace)
          )
       on  (h.oid = l.lanplcallfoid)
       left outer join 
          (pg_catalog.pg_proc v 
           inner join 
              pg_catalog.pg_namespace hv
	   on (hv.oid = v.pronamespace)
          )
       on  (v.oid = l.lanvalidator)
where  l.lanispl;
