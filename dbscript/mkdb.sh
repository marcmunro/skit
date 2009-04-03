#! /bin/sh
/usr/lib/postgresql/8.3/bin/initdb --pgdata /home/marc/proj/skit2/testdb
cp /home/marc/proj/skit2/dbscript/postgresql.conf /home/marc/proj/skit2/testdb
/postmaster -D /home/marc/proj/skit2/testdb \
    -p 54329 > /home/marc/proj/skit2/testdb/logfile 2>&1 &
mkdir -p /home/marc/proj/skit2/testdb/tbs/tbs2
mkdir -p /home/marc/proj/skit2/testdb/tbs/tbs3
mkdir -p /home/marc/proj/skit2/testdb/tbs/tbs4
