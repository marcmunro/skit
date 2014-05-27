# Test data for deps:general_diffs test.  This should be converted into an 
# xml file.
#
# This is primarily responsible for testing each of the various
# transform types identified by transformForDep().
# 

psql -d postgres -v home=`pwd` <<'CLUSTEREOF'
create role r1 with login;

create database regressdb with owner r1;

CLUSTEREOF

psql -d regressdb -U r1 << 'EOF'

create language plpgsql;

-- Test case 1
-- A rebuild depends on a build and drop.

create or replace function "public"."addnint4"(
    _state in "pg_catalog"."int4",
    _next in "pg_catalog"."int4")
  returns "pg_catalog"."int4"
as 
$_$
begin
  return _state + _next;
end;
$_$
language plpgsql stable cost 5;

create aggregate "public"."mysum" (
  basetype = "pg_catalog"."int4",
  sfunc = "addnint4",
  stype = "pg_catalog"."int4",
  initcond = '0'
);


EOF