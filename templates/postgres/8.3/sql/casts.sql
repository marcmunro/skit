-- List all casts
select case when sn.nspname = 'pg_catalog' then ''
       else sn.nspname || '.' end ||
       st.typname || ':' ||
       case when tn.nspname = 'pg_catalog' then ''
       else tn.nspname || '.' end ||
       tt.typname as name,
       st.typname as source_type,
       sn.nspname as source_type_schema,
       tt.typname as target_type,
       tn.nspname as target_type_schema,
       c.castfunc::oid as fn_oid,
       c.castcontext as context
from   pg_catalog.pg_cast c
       inner join
          pg_catalog.pg_type st
       on (st.oid = c.castsource)
       inner join 
          pg_catalog.pg_namespace sn
       on (sn.oid = st.typnamespace)
       inner join
          pg_catalog.pg_type tt
       on (tt.oid = c.casttarget)
       inner join 
          pg_catalog.pg_namespace tn
       on (tn.oid = tt.typnamespace)
where  (   sn.nspname not in ('pg_catalog', 'information_schema')
        or tn.nspname not in ('pg_catalog', 'information_schema'))
order by sn.nspname, st.typname, tn.nspname, tt.typname;
