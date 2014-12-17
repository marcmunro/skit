#! /bin/sh


./skit --diff "$1" "$2" | \
    grep 'diff="\(gone\|new\|diff\|rebuild\)"' | \
    grep -v 'type="grant"' |
    if [ "x$3" = "x" ]; then
	grep .
    else
        grep -v '^#' "$3" | grep . >fgrep.tmp
	grep -e -f fgrep.tmp
    fi
[ $? -ne 0 ]
status=$?
rm -f grep.tmp
exit ${status}