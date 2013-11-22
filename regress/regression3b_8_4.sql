
psql -d postgres -v home=`pwd` <<'CLUSTEREOF'
create role "regress" with login;
alter role "regress" password 'md5c2a101703f1e515ef9769f835d6fe78a';
alter role "regress" valid until 'infinity';
alter role "regress" noinherit;
alter role "regress" with superuser;
alter role "regress" set client_min_messages = 'warning';

create role "keep" with login;
alter role "keep" password 'md5a6e3dfe729e3efdf117eeb1059051f77';
alter role "keep" noinherit;

create role "wibble" with login;
alter role "wibble" password 'md54ea9ea89bc47825ea7b2fe7c2288b27a';
alter role "wibble" valid until '2007-03-01 00:00:00-08';
alter role "wibble" noinherit;
comment on role wibble is 'after';

alter role "regress" with superuser;
alter role "regress" set client_min_messages = 'warning';
comment on role regress is 'new comment';

create role "keep2";

\set tbs3dir '''':home'/regress/REGRESSDB/tbs/tbs3'''
create tablespace "tbs3" owner "regress"
  location :tbs3dir;

create database "regressdb" with
 owner "regress"
 encoding 'UTF-8'
 tablespace "pg_default"
 connection limit = 4;
\connect regressdb

comment on database "regressdb" is
'new comment';
\connect postgres

\set tbs2dir '''':home'/regress/REGRESSDB/tbs/tbs2'''
create tablespace "tbs2" owner "keep"
  location :tbs2dir;

comment on tablespace tbs2 is 'This is the second tablespace';

-- This does nothing but makes the acl for the tablespace into 
-- a non-default value.
revoke all on tablespace tbs2 from public;

set session authorization 'regress';
grant create on tablespace "tbs3" to "keep";
reset session authorization;

\set tbs4dir '''':home'/regress/REGRESSDB/tbs/TBS4'''
create tablespace "tbs4" owner "wibble"
  location :tbs4dir;

CLUSTEREOF

psql -d regressdb -U regress << 'EOF'

create language plpgsql;
alter language plpgsql owner to regress;
comment on language plpgsql is 'PROCEDural';

revoke usage on language plpgsql from public;
grant usage on language plpgsql to keep;

create schema wibble;

create schema wibble2;
comment on schema wibble2 is 'wibble2';

-- This does nothing but makes the acl for the schema into a non-default value.
revoke all on schema wibble2 from public;

create schema schema2;
comment on schema schema2 is 'This is wibble3 again';


grant create on schema schema2 to keep;
revoke usage on schema schema2 from public;

create 
function schema2.myconv(integer, integer, cstring, internal, integer)
  returns void as '$libdir/ascii_and_mic', 'ascii_to_mic' language c;

create conversion myconv for 'SQL_ASCII' to 'MULE_INTERNAL' from schema2.myconv;
alter conversion myconv owner to keep;


comment on conversion myconv is
'conversion comment has changed';

revoke all on schema schema2 from keep;

create conversion schema2.myconv2 for 'SQL_ASCII' to 'MULE_INTERNAL' from schema2.myconv;
alter conversion schema2.myconv2 owner to keep;

comment on conversion schema2.myconv2 is
'New conversion';

-- Change parameter type
create 
function wibble.fn1(p1 text) returns varchar as
$$
begin
  return p1;
end
$$
language plpgsql stable strict;

-- Change parameter name
create or replace
function wibble.fn2(param varchar) returns varchar as
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
  return 'y';
end
$$
language plpgsql stable strict;


-- Change result type
create or replace
function wibble.fn4(p1 varchar) returns char(1) as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;


EOF
 
