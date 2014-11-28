#! /bin/sh
#      Copyright (c) 2009 - 2014 Marc Munro
#      Fileset:	skit - a database schema management toolset
#      Author:  Marc Munro
#      License: GPL V3
#
# Manage databse cluster for skit testing

export TEST_HOME=`find . -name check_params.c | xargs dirname`
export PGBIN=`pg_config --bindir`
export PGPORT=54329

create_cluster()
{
    if dbcheck; then
	echo "Looks like postmaster is already running.  Bailing out..."
	exit 2
    fi     
    echo "Creating new postgres database cluster..."
    mkdir -m 755 ${TEST_HOME}/pgdata 2>/dev/null

    ${PGBIN}/initdb --pgdata ${TEST_HOME}/pgdata
    cp ${TEST_HOME}/postgresql.conf ${TEST_HOME}/pgdata
    cp ${TEST_HOME}/pg_hba.conf ${TEST_HOME}/pgdata
    mkdir -m 755 ${TEST_HOME}/pgdata/tbs 2>/dev/null
    mkdir -m 755 ${TEST_HOME}/pgdata/tbs/tbs2 2>/dev/null
    mkdir -m 755 ${TEST_HOME}/pgdata/tbs/tbs3 2>/dev/null
    mkdir -m 755 ${TEST_HOME}/pgdata/tbs/tbs4 2>/dev/null
    mkdir -m 755 ${TEST_HOME}/pgdata/tbs/TBS4 2>/dev/null
}

dbcheck()
{
  ${PGBIN}/psql -p ${PGPORT} -d postgres -c  "select 'o' || 'k'" 2>&1 | \
      grep "ok" >/dev/null 2>&1
}

waitup()
{
    count=$1
    while [ ${count} -gt 0 ]; do
	if dbcheck; then
	    return 0
	fi 
	sleep 1
	count=`expr ${count} - 1`
    done
    return 1
}

waitdown()
{
    count=$1
    while [ ${count} -gt 0 ]; do
	if dbcheck; then
	    sleep 1
	    count=`expr ${count} - 1`
	else
	    return 0
	fi 
    done
    return 1
}

dbstart()
{
    if dbcheck; then
	echo "Looks like postmaster is already running.  Bailing out..."
	exit 2
    fi     
    echo "Starting postmaster..."
    ${PGBIN}/postgres -D ${TEST_HOME}/pgdata -p ${PGPORT} \
	>${TEST_HOME}/pgdata/postgres.log 2>&1 &
    if waitup 5; then
	echo "started"
    else
	echo "FAILED"
	exit 2
    fi
}

dbstop()
{
    if dbcheck; then
        echo "Stopping postmaster..."
        PGPROC=`ps auwwx | awk "/post[g]res.*-p ${PGPORT}/ {print \\$2}"`
        if [ "x${PGPROC}" != "x" ]; then
  	    kill -15 $PGPROC
	    if waitdown 10; then 
	        return 0
	    fi
	    kill -9 $PGPROC
	    if waitdown 10; then 
	        return 0
	    else
	        echo "FAILED"
	        exit 2
	    fi
	fi
    fi
}

destroy_cluster()
{
    if dbcheck; then
	echo "Looks like postmaster is still running.  Bailing out..."
	exit 2
    fi     
    rm -rf ${TEST_HOME}/pgdata 2>/dev/null
}

case "x$1" in
    xcreate) create_cluster;;
    xstart) dbstart;;
    xstop) dbstop;;
    xcheck) dbcheck;;
    xwait) waitup 5;;
    xwaitd) waitdown 5;;
    xdestroy) destroy_cluster;;
    *) echo "Invalid command $1";;
esac
