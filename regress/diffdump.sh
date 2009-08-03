#! /bin/sh
# Check for substantive differences between two pg_dump files.

# Start by looking only at the number of created objects.
grep '^CREATE' $1 | sort >$1.creates
grep '^CREATE' $2 | sort | diff $1.creates - >$1.diffs
status=$?
rm -f $1.creates
if [ ${status} -ne 0 ]; then
    lost=`grep '^<' $1.diffs | wc -l`
    extra=`grep '^>' $1.diffs | wc -l`
    cat $1.diffs 1>&2
    echo Regression failure: ${lost} objects lost, ${extra} extra objects created 1>&2
    exit ${status}
fi

# Check grants
grep '^GRANT' $1 | sort >$1.creates
grep '^GRANT' $2 | sort | diff $1.creates - >$1.diffs
status=$?
rm -f $1.creates
if [ ${status} -ne 0 ]; then
    lost=`grep '^<' $1.diffs | wc -l`
    extra=`grep '^>' $1.diffs | wc -l`
    cat $1.diffs 1>&2
    echo Regression failure: ${lost} grants lost, ${extra} extra grants created 1>&2
    exit ${status}
fi


# Check comments
grep '^COMMENT' $1 | sort >$1.creates
grep '^COMMENT' $2 | sort | diff $1.creates - >$1.diffs
status=$?
rm -f $1.creates
if [ ${status} -ne 0 ]; then
    lost=`grep '^<' $1.diffs | wc -l`
    extra=`grep '^>' $1.diffs | wc -l`
    cat $1.diffs 1>&2
    echo Regression failure: ${lost} comments lost, ${extra} extra comments created 1>&2
    exit ${status}
fi



rm -f $1.diffs
exit ${status}