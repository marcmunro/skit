# Drop database created by a depdiffs sql script.
#

psql -d postgres <<'CLUSTEREOF'

drop database test_data;
drop role r1;
drop role rs;

CLUSTEREOF