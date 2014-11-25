select p.prsname as name,
       n.nspname as schema,
       s.proname as start_proc,
       ns.nspname as start_schema,
       t.proname as token_proc,
       nt.nspname as token_schema,
       e.proname as end_proc,
       ne.nspname as end_schema,
       h.proname as headline_proc,
       nh.nspname as headline_schema,
       l.proname as lextype_proc,
       nl.nspname as lextype_schema,
       quote_literal(obj_description(p.oid, 'pg_ts_parser')) as comment
  from pg_catalog.pg_ts_parser p
 inner join pg_catalog.pg_namespace n
    on n.oid = p.prsnamespace
 inner join pg_catalog.pg_proc s
    on s.oid = p.prsstart
 inner join pg_catalog.pg_namespace ns
    on ns.oid = s.pronamespace
 inner join pg_catalog.pg_proc e
    on e.oid = p.prsend
 inner join pg_catalog.pg_namespace ne
    on ne.oid = e.pronamespace
 inner join pg_catalog.pg_proc t
    on t.oid = p.prstoken
 inner join pg_catalog.pg_namespace nt
    on nt.oid = t.pronamespace
 inner join pg_catalog.pg_proc l
    on l.oid = p.prslextype
 inner join pg_catalog.pg_namespace nl
    on nl.oid = l.pronamespace
  left outer join pg_catalog.pg_proc h
    on h.oid = p.prsheadline
  left outer join pg_catalog.pg_namespace nh
    on nh.oid = h.pronamespace
 order by n.nspname, p.prsname;
