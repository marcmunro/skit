#define DATABASE_QRY							\
    "(\""								\
    "select d.datname as name,"						\
    "       user as username,"						\
    "       r.rolname as owner,"					\
    "       pg_catalog.pg_encoding_to_char(d.encoding) as encoding,"	\
    "       t.spcname as tablespace,"					\
    "       d.datconnlimit as connections,"				\
    "       quote_literal(obj_description(d.oid, 'pg_database')) "	\
    "as comment,"							\
    "       regexp_replace(d.datacl::text, '.(.*).', E'\\\\\\\\1') as privs" \
    "from   pg_catalog.pg_database d"					\
    "inner join pg_catalog.pg_roles r"					\
    "        on d.datdba = r.oid"					\
    "  inner join pg_catalog.pg_tablespace t"				\
    "          on t.oid = d.dattablespace"				\
    "where d.datname = pg_catalog.current_database();\""		\
    "[ 'name' 'username' 'owner' 'encoding' 'tablespace' "		\
    "'connections' 'comment' 'privs'] "					\
    "[[ 'skittest' 'marc' 'regress' 'UTF8' 'tbs3' '-1' "		\
    "nil '=Tc/regress,regress=CTc/regress']])"
