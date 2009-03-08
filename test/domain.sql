#define DOMAIN_QRY							\
    "(\""								\
    "select t.typname as name,"						\
    "       n.nspname as schema,"					\
    "       o.rolname as owner,"					\
    "       c.oid as conoid,"						\
    "       t2.typname as basetype,"					\
    "       nt2.nspname as basetype_schema,"				\
    "       t.typdefault as default,"					\
    "       case when t.typnotnull then 'no' else 'yes' end as nullable," \
    "       quote_literal(obj_description(t.oid, 'pg_type')) as comment" \
    "from   pg_catalog.pg_type t"					\
    "       inner join"							\
    "          pg_catalog.pg_namespace n"				\
    "       on (n.oid = t.typnamespace)"				\
    "       inner join"							\
    "          pg_catalog.pg_authid o"					\
    "       on (o.oid = t.typowner)"					\
    "       left outer join "						\
    "	  (pg_catalog.pg_type t2"					\
    "	   inner join"							\
    "	       pg_catalog.pg_namespace nt2"				\
    "	   on (nt2.oid = t2.typnamespace)"				\
    "	  )"								\
    "	on (t2.oid = t.typbasetype)"					\
    "        left outer join"						\
    "	   pg_constraint c"						\
    "	on (c.contypid = t.oid)"					\
    "where   t.typisdefined"						\
    "  and   n.nspname not in ('pg_catalog', 'pg_toast', 'information_schema')"	\
    "  and   t.typtype = 'd'"						\
    "order by n.nspname, t.typname;\""					\
    "[ 'name' 'schema' 'owner' 'conoid' 'basetype' "			\
    "'basetype_schema' 'default' 'nullable' 'comment']"			\
    "[[ 'postal2' 'public' 'marc' nil 'mychar' 'public' nil 'yes' nil]"	\
    "[ 'postal3' 'public' 'marc' nil 'mychar' 'public' \"'x'::mychar\"" \
    " 'no' \"'wibble'\"]"						\
    "[ 'us_postal_code' 'public' 'marc' '16427' 'text' "		\
    "'pg_catalog' nil 'yes' nil]])"
