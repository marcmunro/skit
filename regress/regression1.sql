
psql -d postgres -v home=`pwd` <<'CLUSTEREOF'
create role "regress" with login;
alter role "regress" password 'md5c2a101703f1e515ef9769f835d6fe78a';
alter role "regress" valid until 'infinity';
alter role "regress" noinherit;
alter role "regress" with superuser;
alter role "regress" set client_min_messages = 'warning';

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

create role "bark" with login;
alter role "bark" password 'woofgrr';
alter role "bark" with superuser;
alter role "bark" with createdb;
alter role "bark" with createrole;
alter role "bark" set client_min_messages = 'error';

comment on role "bark" is 'woof';

create role "lose" with login;
alter role "lose" password 'md5c62bc3e38bac4209132682f13509ba96';
alter role "lose" noinherit;

create role "keep" with login;
alter role "keep" password 'md5a6e3dfe729e3efdf117eeb1059051f77';
alter role "keep" noinherit;


set session authorization 'keep';
grant "keep" to "lose" with admin option;
reset session authorization;

\set tbs2dir '''':home'/regress/REGRESSDB/tbs/tbs2'''
create tablespace "tbs2" owner "regress"
  location :tbs2dir;

comment on tablespace tbs2 is 'This is the second tablespace';

create role "keep2" with login;
alter role "keep2" password 'md5dd9b387fa54744451a97dc9674f6aba2';
alter role "keep2" noinherit;

set session authorization 'lose';
grant "keep" to "keep2" with admin option;
reset session authorization;


set session authorization 'regress';
grant create on tablespace "tbs2" to "keep2";
reset session authorization;
create role "marco" with login;
alter role "marco" password 'md54ea9ea89bc47825ea7b2fe7c2288b27a';
alter role "marco" noinherit;


set session authorization 'regress';
grant create on tablespace "tbs3" to "marco" with grant option;
reset session authorization;

set session authorization 'regress';
grant create on tablespace "tbs3" to "keep2";
reset session authorization;

set session authorization 'regress';
grant create on tablespace "tbs3" to "keep2" with grant option;
reset session authorization;

set session authorization 'regress';
grant create on tablespace "tbs3" to "keep" with grant option;
reset session authorization;

set session authorization 'regress';
grant create on tablespace "tbs3" to "keep";
reset session authorization;

create role "wibble" with login;
alter role "wibble" password 'md54ea9ea89bc47825ea7b2fe7c2288b27a';
alter role "wibble" valid until '2007-03-01 00:00:00-08';
alter role "wibble" noinherit;

\set tbs4dir '''':home'/regress/REGRESSDB/tbs/tbs4'''
create tablespace "tbs4" owner "wibble"
  location :tbs4dir;

set session authorization 'keep2';
grant "keep2" to "wibble";
reset session authorization;

set session authorization 'keep';
grant "keep" to "wibble" with admin option;
reset session authorization;


set session authorization 'regress';
grant create on tablespace "tbs3" to "wibble";
reset session authorization;

CLUSTEREOF
 
psql -d regressdb -U bark <<'DBEOF'
 
alter schema "public" owner to "regress";
 
comment on schema "public" is
'old public schema';

\echo Done with schema "public";


create language "plpgsql";

comment on language "plpgsql" is
'Procedural language';

\echo updating schema "public";

create or replace function "public"."addint4"(
    in "pg_catalog"."int4",
    in "pg_catalog"."int4")
  returns "pg_catalog"."int4"
as 
$_$
begin
  return _state + _next;
end;
$_$
language plpgsql stable;

create aggregate "public"."mysum" (
  basetype = "pg_catalog"."int4",
  sfunc = "addint4",
  stype = "pg_catalog"."int4",
  initcond = '0'
);

create aggregate "public"."mycount"(*) (
  sfunc = "pg_catalog"."int8inc",
  stype = "pg_catalog"."int8",
  initcond = '0');


create aggregate "public"."mycount"("pg_catalog"."any") (
  sfunc = "pg_catalog"."int8inc_any",
  stype = "pg_catalog"."int8",
  initcond = '0');

create aggregate "public"."mymax"("pg_catalog"."int4") (
  sfunc = "pg_catalog"."int4larger",
  stype = "pg_catalog"."int4",
  sortop = "pg_catalog".">");

comment on aggregate "public"."mymax"("pg_catalog"."int4") is
'another comment';


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

\echo Done with schema "public";


\echo updating schema "public";

set session authorization 'regress';
grant usage on schema "public" to "public";
reset session authorization;

set session authorization 'regress';
grant create on schema "public" to "public";
reset session authorization;
\echo Done with schema "public";


\echo updating schema "public";
-- 

create table "public"."additional" (
  "str1"                text not null,
  "str2"                varchar(40),
  mystr                 mychar
) tablespace tbs3;

alter table public.additional
  alter column str2 set storage main;

alter table public.additional
  alter column str1 set storage external;

comment on column "public"."additional"."str1" is
'str1 column';

comment on column "public"."additional"."str2" is
'str2 column';



\echo Done with schema "public";


\echo updating schema "public";

set session authorization 'wibble';
create table "public"."thing" (
  "keycol" int4 not null
) tablespace tbs3;

comment on table "public"."thing" is
'thing table';
reset session authorization;

alter table thing add constraint thing__pk primary key (keycol);



create table "public"."thing_2" (
  "extra"               bool
                          default 'true'
) inherits ("public"."thing") tablespace tbs3;

\echo Done with schema "public";


set session authorization 'marco';
grant create on database "regressdb" to "keep";
reset session authorization;

grant usage on language "plpgsql" to "keep2" with grant option;

grant usage on language "plpgsql" to "keep";

grant usage on language "plpgsql" to "public";

grant usage on language "plpgsql" to "wibble";

comment on language plpgsql is 'this is plpgsql';

\echo updating schema "public";

grant select on table "public"."additional" to "keep2" with grant option;
 
create or replace function "public"."mycharin2"(
    in "pg_catalog"."cstring")
  returns "public"."mychar2"
as 'charin'
language internal immutable strict;

create or replace function "public"."mycharin3"(
    in "pg_catalog"."cstring")
  returns "public"."mychar3"
as 'charin'
language internal immutable strict;

create or replace function "public"."mycharin4"(
    in "pg_catalog"."cstring")
  returns "public"."mychar4"
as 'charin'
language internal immutable strict;

create or replace function "public"."mycharin5"(
    in "pg_catalog"."cstring")
  returns "public"."mychar5"
as 'charin'
language internal immutable strict;

create or replace function "public"."mycharinnnn5"(
    in "pg_catalog"."cstring")
  returns "public"."mychar5"
as 'charin'
language internal immutable strict;

create or replace function "public"."mycharout2"(
    in "public"."mychar2")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;

create or replace function "public"."mycharout3"(
    in "public"."mychar3")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;

create or replace function "public"."mycharout4"(
    in "public"."mychar4")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;

create or replace function "public"."mycharout5"(
    in "public"."mychar5")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;

create or replace function "public"."plpgsql1"(
    _x in "pg_catalog"."int4",
    _y in "pg_catalog"."int4")
  returns "pg_catalog"."int4"
as 
$_$
begin
  return _x + _y;
end;
$_$
language plpgsql stable strict;

comment on function "public"."plpgsql1"("pg_catalog"."int4", "pg_catalog"."int4") is
'function';

set session authorization 'regress';
grant execute on function "public"."plpgsql1"("pg_catalog"."int4", "pg_catalog"."int4") to "public";
reset session authorization;

create or replace function "public"."plpgsql2"(
    _z in "pg_catalog"."int4",
    _y in "pg_catalog"."int4")
  returns "pg_catalog"."int4"
as 
$_$
begin
  return _z + _y;
end;
$_$
language plpgsql stable strict;

comment on function "public"."plpgsql2"("pg_catalog"."int4", "pg_catalog"."int4") is
'function 2';

-- You will need to have installed the contrib package 'seg' for the next part
create or replace function "public"."seg_in"(
    in "pg_catalog"."cstring")
  returns "public"."seg"
as '$libdir/seg', 'seg_in'
language c immutable strict;

create or replace function "public"."seg_out"(
    in "public"."seg")
  returns "pg_catalog"."cstring"
as '$libdir/seg', 'seg_out'
language c immutable strict;

create type "public"."seg"(
  input = "public"."seg_in",
  output = "public"."seg_out",
  internallength = 12,
  alignment = int4,
  storage = plain,
  delimiter = ',');

comment on type "public"."seg" is
'floating point interval ''FLOAT .. FLOAT'', ''.. FLOAT'', ''FLOAT ..'' or ''FLOAT''';


create or replace function "public"."seg_cmp"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."int4"
as '$libdir/seg', 'seg_cmp'
language c immutable strict;

comment on function "public"."seg_cmp"("public"."seg","public"."seg") is
'btree comparison function';

create or replace function "public"."seg_different"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_different'
language c immutable strict;

comment on function "public"."seg_different"("public"."seg","public"."seg") is
'different';

create or replace function "public"."seg_ge"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_ge'
language c immutable strict;

comment on function "public"."seg_ge"("public"."seg","public"."seg") is
'greater than or equal';

create or replace function "public"."seg_gt"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_gt'
language c immutable strict;

comment on function "public"."seg_gt"("public"."seg","public"."seg") is
'greater than';

create or replace function "public"."seg_le"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_le'
language c immutable strict;

comment on function "public"."seg_le"("public"."seg","public"."seg") is
'less than or equal';

create or replace function "public"."seg_left"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_left'
language c immutable strict;

comment on function "public"."seg_left"("public"."seg","public"."seg") is
'is left of';

create or replace function "public"."seg_lt"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_lt'
language c immutable strict;

comment on function "public"."seg_lt"("public"."seg","public"."seg") is
'less than';

create or replace function "public"."seg_right"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_right'
language c immutable strict;

comment on function "public"."seg_right"("public"."seg","public"."seg") is
'is right of';

create or replace function "public"."seg_same"(
    in "public"."seg",
    in "public"."seg")
  returns "pg_catalog"."bool"
as '$libdir/seg', 'seg_same'
language c immutable strict;

comment on function "public"."seg_same"("public"."seg","public"."seg") is
'same as';

create or replace function "public"."wib_in"(
    in "pg_catalog"."cstring")
  returns "public"."wib"
as 'charin'
language internal immutable strict;

create or replace function "public"."wib_out"(
    in "public"."wib")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;

create type "public"."wib"(
  input = "public"."wib_in",
  output = "public"."wib_out",
  internallength = 12,
  alignment = int4,
  storage = plain,
  delimiter = ',');


create or replace function "public"."wib_gt"(
    in "public"."wib",
    in "public"."wib")
  returns "pg_catalog"."bool"
as 'text_gt'
language internal immutable strict;

create or replace function "public"."wib_lt"(
    in "public"."wib",
    in "public"."wib")
  returns "pg_catalog"."bool"
as 'text_lt'
language internal immutable strict;

create table "public"."thing_1" (
) tablespace tbs3;

comment on table "public"."thing_1" is
'thing1';


create table "public"."thing_3" (
) tablespace tbs3;


create sequence "public"."thingy_id_seq"
  start with 1 increment by 1
  minvalue 1 maxvalue 9223372036854775807
  cache 1;

comment on sequence "public"."thingy_id_seq" is
'thingy';


grant select on table "thingy_id_seq" to "keep";

create sequence "public"."wib_seq"
  start with 1000 increment by 1
  minvalue 1 maxvalue 9223372036854775807
  cache 1;

comment on sequence "public"."wib_seq" is
'wib wib wib';


grant delete on table "wib_seq" to "keep";

grant insert on table "wib_seq" to "keep";

grant references on table "wib_seq" to "keep";

grant rule on table "wib_seq" to "keep";

grant select on table "wib_seq" to "keep";

grant trigger on table "wib_seq" to "keep";

grant update on table "wib_seq" to "keep";

create sequence "public"."wibble_seq"
  start with 1000 increment by 1
  minvalue 1 maxvalue 9223372036854775807
  cache 1;


create sequence "public"."wubble_seq"
  start with 1000 increment by 1
  minvalue 1 maxvalue 9223372036854775807
  cache 1;

-- \echo Done with schema "public";
-- 
-- 
set session authorization 'marco';
grant temporary on database "regressdb" to "keep" with grant option;
reset session authorization;

set session authorization 'marco';
grant temporary on database "regressdb" to "public";
reset session authorization;

set session authorization 'regress';
grant temporary on database "regressdb" to "public";
reset session authorization;
-- 
-- \echo updating schema "public";
-- 
-- 
create operator "public".< (
  leftarg = "public"."wib",
  rightarg = "public"."wib",
  procedure = "public"."wib_lt",
  commutator = operator(public.>=),
  negator = "public".">",
  restrict = "pg_catalog"."scalarltsel",
  join = "pg_catalog"."scalarltjoinsel");

comment on operator public.<(wib,wib) is
'this is wib < wib';

create operator "public".<< (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_left",
  commutator = "public".">>",
  restrict = "pg_catalog"."positionsel",
  join = "pg_catalog"."positionjoinsel");



create operator "public".<= (
  leftarg = "public"."wib",
  rightarg = "public"."wib",
  procedure = "public"."wib_lt",
  commutator = "public".">",
  negator = "public".">=",
  restrict = "pg_catalog"."scalarltsel",
  join = "pg_catalog"."scalarltjoinsel");



create operator "public".<> (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_different",
  commutator = "public"."<>",
  negator = "public"."=",
  restrict = "pg_catalog"."neqsel",
  join = "pg_catalog"."neqjoinsel");



create operator "public".> (
  leftarg = "public"."wib",
  rightarg = "public"."wib",
  procedure = "public"."wib_gt",
  commutator = "public"."<=",
  negator = "public"."<",
  restrict = "pg_catalog"."scalarltsel",
  join = "pg_catalog"."scalarltjoinsel");



create operator "public".>= (
  leftarg = "public"."wib",
  rightarg = "public"."wib",
  procedure = "public"."wib_gt",
  commutator = "public"."<",
  negator = "public"."<=",
  restrict = "pg_catalog"."scalarltsel",
  join = "pg_catalog"."scalarltjoinsel");



create operator "public".>> (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_right",
  commutator = "public"."<<",
  restrict = "pg_catalog"."positionsel",
  join = "pg_catalog"."positionjoinsel");



create operator "public".< (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_lt",
  commutator = "public".">",
  negator = "public".">=",
  restrict = "pg_catalog"."scalarltsel",
  join = "pg_catalog"."scalarltjoinsel");



create operator "public".<= (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_le",
  commutator = "public".">=",
  negator = "public".">",
  restrict = "pg_catalog"."scalarltsel",
  join = "pg_catalog"."scalarltjoinsel");



create operator "public".> (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_gt",
  commutator = "public"."<",
  negator = "public"."<=",
  restrict = "pg_catalog"."scalargtsel",
  join = "pg_catalog"."scalargtjoinsel");



create operator "public".>= (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_ge",
  commutator = "public"."<=",
  negator = "public"."<",
  restrict = "pg_catalog"."scalargtsel",
  join = "pg_catalog"."scalargtjoinsel");



create operator "public".= (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_same",
  commutator = operator(public.=),
  negator = operator(public.<>),
  restrict = "pg_catalog"."eqsel",
  join = "pg_catalog"."eqjoinsel",
  merges,
  sort1 = operator("public".<),
  sort2 = operator("public".<),
  ltcmp = operator("public".<),
  gtcmp = operator("public".>));



create operator class "public"."seg_ops"
  default for type "public"."seg" using btree as
    operator 1  "public".<,
    operator 2  "public".<=,
    operator 5  "public".>,
    operator 4  "public".>=,
    operator 3  "public".=,
    function 1  "public"."seg_cmp"("public"."seg","public"."seg");

comment on operator class "public"."seg_ops" using btree is
'operator class for seg_ops';

comment on operator family "public"."seg_ops" using btree is
'operator family for seg_ops';



-- \echo Done with schema "public";
-- 
-- 
-- create schema "schema1" authorization "regress";
-- -- 
-- -- 
-- set session authorization 'regress';
-- grant usage on schema "schema1" to "keep";
-- reset session authorization;
-- \echo Done with schema "schema1";
-- 
-- 
-- create schema "schema2" authorization "regress";
-- 
-- comment on schema "schema2" is
-- 'wibble';
-- 
-- 
-- set session authorization 'regress';
-- grant usage on schema "schema2" to "keep2";
-- reset session authorization;
-- 
-- set session authorization 'regress';
-- grant create on schema "schema2" to "keep2";
-- reset session authorization;
-- 
-- set session authorization 'regress';
-- grant usage on schema "schema2" to "wibble" with grant option;
-- reset session authorization;
-- 
-- set session authorization 'regress';
-- grant create on schema "schema2" to "wibble" with grant option;
-- reset session authorization;
-- 
-- set session authorization 'regress';
-- grant usage on schema "schema2" to "keep";
-- reset session authorization;
-- 
-- set session authorization 'regress';
-- grant usage on schema "schema2" to "keep2" with grant option;
-- reset session authorization;
-- 
-- set session authorization 'regress';
-- grant usage on schema "schema2" to "public";
-- reset session authorization;
-- \echo Done with schema "schema2";
-- 
-- 
-- create schema "schema3" authorization "regress";
-- 
-- 
-- set session authorization 'regress';
-- grant usage on schema "schema3" to "keep2";
-- reset session authorization;
-- 
-- set session authorization 'regress';
-- grant create on schema "schema3" to "keep2";
-- reset session authorization;
-- \echo Done with schema "schema3";
-- 
-- 
-- \echo updating schema "public";
-- 
create type "public"."mychar2"(
  input = "public"."mycharin2",
  output = "public"."mycharout2",
  passedbyvalue,
  internallength = 1,
  alignment = char,
  storage = plain,
  delimiter = ',');

comment on type "public"."mychar2" is
'mychar2';


create type "public"."mychar3"(
  input = "public"."mycharin3",
  output = "public"."mycharout3",
  passedbyvalue,
  internallength = 1,
  alignment = char,
  storage = plain,
  delimiter = ',');


create type "public"."mychar4"(
  input = "public"."mycharin4",
  output = "public"."mycharout4",
  passedbyvalue,
  internallength = 1,
  alignment = char,
  storage = plain,
  delimiter = ',');


create type "public"."mychar5"(
  input = "public"."mycharin5",
  output = "public"."mycharout5",
  passedbyvalue,
  internallength = 1,
  alignment = char,
  storage = plain,
  delimiter = ',');


create domain "public"."postal3"
  as "public"."mychar"
  default 'x'::mychar not null;

comment on domain "public"."postal3" is
'wibble';


create domain "public"."us_postal_code"
  as "pg_catalog"."text"
  CHECK (((VALUE ~ E'^\\d{4}$'::text) OR (VALUE ~ E'^\\d{5}-\\d{4}$'::text)));


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

comment on type "public"."vv2_t" is
'vv2_t';


create type "public"."vv3_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text",
  "shared"   "pg_catalog"."bool"
);

comment on type "public"."vv3_t" is
'vv3_t';


create type "public"."vv4_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text",
  "shared"   "pg_catalog"."bool"
);

comment on type "public"."vv4_t" is
'vv4_t';


create type "public"."vv5_t" as (
  "name"   "pg_catalog"."text",
  "type"   "pg_catalog"."text",
  "shared"   "pg_catalog"."bool"
);

comment on type "public"."vv5_t" is
'vv5_t';

comment on column "public"."vv5_t".name is
'name column';


create schema schema2;

revoke usage on schema schema2 from public;
grant usage on schema schema2 to regress;
comment on schema schema2 is 'this is a comment for schema2';

create type schema2.yesno as enum ('no', 'yes');
comment on type schema2.yesno is 'boolean-ish';

create domain "public"."postal2"
  as "public"."mychar";


create or replace function "public"."mycharsend"(
    in "public"."mychar")
  returns "pg_catalog"."bytea"
as 'charsend'
language internal immutable strict;

create cast("public"."mychar" as "pg_catalog"."bytea")
  with function "public"."mycharsend"("public"."mychar")
  as assignment;

comment on cast("public"."mychar" as "pg_catalog"."bytea")
is 'cast comment';


create cast("public"."postal2" as "public"."mychar")
  without function;


create table schema2.keys_table (
  key1          int4 not null,
  key2          int4 not null,
  key3          int4 not null,
  key4          int4 not null
);

alter table schema2.keys_table add constraint thing__pk
  primary key(key1, key2, key3, key4);

alter table schema2.keys_table add constraint thing__31_uk
  unique(key3, key1);

alter table schema2.keys_table add constraint thing__41_uk
  unique(key4, key1);

alter table schema2.keys_table add constraint thing__234_uk
  unique(key2, key3, key4) 
with (fillfactor = 90) 
using index tablespace tbs2;


create table schema2.arrays (
  key1          int4 not null,
  one           int4[] not null,
  two           numeric(12,2)[][],
  three         varchar(200)[][][]
);

alter table schema2.arrays alter column key1 set statistics 200;



\echo Done with schema "public";

DBEOF

