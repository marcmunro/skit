-- List all non-ri triggers in non-system schemas.

select t.tgname as name,
       c.relname as table,
       n.nspname as schema,
       t.tgfoid as fn_oid,
       t.tgenabled as enabled,
       t.tgdeferrable as deferrable,
       t.tginitdeferred as initially_deferred,
       t.tgisconstraint as is_constraint_trigger,
       pg_catalog.pg_get_triggerdef(t.oid) as definition,
       quote_literal(obj_description(t.oid, 'pg_trigger')) as comment
  from pg_catalog.pg_trigger t
 inner join pg_catalog.pg_class c
    on c.oid = t.tgrelid
 inner join pg_catalog.pg_namespace n 
    on n.oid = c.relnamespace
 where not exists (
    select null 
      from pg_catalog.pg_constraint cn
     where cn.oid = t.tgconstraint)
   and n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
order by 1, 2;
