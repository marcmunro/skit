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
       p.pronargdefaults as default_args_count,
       pg_get_expr(p.proargdefaults, 0) as all_arg_defaults,
       case when p.proiswindow then 'yes' else null end as is_window_fn,
       case when p.prosecdef then 'yes' else null end as security_definer,
       case when p.proisstrict then 'yes' else null end as is_strict,
       case when p.proretset then 'yes' else null end as returns_set,
       case p.provolatile when 'i' then 'immutable' 
	    when 's' then 'stable' else 'volatile' end as volatility,
       p.proacl::text as privs,
       quote_literal(obj_description(p.oid, 'pg_proc')) as comment,
       p.prosrc as source,
       nullif(p.probin, '-') as bin,
       case when r.rngtypid is not null 
           then true else null end as is_canonical_for_range,
       case when ts.oid is not null 
           then true else null end as is_for_shell_type,
       ts.typname as shell_type_name,
       tsn.nspname as shell_type_schema,
       coalesce(r.rngtypid::oid, t.oid::oid) as typoid,
       t.typinput::oid as type_input_oid,
       t.typoutput::oid as type_output_oid,
       t.typreceive::oid as type_receive_oid,
       t.typsend::oid as type_send_oid,
       case when p.proretset then p.prorows else null end as rows,
       regexp_replace(p.proconfig::varchar, E'{\(.*\)}', E'\\1') 
         as all_config_settings,
       x.extname as extension
  from pg_catalog.pg_proc p
 inner join pg_catalog.pg_namespace n
    on n.oid = p.pronamespace
 inner join pg_catalog.pg_authid o
    on o.oid = p.proowner
 inner join pg_catalog.pg_language l
    on l.oid = p.prolang
  left outer join  pg_catalog.pg_type t
    on (   t.typinput = p.oid
        or t.typoutput = p.oid
        or t.typreceive = p.oid
        or t.typsend = p.oid
        or t.typanalyze = p.oid)
   and t.typtype = 'b'
  left outer join pg_catalog.pg_range r
    on r.rngcanonical = p.oid
  left outer join (pg_catalog.pg_type ts
          inner join pg_catalog.pg_namespace tsn
	     on tsn.oid = ts.typnamespace)
    on ts.oid = p.prorettype
   and ts.typtype = 'p'
   and not ts.typisdefined
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = p.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_proc'
  left outer join (pg_catalog.pg_depend drt -- dependency of range-type
          inner join pg_catalog.pg_class crt
             on crt.oid = drt.refclassid
	    and crt.relname = 'pg_type'
	   inner join pg_catalog.pg_type rt
	      on rt.oid = drt.refobjid
	     and rt.typtype in ('p', 'r'))
    on drt.objid = p.oid   
   and drt.deptype = 'i'
   and l.lanname = 'internal'
 where not p.proisagg
   and n.nspname not in ('pg_catalog', 'information_schema')
   and rt.typname is null         -- Ignore internal functions for range types
 order by n.nspname, p.proname;

