#! /bin/sh
# Check for differences between two postgres dumps.

# This does some minor filtering on the source files, to ensure 
# diffs that are irrelevant are not found.  Also sorts the files
# so that they will have contents in the same order.
sort_attributes()
{
    sed -e '/^SET / d' | sed -e 's/^--.*//' | grep . | \
	sed -e '/dump dbtype/ s/time="[0-9]*"//' | \
	sed -e "/CREATE TABLESPACE/ s/LOCATION '[^']*'//" | \
	sort
}

sort_attributes < $1 >$1.tmp
sort_attributes < $2 >$2.tmp
diff -b $1.tmp $2.tmp; status=$?
rm -f $1.tmp
if [ ${status} -ne 0 ]; then
    echo 1>&2
    echo "Differences found between $1 and $2.  Bailing out..." 1>&2
fi
exit $status