
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

alter tablespace tbs2 set (seq_page_cost = 1.5);
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


\echo updating schema "public";

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

create or replace function "public"."addint4"(
    _state in "pg_catalog"."int4",
    _next in "pg_catalog"."int4",
    _other in "pg_catalog"."int4")
  returns "pg_catalog"."int4"
as 
$_$
begin
  return _state + _next + _other;
end;
$_$
language plpgsql stable cost 5;

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
  delimiter = ',',
  category = 's',
  preferred = true,
  collatable = true);

comment on type "public"."mychar" is
'mychar';

\echo Done with schema "public";


\echo updating schema "public";

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
) with (oids) tablespace tbs3;

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

alter table thing alter keycol set statistics 1000;


comment on constraint thing__pk on public.thing is
'This is the pk for thing';


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

grant usage on language "plpgsql" to "wibble";

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

create table wibbly (
  wib_id integer not null
);

create sequence "public"."wib_seq"
  start with 1000 increment by 1
  minvalue 1 maxvalue 9223372036854775807
  cache 1
  owned by wibbly.wib_id;

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


create operator family seg_ops3 using btree;
alter operator family seg_ops3 using btree add operator 1 <(seg, seg);
alter operator family seg_ops3 using btree add operator 2 <=(seg, seg);
alter operator family seg_ops3 using btree add function 1 seg_cmp(seg, seg);

create operator family seg_ops4 using btree;
alter operator family seg_ops4 using btree add operator 1 <(seg, seg);
alter operator family seg_ops4 using btree add operator 2 <=(seg, seg);
alter operator family seg_ops4 using btree add function 1 seg_cmp(seg, seg);
alter operator family seg_ops4 using btree owner to keep;

comment on operator family seg_ops4 using btree is 
'operator family seg_ops4';



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



create table schema2.arrays (
  key1          int4 not null,
  one           int4[] not null,
  two           numeric(12,2)[][],
  three         varchar(200)[][][]
);

alter table schema2.arrays alter column key1 set statistics 200;

alter table schema2.arrays add constraint arrays__pk
  primary key(key1);

create table schema2.keys_table (
  key1          int4 not null,
  key2          int4 not null,
  key3          int4 not null,
  key4          int4
) with (fillfactor = 90, oids);

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

alter table schema2.keys_table add constraint keys_table__key1_fk
  foreign key(key1) references schema2.arrays(key1);


create table schema2.keys2 (
  key1          int4 not null,
  key2          int4 not null,
  key3          int4 not null,
  key4          int4,
  key5          int4
);

alter table schema2.keys2 add constraint keys2__key2_uk
  unique(key2);

alter table schema2.keys2 add constraint keys2__keys_fk
  foreign key(key1, key2, key3, key4) 
  references schema2.keys_table(key1, key2, key3, key4) match full
  on delete cascade on update set default
  deferrable initially deferred;

alter table schema2.keys2 add constraint keys2__total_chk
  check (key1 + key2 > key3);

alter table schema2.keys2 add constraint keys2__strlen_chk
  check (length(cast(key1 as text) || (cast(key2 as text))) > 5 );


create unique index keys2__key3_uk on schema2.keys2(key3);

comment on index schema2.keys2__key3_uk is
'comment for  keys2__key3_uk index';

create index keys2__key24_idx on schema2.keys2(key2,key4)
where key4 > 0;

create index key2__key2_fnidx on schema2.keys2((abs(key2)));

create rule keys_rule1 as on insert to schema2.keys_table
do also insert into 
   schema2.keys2(key1, key2, key3, key4, key5)
   values (new.key1, new.key2, new.key3, new.key4, null);


create function schema2.nullxform(x in int4) returns int4 as
$$
begin
    return x;
end;
$$
language plpgsql immutable strict
set search_path = schema2;

create or replace rule keys_rule2 as on insert to schema2.keys2
where key5 is not null
do also insert into 
   schema2.keys2(key1, key2, key3, key4)
   values (schema2.nullxform(new.key1), new.key2, new.key3, new.key4);

comment on rule keys_rule2 on schema2.keys2 is
'A comment on keys_rule2';

create 
function seg2int(_in seg) returns integer as
$$
begin
  return 1;
end;
$$
language plpgsql immutable strict;

create cast(seg as integer) with function seg2int(seg) as assignment;

create table casted (
  mykey    seg
);

alter table casted add constraint casted__cast_chk
  check (cast(mykey as integer) < 3);


create or replace 
function "public".trigger1() returns trigger as
$_$
begin
  if new.key1 > 4 then
    return new;
  else
    return null;
  end if;
end;
$_$
language plpgsql;

create trigger mytrigger1 
before insert or update on schema2.keys2
for each row execute procedure public.trigger1();

comment on trigger mytrigger1 on schema2.keys2 is
'Trigger comment';

create constraint trigger my_ctrigger
after insert or update on schema2.arrays
for each row execute procedure public.trigger1();


create view schema2.keys_view as
select k1.key1, k1.key2, k1.key3, 
       k1.key4, cast(k2.mykey as integer) as wibble
from   schema2.keys2 k1
cross join casted k2
where k1.key1 < 99;

comment on view schema2.keys_view is
'A view.';

grant all on table schema2.keys_view to keep2 with grant option;
grant select on table schema2.keys_view to keep;

create view schema2.view_on_view as
select * 
from   schema2.keys_view
where  key1 < 90;

alter view schema2.keys_view alter column key1 set default 99;

create rule keys_vrule1 as on insert to schema2.keys_view
do instead insert into 
   schema2.keys2(key1, key2, key3, key4, key5)
   values (new.key1, new.key2, new.key3, new.key4, new.wibble);

create 
function schema2.myconv(integer, integer, cstring, internal, integer)
  returns void as '$libdir/ascii_and_mic', 'ascii_to_mic' language c;

comment on 
  function schema2.myconv(integer, integer, cstring, internal, integer) is
'This is a conversion function.';

create conversion myconv for 'SQL_ASCII' to 'MULE_INTERNAL' from schema2.myconv;

comment on conversion myconv is
'conversion comment';


-- Now a cyclic view definition
create view v1 as select 'a' as a, 'b' as b;
create view v2 as select * from v1;
create or replace view v1 as select * from v2;


-- A plpgsql function that returns a set of rows
create or replace
function wibble(p1 varchar)
  returns setof varchar as
$$
begin
  return next 'a';
  return next 'b';
  return next 'c';
end;
$$
language plpgsql security definer cost 20 rows 3;


-- Text search stuff
create text search configuration skit (copy = pg_catalog.english);

-- Create a mapping that uses multiple dictionaries.
alter text search configuration public.skit
    drop mapping for asciiword;

alter text search configuration public.skit
    add mapping for asciiword with pg_catalog.english_stem,  pg_catalog.simple;


comment on text search configuration skit is
'comment for text search configuration';

create text search parser myparser (
    start = 'prsd_start',
    gettoken = 'prsd_nexttoken',
    end = 'prsd_end',
    lextypes = 'prsd_lextype',
    headline = 'prsd_headline');

create text search parser myparser2 (
    start = 'prsd_start',
    gettoken = 'prsd_nexttoken',
    end = 'prsd_end',
    lextypes = 'prsd_lextype');

comment on text search parser myparser2 is
'Parser';

create text search dictionary public.simple_dict (
    template = pg_catalog.simple,
    stopwords = english
);

comment on text search dictionary public.simple_dict is
'dict';

create text search template mysimple (
    init = dsimple_init,
    lexize = dsimple_lexize
);

comment on text search template mysimple is
'mysimple template';


-- Foreign Data Wrappers
create foreign data wrapper dummy;

create foreign data wrapper postgresql 
    validator postgresql_fdw_validator;

create foreign data wrapper mywrapper
    options (debug 'true');

grant all on foreign data wrapper mywrapper to keep;


-- Foreign Data Servers
create server kong 
    type 'postgresql' version '8.4'
    foreign data wrapper postgresql; 

grant all on foreign server kong to keep;

create server s2 
    foreign data wrapper dummy
    options (debug 'true');


-- User Mappings
create user mapping for keep
    server kong
    options (user 'general', password 'confusion');

create user mapping for public
    server kong
    options (user 'major', password 'problem');


create extension btree_gist;

create table circles (
    plane_id           int not null,
    is_non_overlapping boolean not null,
    c                  circle not null,
    exclude using gist (plane_id with =, c with &&)
        with (fillfactor = 20)
        using index tablespace tbs2
        where (is_non_overlapping)
);


-- Foreign tables.
create foreign table films (
    code        char(5) not null,
    title       varchar(40) not null
)
server kong;

comment on foreign table films is
'A foreign table';


create foreign table films2 (
    code        char(5) not null,
    title       varchar(40) not null
)
server kong;

comment on foreign table films2 is
'Another foreign table';


-- Collations
create collation my_collation from "C";
create collation wiblish (locale = 'C');


create table collated (
  col1   text collate wiblish
);

create type wiblish_thing as (
  wibbly  char collate wiblish
);


-- Range types
create type float8_range as range (subtype = float8, subtype_diff = float8mi);


create type myint8_range;

create 
function myint8_range_canon(_in myint8_range)
    returns myint8_range
as 'int8range_canonical'
language internal immutable strict;

create 
function myint8_range_diff(_in1 int8, _in2 int8)
    returns double precision as
$$
begin
   return 4;
end;
$$
language plpgsql immutable;

create type myint8_range as range (
    subtype = int8, 
    canonical = myint8_range_canon,
    subtype_diff = myint8_range_diff);


-- Window functions
create function myrank() returns integer as 'window_rank'
language internal window;


-- Leakproof function
create function secure(p1 integer) returns integer as
$$
begin
  return p1 - 3;
end;
$$
language plpgsql leakproof;


-- Security barrier view
create view secure_keys with (security_barrier) as
  select key1, key2 from schema2.keys_table where key1 > 1000;


-- Column privileges
create table cols (
  col1 integer not null,
  col2 integer not null
);

grant select (col1) on cols to keep;


-- Unlogged tables
create unlogged table unlogged (
  col1   text
);


-- Disable/Enable triggers
create trigger mytrigger2 
before insert or update on schema2.keys2
for each row execute procedure public.trigger1();

alter table schema2.keys2 enable always trigger mytrigger2;

create trigger mytrigger3 
before insert or update on schema2.keys2
for each row execute procedure public.trigger1();

alter table schema2.keys2 enable replica trigger mytrigger3;

create trigger mytrigger4
before insert or update on schema2.keys2
for each row execute procedure public.trigger1();

alter table schema2.keys2 disable trigger mytrigger4;


-- Disable/Enable rules
create or replace rule cols_rule1 as on insert to cols
do also insert into 
   schema2.keys2(key1, key2, key3, key4)
   values (new.col1, new.col1, new.col1, new.col1);

alter table cols disable rule cols_rule1;

create or replace rule cols_rule2 as on insert to cols
do also insert into 
   schema2.keys2(key1, key2, key3, key4)
   values (new.col1, new.col1, new.col1, new.col1);

alter table cols enable replica rule cols_rule2;

DBEOF

