# Test data for deps:diffdeps test.  This should be converted into an 
# xml file.
# 
#- See comments in depdiffs_1a.sql for a description of the test cases.

psql -d postgres -v home=`pwd` <<'CLUSTEREOF'
create role r1 with login;
create role rs with superuser;

create database regressdb with owner r1;

CLUSTEREOF

psql -d regressdb -U r1 << 'EOF'

-- Test case 1
create schema n1;
comment on schema n1 is 'n1';
create sequence n1.s1;
comment on sequence n1.s1 is 's1';
create sequence n1.s1b;

-- Test case 2
create schema n2;
comment on schema n2 is 'n2';
create sequence n2.s2;
comment on sequence n2.s2 is 's2';
revoke usage on schema n2 from r1;
reset session authorization;

-- Test case 3
-- No ownership changes.  Schema owner loses usage priv on schema, sequence
-- owned by schema owner, public usage priv on schema.
create schema n3;
create sequence n3.s3;
comment on sequence n3.s3 is 's3';
grant usage on schema n3 to public;
revoke usage on schema n3 from r1;

EOF