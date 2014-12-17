-- List all non-empty, non-system schemata.
select n.nspname as name,
       s.rolname as owner,
       n.nspacl::text as privs,
       quote_literal(obj_description(n.oid, 'pg_namespace')) as comment,
       x.extname as extension
  from pg_catalog.pg_namespace n
 inner join pg_catalog.pg_authid s
    on s.oid = n.nspowner
  left outer join (pg_catalog.pg_depend dx -- dependency on extension
          inner join pg_catalog.pg_extension x
	     on x.oid = dx.refobjid
          inner join pg_catalog.pg_class cx
             on cx.oid = dx.classid)
    on dx.objid = n.oid
   and dx.deptype = 'e'
   and cx.relname = 'pg_namespace'
where  n.nspname NOT IN ('pg_catalog', 'pg_toast', 
			 'information_schema')
and    n.nspname !~ '^pg_temp_'
and    n.nspname !~ '^pg_toast_'
order by s.rolname;
