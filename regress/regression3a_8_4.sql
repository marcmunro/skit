
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

-- Database diffs: comment, connection limit and tablespace.
--
create database "regressdb" with
 owner "regress"
 encoding 'UTF8'
 tablespace "tbs3"
 connection limit = -1;
\connect regressdb

comment on database "regressdb" is
'old comment';
\connect postgres

-- tablespace diffs: comment, ownership, privs

\set tbs2dir '''':home'/regress/REGRESSDB/tbs/tbs2'''
create tablespace "tbs2" owner "lose"
  location :tbs2dir;

comment on tablespace tbs2 is 'This is the 2nd tablespace';

-- This does nothing but makes the acl for the tablespace into 
-- a non-default value.
revoke all on tablespace tbs2 from public;

\set tbs4dir '''':home'/regress/REGRESSDB/tbs/tbs4'''
create tablespace "tbs4" owner "regress"
  location :tbs4dir;

comment on tablespace tbs4 is 'This is the 4th tablespace';

-- Check handling of grant option.
grant create on tablespace tbs3 to keep with grant option;

-- Check role grants
grant regress to keep;
grant regress to wibble with admin option;
CLUSTEREOF

psql -d regressdb -U regress <<'DBEOF'

-- Languages
create language "plpgsql";
comment on language "plpgsql" is 
'plpgsql';

grant usage on language plpgsql to wibble;

-- Schemata
create schema n1;
comment on schema n1 is 'n1';

create schema wibble authorization regress;
comment on schema wibble is 'This is owned by regress';

create schema wibble2;
grant create on schema wibble2 to keep;

create schema schema2;
comment on schema schema2 is 'This is schema2';

-- This does nothing but makes the acl for the schema into a non default value.
revoke all on schema schema2 from public;

-- Functions
-- Change parameter type
create 
function wibble.fn1(p1 varchar) returns varchar as
$$
begin
  return p1;
end
$$
language plpgsql stable strict;

-- Change parameter name, remove comment
create 
function wibble.fn2(p1 varchar) returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;

comment on function wibble.fn2(varchar) is 'Old comment';

-- Change function source code with no access to schema
grant create, usage on schema wibble to wibble;

set session authorization wibble;
create 
function wibble.fn3(p1 varchar) returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict cost 1000;

reset session authorization;

-- Change result type
create 
function wibble.fn4(p1 varchar) returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;

-- Change parameter mode without affecting signature and owner
create 
function wibble.fn5(p1 in varchar, p2 out varchar, p3 out varchar) 
   returns setof record as
$$
begin
  p2 := p1;
  p3 := p1;
end
$$
language plpgsql stable strict;

grant execute on function wibble.fn5(varchar) to keep;
revoke execute on function wibble.fn5(varchar) from public;
alter function wibble.fn5(varchar) owner to keep;

-- Change owner and config options
set session authorization wibble;

create 
function wibble.fn6(p1 varchar) returns setof varchar as
$$
begin
  return next 'x';
end
$$
language plpgsql stable cost 1000;

alter function wibble.fn6(varchar) set enable_hashagg = 'off';
alter function wibble.fn6(varchar) set enable_mergejoin = 'off';

reset session authorization;
revoke create, usage on schema wibble from wibble;

-- Change parameter defaults
create 
function wibble.fn7(
     p1 varchar,
     p2 varchar default 'a,b',
     p3 integer default 0,
     p4 boolean default true) 
  returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable;


-- Aggregate functions.
create or replace function "public"."addint4"(
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
  sfunc = "addint4",
  stype = "pg_catalog"."int4",
  initcond = '0'
);

create or replace function "public"."addkint4"(
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

create aggregate "public"."mysum2" (
  basetype = "pg_catalog"."int4",
  sfunc = "addkint4",
  stype = "pg_catalog"."int4",
  initcond = '0'
);





-- Types
create or replace function "public"."mycharin"(
    in "pg_catalog"."cstring")
  returns "public"."mychar"
as 'charin'
language internal immutable strict;
 
grant execute on function "public"."mycharin"(
    in "pg_catalog"."cstring") to keep;

create or replace function "public"."mycharout"(
    in "public"."mychar")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;

create type "public"."mychar"(
  input = "public"."mycharin",
  output = "public"."mycharout",
  passedbyvalue,
  internallength = 1,
  alignment = char,
  storage = plain,
  delimiter = ',');

comment on type "public"."mychar" is
'mychar';

create type schema2.yesno as enum ('no', 'yes');
comment on type schema2.yesno is 'boolean-ish';

-- Composite types
create type "public"."veil_variable_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text",
  "shared"   "pg_catalog"."bool"
);

comment on type "public"."veil_variable_t" is
'veil_variable_t';


create type "public"."vv2_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text",
  "shared"   "pg_catalog"."bool"
);

create type "public"."vv3_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text"
);

comment on column "public"."vv3_t".name is
'name column';

create type "public"."vv4_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text"
);

comment on column "public"."vv4_t".name is
'name column';

-- Domains
create domain "public"."postal2"
  as "public"."mychar";

create domain "public"."postal3"
  as "public"."mychar"
  default 'x'::mychar not null;

comment on domain "public"."postal3" is
'wibble';

create domain "public"."postal4"
  as "public"."mychar"
  default 'z'::mychar not null;

create domain "public"."postal5"
  as "public"."mychar"
  default 'q'::mychar null;

create domain "public"."postal6"
  as "public"."mychar"
  default 'x'::mychar not null;


create domain "public"."us_postal_code"
  as "pg_catalog"."text"
  constraint "MyConstraintName_Ugh"
  CHECK (((VALUE ~ E'^\\d{4}$'::text) OR (VALUE ~ E'^\\d{5}-\\d{4}$'::text)));

create domain "public"."us_postal_code2"
  as "pg_catalog"."text"
  CHECK (((VALUE ~ E'^\\d{4}$'::text) OR (VALUE ~ E'^\\d{5}-\\d{4}$'::text)));


-- Conversions
create 
function schema2.myconv(integer, integer, cstring, internal, integer)
  returns void as '$libdir/ascii_and_mic', 'ascii_to_mic' language c;

create conversion myconv for 'SQL_ASCII' to 'MULE_INTERNAL' from schema2.myconv;

comment on conversion myconv is
'conversion comment';



DBEOF