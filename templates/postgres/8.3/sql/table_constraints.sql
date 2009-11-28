-- List all pk, uk and fk constraints
select c.oid as oid,
       c.conname as name,
       n.nspname as schema,
       case c.contype when 'p' then 'primary key' 
       when 'u' then 'unique' 
       when 'f' then 'foreign key' else 'unknown' end as constraint_type,
       c.condeferrable as deferrable,
       c.condeferred as deferred,
       regexp_replace(c.conkey::text, '{(.*)}', E'\\1') as columns,
       regexp_replace(c.confkey::text, '{(.*)}', E'\\1') as refcolumns,
       pg_catalog.pg_get_constraintdef(c.oid) as source,
       regexp_replace(cl.reloptions::text, '{(.*)}', E'\\1') as options,
       case t.spcname when 'pg_default' then null 
       else t.spcname end as tablespace, 
       am.amname as access_method, au.rolname as owner,
       cr.oid as refoid, cr.relname as reftable, nr.nspname as refschema,
       idxclass.relname as refindexname,
       idxns.nspname as refindexschema,
       idxcons.conname as refconstraintname,
       case c.contype when 'f' then c.confmatchtype 
       else null end as confmatchtype,
       case c.contype when 'f' then c.confupdtype 
       else null end as confupdtype,
       case c.contype when 'f' then c.confdeltype 
       else null end as confdeltype,
       quote_literal(obj_description(c.oid, 'pg_constraint')) as comment
from   pg_catalog.pg_constraint c
inner join  pg_catalog.pg_namespace n   -- Schema of constraint
  on  n.oid = c.connamespace
inner join pg_catalog.pg_namespace cat  -- pg_catalog schema
  on cat.nspname = 'pg_catalog'
inner join pg_catalog.pg_class cclass   -- pg_class relation
  on  cclass.relname = 'pg_class'
  and cclass.relnamespace = cat.oid
inner join pg_catalog.pg_class ccons    -- pg_constraint relation
  on  ccons.relname = 'pg_constraint'	
  and ccons.relnamespace = cat.oid
left outer join pg_catalog.pg_depend d  -- dependency on index relation
  on  d.refobjid = c.oid                -- (in order to get index tablespace)
  and d.classid = cclass.oid
  and d.refclassid = ccons.oid
left outer join pg_catalog.pg_class cl  -- index relation
  on cl.oid = d.objid
left outer join pg_catalog.pg_tablespace t  -- tablespace for index
  on t.oid = cl.reltablespace
left outer join pg_catalog.pg_am am         -- access method for index
  on am.oid = cl.relam
left outer join pg_catalog.pg_authid au	    -- owner
  on au.oid = cl.relowner
left outer join (pg_catalog.pg_class cr     -- referenced table for fk
   inner join pg_catalog.pg_namespace nr    -- schema of ref table for fk
     on  nr.oid = cr.relnamespace
   )
  on cr.oid = c.confrelid
left outer join (pg_catalog.pg_depend dr    -- dependency of fk back to index
   inner join pg_catalog.pg_class idxclass  -- class entry for referenced idx
     on idxclass.oid = dr.refobjid
   inner join pg_catalog.pg_namespace idxns -- schema for referenced idx
     on idxns.oid = idxclass.relnamespace
   left outer join pg_catalog.pg_depend idxdep
     on  idxdep.objid = idxclass.oid
   left outer join pg_catalog.pg_constraint idxcons
     on  idxcons.oid = idxdep.refobjid
   )
  on  dr.objid = c.oid
  and dr.classid = ccons.oid
  and dr.refclassid = cclass.oid
  and idxclass.relkind = 'i'
  and idxdep.classid = cclass.oid
  and idxdep.refclassid = ccons.oid
where   c.conrelid = :1
--where c.conrelid in (645662, 645683)
order by c.contype, n.nspname, c.conname;
