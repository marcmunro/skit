#define TABLESPACE_QRY							\
    "(\""								\
    "select t.spcname as name,"						\
    "       r.rolname as owner,"					\
    "       t.spclocation as location,"					\
    "       t.spcacl as privs"						\
    "from   pg_tablespace t"						\
    "inner join pg_catalog.pg_roles r"					\
    "   on  r.oid = t.spcowner"						\
    "where  t.spcname not in ('pg_default', 'pg_global')"		\
    "order by 1;\""							\
    "[ 'name' 'owner' 'location' 'privs']"				\
    "[[ 'tbs2' 'regress' '/home/marc/proj/skit2/test/pgdata/tbs/tbs2'"	\
    " '{regress=C/regress,keep2=C/regress}']"				\
    "[ 'tbs3' 'regress' '/home/marc/proj/skit2/test/pgdata/tbs/tbs3'"	\
    " '{regress=C/regress,wibble=C/regress,"				\
    "marco=C*/regress,keep2=C*/regress,keep=C*/regress}']"		\
    "[ 'tbs4' 'wibble' '/home/marc/proj/skit2/test/pgdata/tbs/tbs4'"	\
    " nil]])"
