--
drop sequence skit_test2_seq;
alter sequence skit_test1_seq no maxvalue;

drop table skit_test2;
drop index skit_test__uidx;
alter table skit_test drop column val;

comment on sequence skit_test1_seq is null;


drop type wibbly_type;

create type wibbly_type as (
  wibbly_id    int,
  wibbliness   text
);


drop domain postal99;


alter extension skit_test drop function mychar2out(in mychar2);
alter extension skit_test drop function mychar2in(in cstring);
alter extension skit_test drop type mychar2;
alter extension skit_test drop cast(mychar2 as bytea);
alter extension skit_test drop function mychar2send(mychar2);
drop type mychar2 cascade;

create domain postal99
  as mychar;


drop aggregate mysum99(int4);
drop function add2int4(int4,int4);

drop conversion myconv2;
drop function myconv(integer, integer, cstring, internal, integer);

drop schema skit_test;


comment on extension skit_test is
'Extension skit_test';

