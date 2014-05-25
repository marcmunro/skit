#! /bin/sh
# Check for differences between two postgres dumps.

# Eliminate revokes that are followed by equivalent grants.  These
# pairings are artefacts of pg_dump that appear to be to ensure that the
# restored catalog entry for an object that has default permissions
# which have been granted explicitly is identical to its origin, ie you
# can tell whether the permissions were set explicitly or implicitly
# even though they are equivalent.
#
eliminate_redundant_grants()
{
    awk '
    /^REVOKE/ {
	# Record the revoke line
	priv = $2
	on = $0
	sub(/^REVOKE [A-Z]* ON /, "", on)
	to = on
	sub(/.* FROM /, "", to)
	sub(/ FROM (.*);$/, " ", on)
	key = priv "=" on "=" to
	revokes[key] = $0
	next
    }
    /GRANT/ {
	# If this grant matches a revoke, remove the revoke from the list, 
	# otherwise print the grant
	priv = $2
	on = $0
	sub(/^GRANT [A-Z]* ON /, "", on)
	to = on
	sub(/.* TO /, "", to)
	sub(/ TO (.*);$/, " ", on)
	key = priv "=" on "=" to
	if (key in revokes) {
	    delete revokes[key]
	}
	else {
	    print $0
	}
	next
    }
    {print}
    END {
	# Print the list of unmatched revokes
	for (key in revokes) {
	    print revokes[key]
	}
    }
    ' -
}

# If there are allowed_diffs, we eliminate them from the source files.
# Lines in the ignore file may begin with "< ",  ">  " or ". ".  Lines
# beginning with an angle bracket will be matched against the first or
# second file passed to the diff.  This allows very specific comparisons
# to be made.  More usefully, lines beginning with dot, will be tested
# against both files.
#
eliminate_allowed()
{
    if [ "x$1" = "x2" ]; then
	grep -v '^<' $2
    else
	grep -v '^>' $2
    fi | cut -c 3- >fgrep.tmp
    grep -v -f fgrep.tmp - 
    rm -f fgrep.tmp
}

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

prep_input()
{
    if [ "x$2" = "x" ]; then
	cat - 
    else
	eliminate_allowed $1 $2
    fi | eliminate_redundant_grants | sort_attributes
}

prep_input 1 $3 < $1 >$1.tmp
prep_input 2 $3 < $2 >$2.tmp
echo "=== RUNNING DIFF $1 $2"
diff -b $1.tmp $2.tmp; status=$?
rm -f $1.tmp $2.tmp
if [ ${status} -ne 0 ]; then
    echo 1>&2
    echo "Differences found between $1 and $2.  Bailing out..." 1>&2
fi
exit $status