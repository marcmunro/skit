#! /bin/sh
# extract_usage.sh
#
# Extract usage text from the skit manual for use by the skit --help
# command

extract_option ()
{
#    TODO, extract/construct the synopsis part as well.

    awk -v start="$2" -v end="$3" '
    	BEGIN {
    	    doprint = 0
            looking = 0
    	}
        /OPTIONS/   {
            looking = 1
        }
    	($0 ~ end) {
    	    if (looking) {exit(0)}
        }
    	($0 ~ start) {
    	    doprint = looking
    	}
    	(doprint == 1) {
    	    print
    	}' $1 
}

awk '
    	BEGIN {
    	    doprint = 0
    	}
    	/OPTIONS/ {
    	    doprint = 0
        }
    	/SYNOPSIS/ {
    	    doprint = 1
    	}
    	(doprint == 1) {
    	    print
    	}' $1 >templates/usage_synopsis.txt


extract_option $1 --dbtype --extract >templates/usage_dbtype.txt
extract_option $1 --extract --scatter >templates/usage_extract.txt
extract_option $1 --scatter --diff >templates/usage_scatter.txt
extract_option $1 --diff --generate >templates/usage_diff.txt
extract_option $1 --generate --list >templates/usage_generate.txt
extract_option $1 --list --adddeps >templates/usage_list.txt
extract_option $1 --adddeps --template >templates/usage_adddeps.txt
extract_option $1 --template --print >templates/usage_template.txt
extract_option $1 --print --help >templates/usage_print.txt
extract_option $1 --help --version >templates/usage_usage.txt
extract_option $1 --version EXIT >templates/usage_version.txt
