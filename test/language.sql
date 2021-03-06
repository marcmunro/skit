#define LANGUAGE_QRY							\
    "(\""								\
    "select l.lanname as name,   "					\
    "       case when l.lanpltrusted then 'yes' else 'no' end as trusted,"  \
    "       hn.nspname as handler_schema,"				\
    "       h.proname as handler_function,"				\
    "       hv.nspname as validator_schema,"				\
    "       v.proname as validator_function,"				\
    "       l.lanacl as privs,"						\
    "       quote_literal(obj_description(l.oid, 'pg_language')) as comment"  \
    "from   pg_catalog.pg_language l "					\
    "       inner join "						\
    "          (pg_catalog.pg_proc h "					\
    "           inner join "						\
    "              pg_catalog.pg_namespace hn"				\
    "	   on (hn.oid = h.pronamespace)"				\
    "          )"							\
    "       on  (h.oid = l.lanplcallfoid)"				\
    "       left outer join "						\
    "          (pg_catalog.pg_proc v "					\
    "           inner join "						\
    "              pg_catalog.pg_namespace hv"				\
    "	   on (hv.oid = v.pronamespace)"				\
    "          )"							\
    "       on  (v.oid = l.lanvalidator)"				\
    "where  l.lanispl;\""						\
    "[ 'name' 'trusted' 'handler_schema' 'handler_function'		\
       'validator_schema' 'validator_function' 'privs' 'comment']"	\
    "[[ 'plpgsql' 'yes' 'pg_catalog' 'plpgsql_call_handler'		\
        'pg_catalog' 'plpgsql_validator'				\
	' {=U/marc,marc=U/marc,keep2=U*/marc,keep=U/marc,wibble=U/marc}' \
	\"'Procedural language'\"]])"
