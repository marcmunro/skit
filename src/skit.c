/**
 * @file   skit.c
 * \code
 *     Copyright (c) 2009 - 2014 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides the command line interface into skitlib.
 */


#include <stdio.h>
#include <string.h>
#include <regex.h>
#include "skit_param.h"
#include "skit_lib.h"
#include "exceptions.h"

/* Return the home directory for skit.  This is where the templates dir
 * lives.
 */
static char *
skitHome(char *executable)
{
    char *expr = "/[^/]*$";
    regex_t regex;
    regmatch_t matches[1];
    int errcode;
    int len;
    char *homedir;

    if (errcode = regcomp(&regex, expr, REG_EXTENDED)) {
	char errmsg[200];
	(void) regerror(errcode, &regex, errmsg, 199);
	RAISE(REGEXP_ERROR,
	      newstr("skitHome: regexp compilation failure: %s", errmsg));
    }
    if (regexec(&regex, executable, 1, matches, 0) == 0) {
	// Match found
	len = matches[0].rm_so;
	homedir = skalloc(sizeof(char) * (len + 1));
	strncpy(homedir, executable, len);
	homedir[len] = '\0';
	regfree(&regex);
	return homedir;
    }
    else
    {
	RAISE(FILEPATH_ERROR,
	      newstr("skitHome: No '/' character found in %s", executable));
    }
    return NULL;
}

static void
process_args(int argc,
	     char *argv[])
{
    Hash *params;
    String *action;
    record_args(argc, argv);
    while (action = nextAction()) {
	params = parseAction(action);
	executeAction(action, params);
    }
    finalAction();
}

static void
shutdown()
{
#ifdef DEBUG
    fprintf(stderr, "THIS IS A DEBUG BUILD\n");
#endif
}

int
main(int argc,
     char *argv[])
{
    char *homedir = skitHome(argv[0]);
    char *templatedir = newstr("%s/", homedir);
    skit_register_signal_handler();
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(templatedir);
	//showFree(184168);
	//showMalloc(64101);
	process_args(argc, argv);

    }
    EXCEPTION(ex) {
	fprintf(stderr, "Error: %s\n", ex->text);
	return 1;
    }
    END;
    /* BEGIN DEBUG CODE SECTION
     * This should be commented out in a live version
     */
    skfree(templatedir);
    skfree(homedir);
    skitFreeMem();
    if (exceptionCurHandler())
	fprintf(stderr, "There is still an exception handler in place!\n");
    if (memchunks_in_use() != 0) {
	showChunks();
	fprintf(stderr, "There are still %d memory chunks allocatedi.\n",
		memchunks_in_use());
    }
    memShutdown();
    /* END DEBUG CODE SECTION */
    shutdown();
    return 0;
}
