--

create table skit_test (
  key int4 not null
);

alter table skit_test add constraint skit_test__pk
  primary key (key);

create sequence skit_test1_seq;

create type wibbly_type as (
  wibbly_id    int,
  wibbliness   text
);


create domain postal99
  as mychar;



comment on extension skit_test is
'Extension skit_test';