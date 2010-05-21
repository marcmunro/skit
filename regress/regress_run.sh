#! /bin/sh

errcheck()
{
    iam=`whoami`
    awk '
{print}
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
    echo ==== BUILDING DB FROM $1 ====
    sh ${REGRESS_DIR}/$1 2>&1 | errcheck
    echo ==== FINISHED BUILD FROM $1 ====
}

dump_db()
{
    echo $3taking db snapshot... 1>&2
    ${PG_DUMP} $1 >${REGRESS_DIR}/$2
    errexit
}

extract()
{
    echo $3running skit extract... 1>&2
    ./skit --extract --connect "$1" >${REGRESS_DIR}/$2
    errexit
}

scatter()
{
    echo ...running skit extract with scatter... 1>&2
    ./skit --extract --connect "$1" --scatter --path=${REGRESS_DIR}/$2
    errexit
}

gendrop()
{
    echo ......drop... 1>&2
    ./skit --generate --drop ${REGRESS_DIR}/$1 >${REGRESS_DIR}/tmp
    errexit
    echo .........editing drop script to enable dangerous drop statements... 1>&2
    sed -e  "/drop role[ \"]*`whoami`[\";]/! s/^-- //" \
	    ${REGRESS_DIR}/tmp >${REGRESS_DIR}/$2
}

genbuild()
{
    echo ......build... 1>&2
    ./skit --generate --build ${REGRESS_DIR}/$1 >${REGRESS_DIR}/tmp
    errexit
    echo .........editing build script to not create role `whoami`... 1>&2
    sed -e  "/create role \"`whoami`\"/ s/^/-- /" \
	    ${REGRESS_DIR}/tmp > ${REGRESS_DIR}/$2
}

genboth()
{
    echo ......both... 1>&2
    ./skit --generate --build --drop ${REGRESS_DIR}/$1 >${REGRESS_DIR}/$2
    errexit
}

execdrop()
{
    echo ...executing drop script... 1>&2
    echo ==== RUNNING DROP SCRIPT $1 ==== 
    sh ${REGRESS_DIR}/$1 2>&1 | errcheck
    errexit
    echo ...checking that database is empty... 1>&2
    pg_dumpall -p ${REGRESSDB_PORT} | grep ^CREATE | wc -l | \
            grep ^1$ >/dev/null
    errexit
    echo ==== FINISHED DROP SCRIPT $1 ==== 
}

execbuild()
{
    echo ...executing build script... 1>&2
    echo ==== RUNNING BUILD SCRIPT $1 ==== 
    sh ${REGRESS_DIR}/$1 2>&1 | errcheck
    errexit
}

errexit()
{
    status=$?
    if [ ${status} != 0 ]; then
	exit ${status}
    fi
}

diffdump()
{
    echo ......comparing pg_dump snapshots... 1>&2
    regress/diffdump.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2
    errexit
}

diffextract()
{
    echo ......checking results of extracts... 1>&2
    regress/diffcheck.sh ${REGRESS_DIR}/$1 ${REGRESS_DIR}/$2
    errexit
}

regression_test1()
{
    echo "Running regression test 1 (build and drop)..." 1>&2
    mkdir regress/scratch 2>/dev/null
    build_db regression1.sql
    dump_db regressdb scratch/regressdb_test1a.dmp ...

    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump1a.xml ...

    echo ...running skit generate... 1>&2
    gendrop scratch/regressdb_dump1a.xml scratch/regressdb_drop1.sql
    genbuild scratch/regressdb_dump1a.xml scratch/regressdb_build1.sql
    genboth scratch/regressdb_dump1a.xml scratch/regressdb_both1.sql

    execdrop scratch/regressdb_drop1.sql
    execbuild scratch/regressdb_build1.sql

    echo ...checking success of build script... 1>&2
    extract "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/regressdb_dump1b.xml ......
    dump_db regressdb scratch/regressdb_test1b.dmp ......
    diffdump scratch/regressdb_test1a.dmp scratch/regressdb_test1b.dmp
    diffextract scratch/regressdb_dump1a.xml scratch/regressdb_dump1b.xml

    rm 	-f ${REGRESS_DIR}/tmp >/dev/null 2>&1
    echo ==== FINISHED BUILD SCRIPT regressdb_build1.sql ==== 
    echo Regression test 1 complete 1>&2
}

regression_test2()
{
    echo "Running regression test 2 (scatter and gather)..." 1>&2
    rm -rf scratch/dbdump/*
    build_db regression1.sql
    dump_db regressdb scratch/regressdb_test2a.dmp ...

    scatter "dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}" \
	    scratch/dbdump
    
    echo ...running skit generate from scatterfiles... 1>&2
    gendrop scratch/dbdump/cluster.xml scratch/regressdb_drop2.sql
    genbuild scratch/dbdump/cluster.xml scratch/regressdb_build2.sql
    execdrop scratch/regressdb_drop2.sql
    execbuild scratch/regressdb_build2.sql
    dump_db regressdb scratch/regressdb_test2b.dmp ......
    diffdump scratch/regressdb_test2a.dmp scratch/regressdb_test2b.dmp

    rm 	-f ${REGRESS_DIR}/tmp >/dev/null 2>&1
    echo Regression test 2 complete 1>&2
}

export MYPID=$$

if [ "x$1" = "" ]; then
    regression_test1 >${REGRESS_LOG}
    regression_test2 >>${REGRESS_LOG}
fi

if [ "x$1" = "x1" ]; then
    regression_test1 >${REGRESS_LOG}
    shift
fi

if [ "x$1" = "x2" ]; then
    regression_test2 >>${REGRESS_LOG}
    shift
fi




exit
