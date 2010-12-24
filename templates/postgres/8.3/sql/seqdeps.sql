-- Identify sequence dependencies for a table
-- TODO: This is going to be called once for each table.  We should
-- optimise this so that the query only runs once.  In order to do that,
-- we have to be able to have multiple records keyed by a single
-- table_oid, which is not doable with the current implementation of
-- hash keys for runsql.
select distinct 
       attrdep.refobjid as table_oid,
       seqdep.refobjid as sequence_oid,
       seqclass.relname as sequence_name,
       n.nspname as sequence_schema
from   pg_catalog.pg_depend attrdep
inner join 
       pg_catalog.pg_class sclass
   on  sclass.oid = attrdep.classid
   and sclass.relname = 'pg_attrdef' 
inner join
       pg_catalog.pg_depend seqdep
   on  seqdep.objid = attrdep.objid
   and seqdep.refobjid != attrdep.refobjid
   and seqdep.refclassid = attrdep.refclassid
inner join
       pg_catalog.pg_class seqclass
   on  seqclass.oid = seqdep.refobjid
   and seqclass.relkind = 'S'
inner join
       pg_catalog.pg_namespace n
   on  n.oid = seqclass.relnamespace
where  attrdep.refobjid = :1;



