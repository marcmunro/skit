#define USERS_QRY							\
    "(\""								\
    "select a.rolname as name,"						\
    "       case when a.rolsuper then 'y' else 'n' end as superuser,"	\
    "       case when a.rolinherit then 'y' else 'n' end as inherit,"	\
    "       case when a.rolcreaterole then 'y' else 'n' end as createrole," \
    "       case when a.rolcreatedb then 'y' else 'n' end as createdb,"	\
    "       case when a.rolcanlogin then 'y' else 'n' end as login,"	\
    "       a.rolconnlimit as max_connections,"				\
    "       a.rolpassword as password,"					\
    "       a.rolvaliduntil as expires,"				\
    "       a.rolconfig as config"					\
    "from   pg_catalog.pg_authid a"					\
    "order by a.rolname;\""						\
    "[ 'name' 'superuser' 'inherit' 'createrole' 'createdb' "		\
    "'login' 'max_connections' 'password' 'expires' 'config']"		\
    "[[ 'keep' 'n' 'n' 'n' 'n' 'y' '-1' "				\
    "'md5a6e3dfe729e3efdf117eeb1059051f77' nil nil]"			\
    "[ 'keep2' 'n' 'n' 'n' 'n' 'y' '-1' "				\
    "'md5dd9b387fa54744451a97dc9674f6aba2' nil nil]"			\
    "[ 'lose' 'n' 'n' 'n' 'n' 'y' '-1' '"				\
    "md5c62bc3e38bac4209132682f13509ba96' nil nil]"			\
    "[ 'marc' 'y' 'y' 'y' 'y' 'y' '-1' "				\
    "'md5c62bc3e38bac4209132682f13509ba96' nil '"			\
    "{client_min_messages=error}']"					\
    "[ 'marco' 'n' 'n' 'n' 'n' 'y' '-1' "				\
    "'md54ea9ea89bc47825ea7b2fe7c2288b27a' nil nil]"			\
    "[ 'regress' 'y' 'n' 'n' 'n' 'y' '-1' "\
    "'md5c2a101703f1e515ef9769f835d6fe78a' 'infinity' "			\
    "'{client_min_messages=warning}']"					\
    "[ 'wibble' 'n' 'n' 'n' 'n' 'y' '-1' "\
    "'md54ea9ea89bc47825ea7b2fe7c2288b27a' '2007-03-01 00:00:00-08' nil]])"
