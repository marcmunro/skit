select distinct cproc.relname as reltype, d.refobjid as objoid, 
       d.refobjsubid as colnum
from   pg_catalog.pg_depend d
inner join pg_catalog.pg_class cclass   -- source relation
  on  cclass.relname = 'pg_class'
  and cclass.oid = d.classid
inner join pg_catalog.pg_class cproc   -- target relations
  on  cproc.relname in ('pg_class')
  and cproc.oid = d.refclassid
where d.objid = :1
  and d.refobjsubid != 0;