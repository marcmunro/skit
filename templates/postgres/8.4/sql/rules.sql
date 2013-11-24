-- List all rules

select rw.oid as oid,
       rw.rulename as name,
       t.relname as table,
       n.nspname as schema,
       pg_get_ruledef(rw.oid) as definition,
       r.rolname as table_owner,
       quote_literal(obj_description(rw.oid, 'pg_rewrite')) as comment
  from pg_rewrite rw
inner join 
       pg_class t
    on t.oid = rw.ev_class
inner join pg_roles r
    on r.oid = t.relowner
left outer join 
       pg_namespace n
    on n.oid = t.relnamespace
 where rw.rulename <> '_RETURN'::name
   and n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
order by 1, 2;

