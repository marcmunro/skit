-- List inheritence

select c.relname as inherit_table,
       n.nspname as inherit_schema,
       i.inhseqno as inherit_order
from   pg_catalog.pg_inherits i
    inner join pg_catalog.pg_class c
        on c.oid = i.inhparent
    inner join pg_catalog.pg_namespace n
        on n.oid = c.relnamespace
where  i.inhrelid = :1
order by i.inhseqno;
