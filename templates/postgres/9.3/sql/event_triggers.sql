select et.evtname as name,
       et.evtevent as event_name,
       o.rolname as owner,
       et.evtfoid as fn_oid,
       et.evtenabled as enabled,
       regexp_replace(
           regexp_replace(et.evttags::text, '^{(.*)}$', '\1'),
	   '"', '''', 'g') as event_tags,
       quote_literal(obj_description(et.oid, 'pg_event_trigger')) as comment,
       x.extname as extension
  from pg_catalog.pg_event_trigger et
 inner join pg_catalog.pg_authid o
    on o.oid = et.evtowner  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = et.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_event_trigger'
