-- List all text search configuration map entries
select m.mapcfg as config_oid,
       t.alias as name,
       m.maptokentype as token_id
  from pg_catalog.pg_ts_config_map m
 inner join pg_catalog.ts_token_type('3722'::pg_catalog.oid) t
    on t.tokid = m.maptokentype
 group by m.mapcfg, m.maptokentype, t.alias
 order by m.mapcfg, m.maptokentype;