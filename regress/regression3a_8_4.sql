
psql -d postgres -v home=`pwd` <<'CLUSTEREOF'
create role "regress" with login;
alter role "regress" password 'md5c2a101703f1e515ef9769f835d6fe78a';
alter role "regress" valid until '2100-03-04 00:00:00';
alter role "regress" set client_min_messages = 'notice';
alter role "regress" with superuser;

create role "wibble" with login;
alter role "wibble" password 'md54ea9ea89bc47825ea7b2fe7c2288b27a';
alter role "wibble" valid until '2007-03-02 00:00:00-08';
alter role "wibble" noinherit;
comment on role wibble is 'before';

create role "keep" with login;
alter role "keep" password 'md5a6e3dfe729e3efdf117eeb1059051f77';
alter role "keep" noinherit;
comment on role keep is 'keep';

create role "lose" with login;

\set tbs3dir '''':home'/regress/REGRESSDB/tbs/tbs3'''
create tablespace "tbs3" owner "regress"
  location :tbs3dir;

create database "regressdb" with
 owner "regress"
 encoding 'UTF8'
 tablespace "tbs3"
 connection limit = -1;
\connect regressdb

comment on database "regressdb" is
'old comment';
\connect postgres

\set tbs2dir '''':home'/regress/REGRESSDB/tbs/tbs2'''
create tablespace "tbs2" owner "lose"
  location :tbs2dir;

comment on tablespace tbs2 is 'This is the 2nd tablespace';

-- This does nothing but makes the acl for the tablespace into 
-- a non-default value.
revoke all on tablespace tbs2 from public;

set session authorization 'regress';
grant create on tablespace "tbs3" to "keep";
reset session authorization;

\set tbs4dir '''':home'/regress/REGRESSDB/tbs/tbs4'''
create tablespace "tbs4" owner "wibble"
  location :tbs4dir;

CLUSTEREOF

psql -d regressdb -U regress << 'EOF'

create language plpgsql;

create schema wibble authorization regress;
comment on schema wibble is 'This is owned by regress';

create schema wibble2;

grant create on schema wibble2 to keep;

create schema schema2;
comment on schema schema2 is 'This is schema2';

-- This does nothing but makes the acl for the schema into a non default value.
revoke all on schema schema2 from public;

create 
function schema2.myconv(integer, integer, cstring, internal, integer)
  returns void as '$libdir/ascii_and_mic', 'ascii_to_mic' language c;

create conversion myconv for 'SQL_ASCII' to 'MULE_INTERNAL' from schema2.myconv;

comment on conversion myconv is
'conversion comment';

-- Change parameter type
create 
function wibble.fn1(p1 varchar) returns varchar as
$$
begin
  return p1;
end
$$
language plpgsql stable strict;

-- Change parameter name
create 
function wibble.fn2(p1 varchar) returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;


-- Change function source code
create 
function wibble.fn3(p1 varchar) returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;


-- Change result type
create 
function wibble.fn4(p1 varchar) returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;

EOF
