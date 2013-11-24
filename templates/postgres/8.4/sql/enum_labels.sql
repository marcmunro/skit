-- Get label values for an enum
select quote_literal(enumlabel) as label
from   pg_catalog.pg_enum
where  enumtypid = :1
order by oid;
