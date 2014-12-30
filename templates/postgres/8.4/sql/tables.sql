-- List all tables in non-system schemas.

select c.oid::oid as oid,
       n.nspname as schema,
       c.relname as name,
       r.rolname as owner,
       case when c.relhasoids then 't' else null end as with_oids,
       case when t.spcname is null then td.spcname
       else t.spcname end as tablespace, 
       t.spcname is null as tablespace_is_default,
       c.relacl::text as privs,
       regexp_replace(c.reloptions::text, '{(.*)}', E'\\1') as options,
       quote_literal(obj_description(c.oid, 'pg_class')) as comment
from   pg_catalog.pg_class c
    inner join pg_catalog.pg_database d
        on d.datname = current_database()
    inner join pg_catalog.pg_roles r 
        on  r.oid = c.relowner
    inner join pg_catalog.pg_namespace n 
        on n.oid = c.relnamespace
    inner join pg_catalog.pg_tablespace td
          on td.oid = d.dattablespace
    left outer join pg_catalog.pg_tablespace t
         on t.oid = c.reltablespace
where  c.relkind = 'r'
and    n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')
order by 1, 2;
