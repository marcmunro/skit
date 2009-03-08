#define CONSTRAINT_QRY							\
    "(\""								\
    "select c.oid as oid,"						\
    "       c.conname as name,"						\
    "       n.nspname as schema,"					\
    "       c.contype as constraint_type,"				\
    "       pg_catalog.pg_get_constraintdef(c.oid) as source"		\
    "from   pg_catalog.pg_constraint c"					\
    "       inner join"							\
    "          pg_catalog.pg_namespace n"				\
    "       on (n.oid = c.connamespace)"				\
    "where   c.contype = 'c'"						\
    "  and   n.nspname not in ('pg_catalog', 'pg_toast', "		\
    "'information_schema')"						\
    "order by n.nspname, c.conname;\""					\
    "[ 'oid' 'name' 'schema' 'constraint_type' 'source']"		\
    "[[ '16427' 'us_postal_code_check' 'public' 'c' "			\
    "\"CHECK (((VALUE ~ '^\\\\\\\\d{4}$'::text) OR "			\
    "(VALUE ~ '^\\\\\\\\d{5}-\\\\\\\\d{4}$'::text)))\"]])"
