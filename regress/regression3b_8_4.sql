
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
 encoding 'UTF-8'
 tablespace "pg_default"
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

grant create on tablespace tbs3 to keep;

CLUSTEREOF

