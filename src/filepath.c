/**
 * @file   filepath.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Determine where to find files based on path and version information
 *
 */

#include <stdio.h>
#include "skit_lib.h"
#include "exceptions.h"
#include <glob.h>
#include <string.h>


static glob_t glob_buf;
static int glob_buf_idx = 0;
static boolean glob_buf_freed = TRUE;

static void
freeGlobBuf()
{
    if (!glob_buf_freed) {
	globfree(&glob_buf);
    }
}


// Use glob to get an array of filepaths matching path.  Return the
// count of filepaths
static int
get_matches(char *path)
{
    freeGlobBuf();
    glob(path, GLOB_TILDE, NULL, &glob_buf);
    glob_buf_freed = FALSE;
    glob_buf_idx = 0;
    //fprintf(stderr, "LOOKING IN: %s  (%d)\n", path, glob_buf.gl_pathc);
    return glob_buf.gl_pathc;
}

// Return the next filepath set up by get_matches.
static char *
next_match()
{
    return glob_buf.gl_pathv[glob_buf_idx++];
}

// Try finding filename in path/filename, path/dbdir/filename and
// path/dbdir/*/filename.  Return -1 if the file was found in one of the
// two simple (non-wildcarded) paths, count otherwise.
static int
locateFile(char *path, char *templatedir, char *dbdir, char *filename)
{
    int count;
    char *mypath = newstr("%s/%s", path, filename);
    count = get_matches(mypath);
    skfree(mypath);
    if (count) {
	return -1;
    }

    mypath = newstr("%s/%s/%s", path, templatedir, filename);
    count = get_matches(mypath);
    skfree(mypath);
    if (count) {
	return -1;
    }

    mypath = newstr("%s/%s/%s/%s", path, templatedir, dbdir, filename);
    count = get_matches(mypath);
    skfree(mypath);
    if (count) {
	return -1;
    }

    mypath = newstr("%s/%s/%s/*/%s", path, templatedir, dbdir, filename);
    count = get_matches(mypath);
    skfree(mypath);
    return count;
}

// Extract from filepath a version.  The filepath will be in the form
// rootpath/version/filename.  The filename parameter is passed in to
// simplify the identification of the version part of the filepath.
// Return a version object (a list consisting of the version components,
// eg 8.1.4 becomes (8 1 4))
Object *
versionFromMatch(char *filepath, char *filename)
{
    char *version_str;
    char *tmpstr;
    int   name_len = strlen(filename);
    int   filepath_len = strlen(filepath);
    int   end_idx = filepath_len - name_len - 2;
    int   start_idx;
    Object *version;
    // look back through filepath til we find the previous '/'
    for (start_idx = end_idx; start_idx > 0; start_idx--) {
	if (filepath[start_idx] == '/') {
	    break;
	}
    }

    // start_idx is the position of the '/' character, end_idx is
    // the end of the version string.
    version_str = skalloc(1 + end_idx - start_idx);
    strncpy(version_str, filepath + start_idx + 1, end_idx - start_idx);
    version_str[end_idx - start_idx] = '\0';

    // Now create a version object from the version string
    //traceOn(TRUE);
    tmpstr = newstr("(version '%s')", version_str);
    version = evalSexp(tmpstr);
    skfree(tmpstr);
    //traceOn(FALSE);
    skfree(version_str);
    return version;
}


// For each root in turn we look for a file that matches filename in:
//    root
//    root/templatedir
//    root/templatedir/dbdir
//    root/templatedir/dbdir/version
// If more than one is found in root/sub/version, then the match
// is on the match with the lowest version >= version
char *
pathToFile(Vector *roots, String *templatedir, String *dbdir, 
	   Object *version, String *filename)
{
    String *root;
    char *match;                  // Contains string managed by glob
    char *best_match_str = NULL;  // Contains string managed by glob
    Object *best_match = NULL;
    Object *match_version;
    int matches;
    int i;
    int cmp;
    //printSexp(stderr, "VERSION: ", version);
    for (i = 0; i < roots->elems; i++) {
	root = (String *) roots->contents->vector[i];

	if (matches = locateFile(root->value, templatedir->value,
				 dbdir->value, filename->value)) {
	    if (matches == -1) {
		return newstr(next_match());
	    }

	    // Any matches must be from versioned subdirectories.  Check
	    // each match and return the most appropriate (if any).
	    while (match = next_match()) {
		//fprintf(stderr, "CHECKING MATCH %s\n", match);
		match_version = versionFromMatch(match, filename->value);
		//printSexp(stderr, "Match version", match_version);
		//printSexp(stderr, "Version", version);

		cmp = objectCmp(match_version, version);

		if (cmp == 0) {
		    // This is a perfect match.  No need to look any
		    // further.
		    objectFree(match_version, TRUE);
		    best_match_str = match;
		    break;
		}
		else if (cmp < 0) {
		    // This is a version of the file from earlier than
		    // the current db version.  This means it is usable
		    // unless there is something even more recent.
		    if (best_match) {
			// Compare the version with any previously
			// identified match and take the latest.
			if (objectCmp(match_version, best_match) > 0) {
			    // The new match is better than the old
			    objectFree(best_match, TRUE);
			    best_match = match_version;
			    best_match_str = match;
			}
			else {
			    objectFree(match_version, TRUE);
			}
		    }
		    else {
			best_match = match_version;
			best_match_str = match;
		    }
		}
		else {
		    objectFree(match_version, TRUE);
		}
	    }
	}
    }
    if (best_match_str) {
	if (best_match) {
	    objectFree(best_match, TRUE);
	}
	return newstr(best_match_str);
    }
    return NULL;
}

// Simpler interface to above, using built-in symbols to provide the
// fixed(-ish) parameters.
String *
findFile(String *filename)
{
    Vector *roots = (Vector *) symbolGetValue("template-paths");
    String *tmpltdir = (String *) symbolGetValue("templates-dir");
    String *dbdir = (String *) symbolGetValue("dbtype");
    Object *ver = symbolGetValue("dbver");
    char   *path;
    String *result;
    //printSexp(stderr, "ROOTS: ", roots);
    //printSexp(stderr, "TMPLATE: ", tmpltdir);
    //printSexp(stderr, "DBDIR: ", dbdir);
    //printSexp(stderr, "VER: ", value);
    if (!ver) {
	ver = symbolGetValue("dbver-from-source");
    }
    path = pathToFile(roots, tmpltdir, dbdir, ver, filename);
    freeGlobBuf();
    if (path) {
	result = stringNewByRef(path);
	return result;
    }
    return NULL;
}

static long
fileLen(FILE *fp)
{
    long result;
    fseek(fp, 0L, SEEK_END);
    result = ftell(fp);  
    rewind(fp);
    return result;
}

String *
readFile(String *filename)
{
    String *filepath = findFile(filename);
    String *result = NULL;
    FILE *fp;
    long len;
    char *str;
    char *msg = NULL;
    if (filepath) {
	if (fp = fopen(filepath->value, "r")) {
	    if (str = skalloc((len = fileLen(fp)) + 1)) {
		fread(str, len, 1, fp);
		str[len] = '\0';
		result = stringNewByRef(str);
	    }
	    else {
		msg = newstr("Cannot allocate memory to read file %s",
			     filepath->value);
	    }
	}
	fclose(fp);
    }
    objectFree((Object *) filepath, TRUE);

    if (msg) {
	RAISE(GENERAL_ERROR, msg);
    }

    return result;
}
