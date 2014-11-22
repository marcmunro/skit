-- List all text search configuration mappings 
select m.mapcfg as config_oid,
       t.alias as name,
       m.maptokentype as token_id,
       m.mapseqno as seqno,
       d.dictname as dictionary_name,
       n.nspname as dictionary_schema,
       o.rolname as dictionary_owner
  from pg_catalog.pg_ts_config_map m
 inner join pg_catalog.pg_ts_dict d
    on d.oid = m.mapdict
 inner join pg_catalog.pg_namespace n
     on (n.oid = d.dictnamespace)
 inner join pg_catalog.pg_authid o
     on (o.oid = d.dictowner)
 inner join pg_catalog.ts_token_type('3722'::pg_catalog.oid) t
    on t.tokid = m.maptokentype
 order by m.mapcfg, m.mapseqno, m.maptokentype;