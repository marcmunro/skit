-- List all functions: 
select p.oid as oid,
       p.proname as name,
       n.nspname as schema,
       o.rolname as owner,
       l.lanname as language,
       p.prorettype as result_type_oid,
       p.procost as cost,
       regexp_replace(p.proargtypes::varchar, ' ', ',', 'g') as sigargtypes,
       case when p.proallargtypes is null then
         regexp_replace(p.proargtypes::varchar, ' ', ',', 'g') 
       else
         regexp_replace(p.proallargtypes::varchar, E'{\(.*\)}', E'\\1') 
       end as argtype_oids,
       regexp_replace(p.proargmodes::varchar, E'{\(.*\)}', E'\\1') 
         as all_argmodes,
       regexp_replace(p.proargnames::varchar, E'{\(.*\)}', E'\\1') 
         as all_argnames,
       case when p.prosecdef then 'yes' else null end as security_definer,
       case when p.proisstrict then 'yes' else null end as is_strict,
       case when p.proretset then 'yes' else null end as returns_set,
       case p.provolatile when 'i' then 'immutable' 
	    when 's' then 'stable' else 'volatile' end as volatility,
       p.proacl as privs,
       quote_literal(obj_description(p.oid, 'pg_proc')) as comment,
       p.prosrc as source,
       nullif(p.probin, '-') as bin,
       t.oid::oid as typoid,
       t.typinput::oid as type_input_oid,
       t.typoutput::oid as type_output_oid,
       t.typreceive::oid as type_receive_oid,
       t.typsend::oid as type_send_oid
from   pg_catalog.pg_proc p
       inner join
          pg_catalog.pg_namespace n
       on (n.oid = p.pronamespace)
       inner join
          pg_catalog.pg_authid o
       on (o.oid = p.proowner)
       inner join
          pg_catalog.pg_language l
       on (l.oid = p.prolang)
       left outer join
          pg_catalog.pg_type t
       on (   t.typinput = p.oid
           or t.typoutput = p.oid
           or t.typreceive = p.oid
	   or t.typsend = p.oid
           or t.typanalyze = p.oid)
       and t.typtype = 'b'
where  not p.proisagg
and    n.nspname not in ('pg_catalog', 'information_schema')
order by n.nspname, p.proname;

