#define SCHEMA_QRY							\
    "(\""								\
    "select n.nspname as name,"						\
    "       s.rolname as owner,"					\
    "       n.nspacl as privs,"						\
    "       quote_literal(obj_description(n.oid, 'pg_namespace')) as comment"\
    "from   pg_catalog.pg_namespace n"					\
    "       inner join pg_catalog.pg_authid s"				\
    "               on s.oid = n.nspowner"				\
    "where  n.nspname NOT IN ('pg_catalog', 'pg_toast', "		\
    "			 'information_schema')"				\
    "and    n.nspname !~ '^pg_temp_'"					\
    "order by s.rolname;\""						\
    "[ 'name' 'owner' 'privs' 'comment']"				\
    "[[ 'pg_toast_temp_1' 'marc' nil nil]"				\
    "[ 'public' 'marc' '{marc=UC/marc,regress=UC/marc,=UC/marc}' "	\
    "''standard public schema'']"					\
    "[ 'schema1' 'regress' '{regress=UC/regress,keep=U/regress}' nil]"	\
    "[ 'schema2' 'regress' '{regress=UC/regress,keep2="			\
    "U*C/regress,wibble=U*C*/regress,keep=U/regress,=U/regress}' "	\
    "''wibble'']"							\
    "[ 'schema3' 'regress' '{regress=UC/regress,keep2=UC/regress}' nil]])"
