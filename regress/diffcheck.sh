#! /bin/sh
# Check for differences between two xml schema dumps.

# Put attributes of elements into a consistent order. 
sort_attributes()
{
    sed -e '/<grant / s/\(.*\)\( from="[^"]*"\)\(.*\)\/>/\1\3\2\/>/' | \
    sed -e '/<grant / s/\(.*\)\( to="[^"]*"\)\(.*\)\/>/\1\3\2\/>/' | \
    sed -e '/<grant / s/\(.*\)\( with_[a-z]*="[^"]*"\)\(.*\)\/>/\1\3\2\/>/' | \
    sed -e '/^SET / d' | sed -e 's/^--.*//' | grep . | \
    sort
}

sort_attributes < $1 | sed -e '/dump dbtype/ s/time="[0-9]*"//' >$1.tmp
sort_attributes < $2 | sed -e '/dump dbtype/ s/time="[0-9]*"//' >$2.tmp
diff -b $1.tmp $2.tmp; status=$?
rm -f $1.tmp
if [ ${status} -ne 0 ]; then
    echo 1>&2
    echo "Differences found between $1 and $2.  Bailing out..." 1>&2
fi
exit $status