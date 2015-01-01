
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

alter tablespace tbs2 set (seq_page_cost = 1.5, random_page_cost = 4.2);

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

psql -d regressdb -U regress -v contrib=${pg_contrib} <<'DBEOF'

-- Languages

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


-- Casts
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



-- Operators
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

create operator "public".<= (
  leftarg = "public"."wib",
  rightarg = "public"."wib",
  procedure = "public"."wib_lt",
  commutator = "public".">",
  negator = "public".">=",
  restrict = "pg_catalog"."scalarltsel",
  join = "pg_catalog"."scalarltjoinsel");


-- Operator Class
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

create operator "public".<< (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_left",
  commutator = "public".">>",
  restrict = "pg_catalog"."positionsel",
  join = "pg_catalog"."positionjoinsel");

create operator "public".<> (
  leftarg = "public"."seg",
  rightarg = "public"."seg",
  procedure = "public"."seg_different",
  commutator = "public"."<>",
  negator = "public"."=",
  restrict = "pg_catalog"."neqsel",
  join = "pg_catalog"."neqjoinsel");

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


create operator class "public"."seg_ops2"
  for type "public"."seg" using btree as
    operator 1  "public".<,
    operator 2  "public".<=,
    operator 5  "public".>,
    operator 4  "public".>=,
    operator 3  "public".=,
    function 1  "public"."seg_cmp"("public"."seg","public"."seg");

comment on operator class "public"."seg_ops2" using btree is
'operator class for seg_ops2';


-- Operator Family
comment on operator family "public"."seg_ops" using btree is
'operator family for seg_ops';

create operator family seg_ops3 using btree;
alter operator family seg_ops3 using btree add operator 1 <(seg, seg);
alter operator family seg_ops3 using btree add function 1 seg_cmp(seg, seg);

comment on operator family seg_ops3 using btree is 
'operator family seg_ops3';


create operator family seg_ops4 using btree;
alter operator family seg_ops4 using btree add operator 1 <(seg, seg);
alter operator family seg_ops4 using btree add operator 2 <=(seg, seg);
alter operator family seg_ops4 using btree add function 1 seg_cmp(seg, seg);
alter operator family seg_ops4 using btree owner to keep;

comment on operator family seg_ops4 using btree is 
'operator family seg_ops4';


-- Basetypes
create or replace function "public"."wib2_in"(
    in "pg_catalog"."cstring")
  returns "public"."wib2"
as 'charin'
language internal immutable strict;

create or replace function "public"."wib2_out"(
    in "public"."wib2")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;

create type "public"."wib2"(
  input = "public"."wib2_in",
  output = "public"."wib2_out",
  internallength = 12,
  alignment = int4,
  storage = plain,
  delimiter = ',');

comment on type "public"."wib2" is
'wib2';


create or replace function "public"."wib3_in"(
    in "pg_catalog"."cstring")
  returns "public"."wib3"
as 'charin'
language internal immutable strict;

create or replace function "public"."wib3_out"(
    in "public"."wib3")
  returns "pg_catalog"."cstring"
as 'charout'
language internal immutable strict;


create type "public"."wib3"(
  input = "public"."wib3_in",
  output = "public"."wib3_out",
  internallength = 12,
  alignment = int4,
  storage = plain,
  delimiter = ',');

comment on type "public"."wib3" is
'wib3';


-- Sequences
create sequence "public"."thingy_id_seq"
  start with 1 increment by 1
  minvalue 1 maxvalue 9223372036854775807
  cache 1;

comment on sequence "public"."thingy_id_seq" is
'thingy';

create sequence wibble.x;

-- Tables
-- Test:
--    drop table
--    create table
--    changes to ownership
--    add columns
--    drop columns
--    column comment
--    column: type, size, nullability, default
--    have type of column rebuilt
--    storage
--    statistics
--    inheritence
--    tablespace

-- drop/create
create table x (
  key	integer not null,
  val   varchar(20) not null
);

--  changes to ownership, comment
create table o (
  key	integer not null,
  val   varchar(20) not null
) with (oids);

comment on table o is 
'This is table o';

--  add/drop columns, change size and precision, change nullability and
--  default, and change fillfactor
create table c (
  key	integer not null,
  val1  varchar(20) not null,
  val2  numeric(12,4) not null default 0::numeric,
  val3  numeric(12,4) not null default 0::numeric,
  val4  vv2_t not null,
  val5  text,
  val8  varchar(20) not null
) with (fillfactor = 90);

-- column comments
comment on column c.key is 'key';
comment on column c.val1 is 'val1';
comment on column c.val4 is 'val4';

-- storage policies
alter table c alter column val2 set storage main;
alter table c alter column val3 set storage external;
alter table c alter column val4 set storage external;
alter table c alter column val8 set storage external;

-- stats targets
alter table c alter column key set statistics 1000;
alter table c alter column val2 set statistics 1000;
alter table c alter column val4 set statistics 1000;
alter table c alter column val8 set statistics 1000;

-- Inheritence
create table d (
  val20     varchar
);

create table i (
  val9     varchar(20)
) inherits (c, d);

-- Tablespaces
create table t4 (
  key integer not null
) tablespace tbs4;

alter table t4 owner to lose; -- Lose does not have create rights on tbs4


-- Sequences owned by tables
create sequence xseq owned by x.key;
create sequence oseq owned by o.key;


-- Constraints
alter table t4 add constraint t4__keycheck check (key > 0);
alter table c add constraint c__val1check check (val1 > 'a');
alter table c add constraint c__keycheck check (key > 0);
alter table i add constraint i__pk primary key (key);
comment on constraint i__pk on i is
'This is the pk for i';


comment on constraint c__val1check on c is
'This is a check constraint';

create table i2 (
  key integer not null,
  key2 integer not null
);
alter table i2 add constraint i2__i_fk 
  foreign key(key) references i(key);

alter table i2 add constraint i2__key_uk
  unique(key);


-- Indexes
create unique index i2__key2__uk on i2(key2)
tablespace tbs4;

comment on index i2__key2__uk is
'comment';


-- Rules
create rule i2__rule1 as on insert to i2
do also insert into i 
          (key)
   values (new.key);

-- Triggers
create or replace 
function trigger1() returns trigger as
$_$
begin
  if new.key > 4 then
    return new;
  else
    return null;
  end if;
end;
$_$
language plpgsql;

create or replace 
function trigger2() returns trigger as
$_$
begin
  if new.key > 4 then
    return new;
  else
    return null;
  end if;
end;
$_$
language plpgsql;

create trigger mytrigger1 
before insert or update on i
for each row execute procedure trigger1();

comment on trigger mytrigger1 on i is
'Trigger comment';

create trigger mytrigger2 
before insert or update on i
for each row execute procedure trigger2();

comment on trigger mytrigger2 on i is
'Trigger comment';


-- Views
create view vi as select * from i;
create view vi2 as select key, val1 from i;

create table vi4_t (
  key   integer not null,
  val   text);

create view vi4 as select * from vi4_t;
alter view vi4 owner to keep;
alter view vi4 alter column key set default nextval('public.thingy_id_seq');
comment on view vi4 is 'vi4';

create rule vi4_vrule1 as on insert to vi4
do instead insert into 
   vi4_t(key, val)
   values (new.key, new.val);


-- Text search configuration
create text search configuration skit (copy = pg_catalog.english);

comment on text search configuration skit is
'comment for text search configuration';


-- Text Search template
create text search template mysimple (
    init = dsimple_init,
    lexize = dsimple_lexize
);

comment on text search template mysimple is
'mysimple template';

create text search template mysimple2 (
    init = dsimple_init,
    lexize = dsimple_lexize
);

comment on text search template mysimple is
'mysimple2 template';


-- Text search dictionary
create text search dictionary public.simple_dict (
    template = pg_catalog.simple,
    stopwords = english
);

comment on text search dictionary public.simple_dict is
'dict';


-- Text search parser
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


-- Foreign Data Wrapper
create foreign data wrapper dummy
    options (debug 'true', abc '99,100');

create foreign data wrapper postgresql 
    validator postgresql_fdw_validator;

create foreign data wrapper mywrapper
    options (debug 'true');

grant all on foreign data wrapper mywrapper to keep;

-- Foreign Data Server
create server kong 
    type 'postgresql' version '8.4'
    foreign data wrapper postgresql; 

grant all on foreign server kong to keep;

create server s2 
    foreign data wrapper dummy
    options (debug 'true');

create server stable
    type 'postgresql' version '8.4'
    foreign data wrapper postgresql; 

grant all on foreign server stable to keep;


-- User Mappings
create user mapping for keep
    server kong
    options (user 'general', password 'confusion');

create user mapping for public
    server kong
    options (user 'major', password 'problem');


-- Exclusion constraints (need btree_gist)
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

-- Language ownership
create language plpythonu;


-- Extensions
create extension skit_test;


-- Foreign Tables
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
server stable;

comment on foreign table films2 is
'Another foreign table';


-- Collations
create collation my_collation from "C";
create collation wiblish (locale = 'C');

create table collated (
  col1   text collate wiblish
);


-- Shell Types
create type shell1;


-- Window functions
create function myrank() returns integer as 'window_rank'
language internal window;


-- Column privileges
create table cols (
  col1 integer not null,
  col2 integer not null
);

grant select (col1) on cols to keep;

DBEOF