--

create table skit_test (
  key int4 not null,
  val text
);

alter table skit_test add constraint skit_test__pk
  primary key (key);

create unique index skit_test__uidx on skit_test(val);

create table skit_test2 (
  key2 int4 not null,
  val2 text
);


create sequence skit_test1_seq maxvalue 99;
comment on sequence skit_test1_seq is
'comment';

create sequence skit_test2_seq;

create type wibbly_type as (
  wibbly_id    int,
  wibbliness   varchar(40),
  wibble_factor int
);

create or replace 
function mychar2in(in cstring)
  returns mychar2
as 'charin'
language internal immutable strict;

create or replace 
function mychar2out(in mychar2)
  returns cstring
as 'charout'
language internal immutable strict;

create type mychar2(
  input = mychar2in,
  output = mychar2out,
  passedbyvalue,
  internallength = 1,
  alignment = char,
  storage = plain,
  delimiter = ',');

create domain postal99
  as mychar2
  default 'x'::mychar2 not null;


create or replace function add2int4(
    _state in int4,
    _next in int4)
  returns int4
as 
$_$
begin
  return _state + _next;
end;
$_$
language plpgsql stable cost 5;

create aggregate mysum99 (
  basetype = int4,
  sfunc = add2int4,
  stype = int4,
  initcond = '0'
);


create or replace 
function mychar2send(in mychar2)
  returns bytea
as 'charsend'
language internal immutable strict;

create cast(mychar2 as bytea)
  with function mychar2send(mychar2)
  as assignment;


create 
function myconv(integer, integer, cstring, internal, integer)
  returns void as '$libdir/ascii_and_mic', 'ascii_to_mic' language c;

create conversion myconv2 for 'SQL_ASCII' to 'MULE_INTERNAL' from myconv;


create schema skit_test;


comment on extension skit_test is
'extension';

