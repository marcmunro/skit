-- Get label values for an enum
select quote_literal(enumlabel) as label,
       row_number() over (order by oid) as seq_no
from   pg_catalog.pg_enum
where  enumtypid = :1
order by oid;
