# Drop database regressdb and all roles.  Be sure not to run this on any
# database cluster you care about. 
#

(
    echo "drop database regressdb;" 
    psql -d postgres -q -t -c \
        "select 'drop role ' || rolname || ';' from pg_roles" | \
        grep -v "drop role `whoami`;"
) | psql -d postgres
