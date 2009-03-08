#define VERSION_QRY			     \
    "(\"\n\n"				     \
    "select substring(pg_catalog.version() " \
    "from E'\\\\([0-9]+\\\\.[0-9]+\\\\"	     \
    "(\\\\.[0-9]+\\\\)?\\\\)') \n"	     \
    "	as version;\n\""		     \
    "[ 'version'] [[ '8.3.6']])"
