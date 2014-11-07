
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

-- Postgres bug #11867
-- Cannot combine tablespace change and type replacement in one test.
create database "regressdb" with
 owner "regress"
 encoding 'UTF-8'
-- tablespace "pg_default"   -- To deal with postgres bug (see above)
 tablespace "tbs3"
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

-- Diffs on privs to tablespace
revoke all on tablespace tbs3 from public;
grant create on tablespace tbs3 to wibble;


\set tbs4dir '''':home'/regress/REGRESSDB/tbs/tbs4'''
create tablespace "tbs4" owner "regress"
  location :tbs4dir;

comment on tablespace tbs4 is 'This is the 4th tablespace';
revoke all on tablespace tbs4 from public;

-- Check handling of grant option.
grant create on tablespace tbs3 to keep;

-- Check role grants
grant regress to wibble;
CLUSTEREOF

psql -d regressdb -U regress <<'DBEOF'

-- Languages
alter user keep with superuser;

set session authorization keep;
create trusted language "plpgsql";
comment on language "plpgsql" is 
'procedural language plpgsql';
reset session authorization;

-- Schemata
grant regress to keep;
create schema n1;
comment on schema n1 is 'schema n1';
reset session authorization;
revoke regress from keep;
alter user keep with nosuperuser;

create schema wibble;

create schema schema2;
comment on schema schema2 is 'This is wibble3 again';

grant create on schema schema2 to keep;
revoke usage on schema schema2 from public;

-- Functions
-- Change parameter type
create 
function wibble.fn1(p1 text) returns varchar as
$$
begin
  return p1;
end
$$
language plpgsql stable strict;

-- Change parameter name, remove comment
create or replace
function wibble.fn2(param varchar) returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;

-- Change function source code with no access to schema
grant create on schema wibble to wibble;

set session authorization wibble;
create 
function wibble.fn3(p1 varchar) returns varchar as
$$
begin
  return 'y';
end
$$
language plpgsql stable strict;

reset session authorization;

revoke create on schema wibble from wibble;

-- Change result type
create or replace
function wibble.fn4(p1 varchar) returns char(1) as
$$
begin
  return 'x';
end
$$
language plpgsql stable strict;

-- Change parameter mode without affecting signature
create or replace
function wibble.fn5(p1 in out varchar, p2 out varchar, p3 out varchar) 
   returns setof record as
$$
begin
  p2 := p1;
  p3 := p1;
end
$$
language plpgsql stable strict cost 4;
alter function wibble.fn5(varchar) owner to wibble;

-- Change owner and config options
set session authorization regress;

create 
function wibble.fn6(p1 varchar) returns setof varchar as
$$
begin
  return next 'x';
end
$$
language plpgsql volatile strict security definer rows 4;

alter function wibble.fn6(varchar) set enable_nestloop = 'off';
alter function wibble.fn6(varchar) set enable_mergejoin = 'on';

grant execute on function wibble.fn6(varchar) to keep;

reset session authorization;

-- Change parameter defaults
create 
function wibble.fn7(
     p1 varchar,
     p2 varchar,
     p3 integer default 0,
     p4 boolean default false) 
  returns varchar as
$$
begin
  return 'x';
end
$$
language plpgsql stable;


-- Aggregate functions
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

alter aggregate public.mysum2(int4) owner to wibble;
comment on aggregate public.mysum2(int4) is
'aggregate comment';










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

create type schema2.yesno as enum ('yes', 'no');

create type "public"."veil_variable_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text",
  "shared"   "pg_catalog"."bool"
);

comment on type "public"."veil_variable_t" is
'veil_variable_t with different comment';

alter type public.veil_variable_t owner to keep;


create type "public"."vv2_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text",
  "shared"   "pg_catalog"."text"
);

create type "public"."vv3_t" as (
  "name"   "pg_catalog"."text",
  "shared"   "pg_catalog"."bool"
);

comment on type "public"."vv3_t" is
'vv3_t';

comment on column "public"."vv3_t".name is
'name column updated';


create type "public"."vv4_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text"
);

comment on column "public"."vv4_t".name is
'name column with changed comment';


comment on column "public"."vv4_t"."type" is
'type column with new comment';




-- Domains
create domain "public"."postal2"
  as "public"."mychar";

create domain "public"."postal3"
  as "public"."mychar"
  default 'x'::mychar not null;

comment on domain "public"."postal3" is
'wibbly';

alter domain public.postal3 owner to keep;

create domain "public"."postal4"
  as "public"."mychar"
  default 'z'::mychar null;

create domain "public"."postal5"
  as "public"."mychar"
  default 'z'::mychar null;

create domain "public"."postal6"
  as "public"."mychar";


-- Incorrect regexp below
create domain "public"."us_postal_code"
  as "pg_catalog"."text"
  CHECK (((VALUE ~ E'^\\d{3}$'::text) OR (VALUE ~ E'^\\d{5}-\\d{4}$'::text)));

create domain "public"."us_postal_code2"
  as "pg_catalog"."text";


-- Conversions
create 
function schema2.myconv(integer, integer, cstring, internal, integer)
  returns void as '$libdir/ascii_and_mic', 'ascii_to_mic' language c;

create conversion myconv for 'SQL_ASCII' to 'MULE_INTERNAL' from schema2.myconv;
alter conversion myconv owner to keep;

comment on conversion myconv is
'conversion comment has changed';


DBEOF