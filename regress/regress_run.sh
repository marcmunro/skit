#! /bin/sh
#

errcheck()
{
    iam=`whoami`
    awk '
{print}
	# Temporarily ignore this error
/^ERROR:  must be superuser to alter superusers/ {
	next
}
/^ERROR:/ {
    if ($3 != IAM) {
	errors = 1 
	print $0 >"/dev/stderr" 
    }
}
END {if (errors) {exit(1)}}' IAM="\"${iam}\""
    status=$?
    if [ ${status} != 0 ]; then
	exit ${status}
    fi
}

build_db()
{
    echo ...creating base objects... 1>&2
    echo ==== BUILDING DB FROM ${REGRESS_DIR}/$1 $2====
    if [ ! -r "${REGRESS_DIR}/$1" ]; then
	echo "FILE NOT FOUND: \"${REGRESS_DIR}/$1\"" 1>&2
	exit 2
    fi
    exitonerr sh -e ${REGRESS_DIR}/$1 
    echo ==== FINISHED BUILD FROM $1 ====
}

dump_db()
{
    echo $3taking db snapshot... 1>&2
    exitonfail ${PG_DUMP} $1 >${REGRESS_DIR}/$2
}

dump_db_globals()
{
    echo $3taking db globals snapshot... 1>&2
    exitonfail pg_dumpall -p ${REGRESSDB_PORT} --globals-only >${REGRESS_DIR}/$2
}

extract()
{
    echo $3running skit extract... 1>&2
    exitonfail ./skit --extract --connect "$1" >${REGRESS_DIR}/$2
}

scatter()
{
    echo ...running skit extract with scatter... 1>&2
    exitonfail ./skit --extract --connect "$1" --scatter \
	--path=${REGRESS_DIR}/$2
}

gendrop()
{
    echo ......drop... 1>&2
    exitonfail ./skit --generate --drop $3 ${REGRESS_DIR}/$1 >${REGRESS_DIR}/tmp
    echo .........editing drop script to enable dangerous drop statements... 1>&2
    sed -e  "/drop role[ \"]*`whoami`[\";]/! s/^-- //" \
	    ${REGRESS_DIR}/tmp >${REGRESS_DIR}/$2
}

genbuild()
{
    echo ......build... 1>&2
    exitonfail ./skit --generate --build $3 ${REGRESS_DIR}/$1 >${REGRESS_DIR}/tmp
    echo .........editing build script to not create role `whoami`... 1>&2
    sed -e  "/create role \"`whoami`\"/ s/^/-- /" \
	    ${REGRESS_DIR}/tmp > ${REGRESS_DIR}/$2
}

genboth()
{
    echo ......both... 1>&2
    exitonfail ./skit --generate --build --drop $3 ${REGRESS_DIR}/$1 >${REGRESS_DIR}/$2
}

execdrop()
{
    echo ...executing drop script... 1>&2
    echo ==== RUNNING DROP SCRIPT $1 ==== 
    exitonerr sh -e ${REGRESS_DIR}/$1
    echo ...checking that database is empty... 1>&2
    # Expect only the connecting user's role
    # pg_dumpall will contain plpgsql after version 9.0 of postgres, so
    # we ignore that.
    pg_dumpall -p ${REGRESSDB_PORT} | grep ^CREATE | \
	grep -v 'plpgsql' | wc -l | \
            grep ^1$ >/dev/null
    errexit
    echo ==== FINISHED DROP SCRIPT $1 ==== 
}

gendiff()
{
    echo ......generating diff... 1>&2
    echo "==== RUNNING SKIT DIFF $1 $2 -> $3 ===="
    exitonfail ./skit --diff ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2 \
	--generate >${REGRESS_DIR}/tmp

    echo .........editing diff script to enable dangerous drop statements... 1>&2
    sed -e  "/drop role[ \"]*`whoami`[\";]/! s/^-- //" \
	    ${REGRESS_DIR}/tmp >${REGRESS_DIR}/$3
    errexit
    echo ==== FINISHED DIFF $1 $2 ==== 
}

execdiff()
{
    echo ......executing diff script... 1>&2
    echo ==== RUNNING DIFF SCRIPT $1 ==== 
    sh -e ${REGRESS_DIR}/$1 2>&1 | errcheck
    errexit
}

execbuild()
{
    echo ...executing build script... 1>&2
    echo ==== RUNNING BUILD SCRIPT $1 ==== 
    exitonerr sh -e ${REGRESS_DIR}/$1
}

# This should be used in place of cmd; errexit as it gives better feedback.
# Usage: exitonfail cmd params...
#
exitonfail()
{
    cmd=$1
    shift
    ${cmd} "$@"
    status=$?
    if [ ${status} != 0 ]; then
	echo "FAILED, EXECUTING:" 1>&2
	echo ${cmd} $* 1>&2
	echo "EXITING" 1>&2
	exit ${status}
    fi
}

exitonerr()
{
    cmd=$1
    shift
    ${cmd} "$@" 2>&1 | errcheck
    status=$?
    if [ ${status} != 0 ]; then
	echo "FAILED, EXECUTING:" 1>&2
	echo ${cmd} $* 1>&2
	echo "EXITING" 1>&2
	exit ${status}
    fi
}

# Deprecate this following command in favour of exitonfail running the
# command.
#
errexit()
{
    status=$?
    if [ ${status} != 0 ]; then
	exit ${status}
    fi
}

# Do differences on 2 dump files, ignoring lines that match the contents of 
# an optional 3rd file
diffdump()
{
    echo ......comparing pg_dump snapshots... 1>&2
    if [ "x$3" = "x" ]; then
        exitonfail regress/diffdump.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2 
    else
        exitonfail regress/diffdump.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2 \
	    ${REGRESS_DIR}/$3
    fi
}

diffglobals()
{
    echo ......comparing pg_dumpall snapshots... 1>&2
    if [ "x$3" = "x" ]; then
        exitonfail regress/diffdump.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2 
    else
        exitonfail regress/diffdump.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2 \
	    ${REGRESS_DIR}/$3
    fi
}

diffxml()
{
    echo ......comparing skit dumps... 1>&2
    if [ "x$3" = "x" ]; then
        exitonfail regress/diffxml.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2 
    else
        exitonfail regress/diffxml.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2 \
	    ${REGRESS_DIR}/$3
    fi
}


diffextract()
{
    echo ......checking results of extracts... 1>&2
    exitonfail regress/diffcheck.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2
}

regression_test1()
{
    echo "Running regression test 1 (build and drop, simple-sort)..." 1>&2
    mkdir regress/scratch 2>/dev/null
    build_db regression1_`pguver`.sql
    dump_db regressdb scratch/regressdb_test1a.dmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump1a.xml ...
    echo ...running skit generate... 1>&2
    gendrop scratch/regressdb_dump1a.xml scratch/regressdb_drop1.sql \
	--simple-sort
    genbuild scratch/regressdb_dump1a.xml scratch/regressdb_build1.sql \
	--simple-sort
    genboth scratch/regressdb_dump1a.xml scratch/regressdb_both1.sql \
	--simple-sort
    execdrop scratch/regressdb_drop1.sql
    execbuild scratch/regressdb_build1.sql

    echo ...checking success of build script... 1>&2
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump1b.xml ......
    dump_db regressdb scratch/regressdb_test1b.dmp ......
    diffdump scratch/regressdb_test1a.dmp scratch/regressdb_test1b.dmp \
	regression_ignore1.txt
    diffextract scratch/regressdb_dump1a.xml scratch/regressdb_dump1b.xml

    rm 	-f ${REGRESS_DIR}/tmp >/dev/null 2>&1
    echo Regression test 1 complete 1>&2
}

regression_test2()
{
    echo "Running regression test 2 (scatter, gather)..." 1>&2
    mkdir regress/scratch 2>/dev/null
    rm -rf scratch/dbdump/*
    build_db regression2_`pguver`.sql
    dump_db regressdb scratch/regressdb_test2a.dmp ...

    scatter "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/dbdump
    
    echo ...running skit generate from scatterfiles... 1>&2
    gendrop scratch/dbdump/cluster.xml scratch/regressdb_drop2.sql 
    genbuild scratch/dbdump/cluster.xml scratch/regressdb_build2.sql 
    execdrop scratch/regressdb_drop2.sql
    execbuild scratch/regressdb_build2.sql
    dump_db regressdb scratch/regressdb_test2b.dmp ......
    diffdump scratch/regressdb_test2a.dmp scratch/regressdb_test2b.dmp \
	regression_ignore2.txt

    rm 	-f ${REGRESS_DIR}/tmp >/dev/null 2>&1
    echo Regression test 2 complete 1>&2
}

regression_test3()
{
    echo "Running regression test 3 (diffs)..." 1>&2
    mkdir regress/scratch 2>/dev/null
    rm -rf scratch/dbdump/*
    # Get up to date dumps of each of 2 starting databases
    build_db regression3b_`pguver`.sql
    echo ...Creating target diff database... 1>&2
    dump_db regressdb scratch/regressdb_test3b.dmp ...
    dump_db_globals regressdb scratch/regressdb_test3b.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump3b.xml ...
    echo ...running skit generate to create initial drop... 1>&2
    gendrop scratch/regressdb_dump3b.xml scratch/regressdb_drop3b.sql 
    execdrop scratch/regressdb_drop3b.sql

    echo ...Creating source diff database... 1>&2
    build_db regression3a_`pguver`.sql
    dump_db regressdb scratch/regressdb_test3a.dmp ...
    dump_db_globals regressdb scratch/regressdb_test3a.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump3a.xml ...
    echo "...diff source->target..." 1>&2

    gendiff scratch/regressdb_dump3a.xml scratch/regressdb_dump3b.xml \
	scratch/regressdb_diff3a2b.sql  
    execdiff scratch/regressdb_diff3a2b.sql
    echo "...checking db equivalence to target..." 1>&2
    dump_db regressdb scratch/regressdb_test3b2.dmp ...
    dump_db_globals regressdb scratch/regressdb_test3b2.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump3b2.xml ...
    diffdump scratch/regressdb_test3b.dmp scratch/regressdb_test3b2.dmp \
    	regression_ignore3.txt

    diffglobals scratch/regressdb_test3b.gdmp  scratch/regressdb_test3b2.gdmp \
	regression_ignore3.txt
    echo "...diff target->source..." 1>&2
    gendiff scratch/regressdb_dump3b.xml scratch/regressdb_dump3a.xml \
	scratch/regressdb_diff3b2a.sql

    execdiff scratch/regressdb_diff3b2a.sql

    echo "...checking db equivalence to source ..." 1>&2
    dump_db regressdb scratch/regressdb_test3a2.dmp ...
    dump_db_globals regressdb scratch/regressdb_test3a2.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump3a2.xml ...
    diffdump scratch/regressdb_test3a.dmp scratch/regressdb_test3a2.dmp \
	regression_ignore3.txt
    diffglobals scratch/regressdb_test3a.gdmp  scratch/regressdb_test3a2.gdmp \
	regression_ignore3.txt

    # Finally, check for significant diffs between the skit dumps taken
    # before and after running the diffs.
    diffxml scratch/regressdb_dump3a.xml scratch/regressdb_dump3a2.xml \
	regression_xmlignore3.txt
    diffxml scratch/regressdb_dump3b.xml scratch/regressdb_dump3b2.xml \
	regression_xmlignore3.txt

    rm 	-f ${REGRESS_DIR}/tmp >/dev/null 2>&1
    echo Regression test 3 complete 1>&2
}


regression_test4()
{
    echo "Running regression test 4 (diffs)..." 1>&2
    mkdir regress/scratch 2>/dev/null
    rm -rf scratch/dbdump/*
    # Get up to date dumps of each of 2 starting databases
    build_db regression4b_`pguver`.sql
    echo ...Creating target diff database... 1>&2
    dump_db regressdb scratch/regressdb_test4b.dmp ...
    dump_db_globals regressdb scratch/regressdb_test4b.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump4b.xml ...
    echo ...running skit generate to create initial drop... 1>&2
    gendrop scratch/regressdb_dump4b.xml scratch/regressdb_drop4b.sql 
    execdrop scratch/regressdb_drop4b.sql

    echo ...Creating source diff database... 1>&2
    build_db regression4a_`pguver`.sql
    dump_db regressdb scratch/regressdb_test4a.dmp ...
    dump_db_globals regressdb scratch/regressdb_test4a.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump4a.xml ...
    echo "...diff source->target..." 1>&2

    gendiff scratch/regressdb_dump4a.xml scratch/regressdb_dump4b.xml \
	scratch/regressdb_diff4a2b.sql  
    execdiff scratch/regressdb_diff4a2b.sql

    echo "...checking db equivalence to target..." 1>&2
    dump_db regressdb scratch/regressdb_test4b2.dmp ...
    dump_db_globals regressdb scratch/regressdb_test4b2.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump4b2.xml ...
    diffdump scratch/regressdb_test4b.dmp scratch/regressdb_test4b2.dmp \
	regression_ignore4.txt

    diffglobals scratch/regressdb_test4b.gdmp  scratch/regressdb_test4b2.gdmp
    echo "...diff target->source..." 1>&2
    gendiff scratch/regressdb_dump4b.xml scratch/regressdb_dump4a.xml \
	scratch/regressdb_diff4b2a.sql

    execdiff scratch/regressdb_diff4b2a.sql

    echo "...checking db equivalence to source ..." 1>&2
    dump_db regressdb scratch/regressdb_test4a2.dmp ...
    dump_db_globals regressdb scratch/regressdb_test4a2.gdmp ...
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump4a2.xml ...
    diffdump scratch/regressdb_test4a.dmp scratch/regressdb_test4a2.dmp \
	regression_ignore4.txt
    diffglobals scratch/regressdb_test4a.gdmp  scratch/regressdb_test4a2.gdmp

    gendrop scratch/regressdb_dump4a.xml scratch/regressdb_drop4a.sql 
    execdrop scratch/regressdb_drop4a.sql
    rm 	-f ${REGRESS_DIR}/tmp >/dev/null 2>&1
    echo Regression test 4 complete 1>&2
}


# Prep the regression test database for use in temporary unit tests
# This allows make unit to be used for testing against a real database
# while we are developing new functionality.
# This function is very tightly coupled with the unit tests it supports
# and will be re-written and modified as needed.
#
prep_for_unit_test()
{
    echo "Prepping for unit tests..." 1>&2
    mkdir regress/scratch 2>/dev/null
    rm -rf scratch/dbdump/*
    # Get up to date dumps of each of 2 starting databases
    build_db regression3b_`pguver`.sql
    echo ...Creating target diff database... 1>&2
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump3b.xml ...
    echo ...running skit generate to drop target database... 1>&2
    gendrop scratch/regressdb_dump3b.xml scratch/regressdb_drop3b.sql 
    execdrop scratch/regressdb_drop3b.sql
    echo ...Creating source diff database... 1>&2
    build_db regression3a_`pguver`.sql
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump3a.xml ...
    ./skit --diff regress/scratch/regressdb_dump3a.xml \
	 regress/scratch/regressdb_dump3b.xml \
	     >test/data/diffstream1.xml
}

# Return a simplified (2-part) version string.
#
strip_version()
{
    sed 's/.*\([0-9][0-9]*\.[0-9][0-9]*\)\.[0-9][0-9]*.*/\1/'
}

# Get postgres server version by connecting to postgres using psql.
#
pgver()
{
   psql --no-psqlrc --tuples-only --command="select version()" |
       awk '{print $2}' | strip_version
}

# Get postgres version with underscores replacing periods.
#
pguver()
{
    pgver | sed -e 's/\./_/g'
}

# Find the bin directory to be used for postgres executables, etc
#
pgbin()
{
    if cver=`pg_config --version | strip_version`; then
	if [ "x${cver}" = "x`pgver`" ]; then
	    pg_config --bindir
	    return
	fi
    fi
    # Standard pg_config was not found, or did not match our database version.
    ver=`pgver`
    if [ -f /usr/lib/postgresql/${ver}/bin/pg_config ]; then
	/usr/lib/postgresql/${ver}/bin/pg_config --bindir
	return
    fi
    # can't find pg_config, we'll just have to assume the bin is where
    # psql lives
    which psql | sed -e 's!/psql$!!'
}

export MYPID=$$

if [ "x$1" = "xpgbin" ]; then
    pgbin
    exit
fi

if [ "x$1" = "xpglib" ]; then
    `pgbin`/pg_config --pkglibdir
    exit
fi

if [ "x$1" = "xpgconfig" ]; then
    `pgbin`/pg_config
    exit
fi

pgbin=`pgbin`
export pg_contrib=`${pgbin}/pg_config --sharedir`/contrib

if [ "x$1" = "" ]; then
    regression_test1 >${REGRESS_LOG}
    regression_test2 >>${REGRESS_LOG}
    regression_test3 >>${REGRESS_LOG}
    regression_test4 >>${REGRESS_LOG}
fi

if [ "x$1" = "x1" ]; then
    regression_test1 >${REGRESS_LOG}
    shift
fi

if [ "x$1" = "x2" ]; then
    regression_test2 >>${REGRESS_LOG}
    shift
fi

if [ "x$1" = "x3" ]; then
    regression_test3 >>${REGRESS_LOG}
    shift
fi

if [ "x$1" = "x4" ]; then
    regression_test4 >>${REGRESS_LOG}
    shift
fi

if [ "x$1" = "xprep" ]; then
    prep_for_unit_test >>${REGRESS_LOG}
    shift
fi


exit
