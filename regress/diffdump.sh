#! /bin/sh
# Check for differences between two postgres dumps.

# Eliminate revokes that are immediately followed by equivalent 
# grants.  These pairings are artefacts of pg_dump that appear to be
# to ensure that the restored catalog entry for an object that has
# default permissions which have been granted explicitly is identical to
# its origin, ie you can tell whether the permissions were set
# explicitly or implicitly even though they are equivalent.
#
eliminate_redundant_grants()
{
    awk '
        BEGIN {
            have_line = 0
        }
        /^GRANT/ {
            if (have_line) {
            	privs2 = $0
            	sub(/GRANT /, "", privs2)
            	sub(/ ON.*/, "", privs2)
            	object2 = $0
            	sub(/ TO .*/, "", object2)
            	sub(/.* ON /, "", object2)
            	to2 = $0
            	sub(/.* /, "", to2)
                if ((object == object2) &&
                    (privs == privs2) &&
                    (to == to2))
                {
                    # This grant matches the previous revoke so do not
                    # print the saved revoke or this grant
                    have_line = 0
                    next
                }
            }
        }
        (have_line) {
            # Print the saved revoke line
            print line
            have_line = 0
        }
        /^REVOKE/ {
            privs = $0
            sub(/REVOKE /, "", privs)
            sub(/ ON.*/, "", privs)
            object = $0
            sub(/ FROM .*/, "", object)
            sub(/.* ON /, "", object)
            to = $0
            sub(/.* /, "", to)
            have_line = 1
            line = $0
            next
        }
        {print}'
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
    eliminate_redundant_grants | sort_attributes
}


prep_input < $1 >$1.tmp
prep_input < $2 >$2.tmp
echo "=== RUNNING DIFF $1 $2"
diff -b $1.tmp $2.tmp; status=$?
rm -f $1.tmp
if [ ${status} -ne 0 ]; then
    echo 1>&2
    echo "Differences found between $1 and $2.  Bailing out..." 1>&2
fi
exit $status