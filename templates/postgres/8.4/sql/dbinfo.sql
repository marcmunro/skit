-- Get basic database info for the current database
select d.datname as name,
       user as username,
       r.rolname as owner,
       pg_catalog.pg_encoding_to_char(d.encoding) as encoding,
       t.spcname as tablespace,
       d.datconnlimit as connections,
       quote_literal(shobj_description(d.oid, 'pg_database')) as comment,
       -- Explicitly show default privs if they are not given.
       -- Defaults are temp, create and connect to owner and
       -- temp and connect to public.
       case when d.datacl is null 
       then '{=Tc/' || r.rolname || ',' || r.rolname || 
              '=TCc/' || r.rolname || '}'
       else d.datacl::text end as privs
from   pg_catalog.pg_database d
inner join pg_catalog.pg_roles r 
        on d.datdba = r.oid
  inner join pg_catalog.pg_tablespace t
          on t.oid = d.dattablespace
where d.datname = pg_catalog.current_database();

