# Test data for deps:diffdeps test.  This should be converted into an 
# xml file.
# 
#- See comments in depdiffs_1a.sql for a description of the test cases.

psql -d postgres -v home=`pwd` <<'CLUSTEREOF'
create role r1 with login;
create role rs with superuser;

create database test_data with owner r1;

CLUSTEREOF

psql -d test_data -U r1 << 'EOF'

-- Test case 1
create schema s1;
comment on schema s1 is 's1';
create sequence s1.t1;
comment on sequence s1.t1 is 't1';
create sequence s1.t1b;




EOF