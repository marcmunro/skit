# Drop database created by a depdiffs sql script.
#

psql -d postgres <<'CLUSTEREOF'

drop database regressdb;
drop role r1;
drop role rs;

CLUSTEREOF