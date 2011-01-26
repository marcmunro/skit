#! /bin/sh
dir=`dirname $0`
INITDB=/usr/lib/postgresql/8.3/bin/initdb
${INITDB} --pgdata ${dir}/../testdb
cp ${dir}/../dbscript/postgresql.conf ${dir}/../testdb
/postmaster -D ${dir}/../testdb \
    -p 54329 > ${dir}/../testdb/logfile 2>&1 &
mkdir -p ${dir}/../testdb/tbs/tbs2
mkdir -p ${dir}/../testdb/tbs/tbs3
mkdir -p ${dir}/../testdb/tbs/tbs4
mkdir -p ${dir}/../testdb/tbs/TBS4
