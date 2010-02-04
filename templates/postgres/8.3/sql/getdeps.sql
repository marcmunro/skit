-- Identify dependencies
select cproc.relname as reltype, d.refobjid as objoid
from   pg_catalog.pg_depend d
inner join pg_catalog.pg_class cclass   -- source relation
  on  cclass.relname = ':2'
  and cclass.oid = d.classid
inner join pg_catalog.pg_class cproc   -- target relations
  on  cproc.relname in :3
  and cproc.oid = d.refclassid
where objid = :1;

