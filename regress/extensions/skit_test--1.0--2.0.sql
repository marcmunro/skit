--

alter table skit_test add column val text;

create unique index skit_test__uidx on skit_test(val);

create table skit_test2 (
  key2 int4 not null,
  val2 text
);

alter sequence skit_test1_seq maxvalue 99;

comment on sequence skit_test1_seq is
'comment';

create sequence skit_test2_seq;

drop type wibbly_type;

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

drop domain postal99;
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

create rule skit_test2__rule1 as on insert to skit_test2
do also insert into skit_test
          (key)
   values (new.key2);



create or replace 
function trigger99() returns trigger as
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

create trigger mytrigger99
before insert or update on skit_test
for each row execute procedure trigger99();



create text search configuration skit_test (copy = pg_catalog.english);

create text search template mysimple99 (
    init = dsimple_init,
    lexize = dsimple_lexize
);

create text search dictionary public.simple_dict99 (
    template = pg_catalog.simple,
    stopwords = english
);

create text search parser myparser99 (
    start = 'prsd_start',
    gettoken = 'prsd_nexttoken',
    end = 'prsd_end',
    lextypes = 'prsd_lextype',
    headline = 'prsd_headline');

create foreign data wrapper dummy99
    options (debug 'true', abc '99,100');

create server s99
    foreign data wrapper dummy99
    options (debug 'true');

create user mapping for keep
    server s99
    options (user 'general', password 'confusion');

create foreign table films99 (
    code        char(5) not null,
    title       varchar(40) not null,
    prod	date
)
server s99;

create collation collation99 from "C";


comment on extension skit_test is
'extension';
