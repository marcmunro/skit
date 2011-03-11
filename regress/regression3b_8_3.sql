
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
 encoding 'UTF8'
 tablespace "tbs3"
 connection limit = -1;
\connect regressdb

comment on database "regressdb" is
'New comment';
\connect postgres

\set tbs2dir '''':home'/regress/REGRESSDB/tbs/tbs2'''
create tablespace "tbs2" owner "keep"
  location :tbs2dir;

comment on tablespace tbs2 is 'This is the second tablespace';

set session authorization 'regress';
grant create on tablespace "tbs3" to "keep";
reset session authorization;

\set tbs4dir '''':home'/regress/REGRESSDB/tbs/TBS4'''
create tablespace "tbs4" owner "wibble"
  location :tbs4dir;

CLUSTEREOF
 
