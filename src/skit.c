/**
 * @file   skit.c
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
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
    char *execdir;

    if (errcode = regcomp(&regex, expr, REG_EXTENDED)) {
	char errmsg[200];
	(void) regerror(errcode, &regex, errmsg, 199);
	RAISE(REGEXP_ERROR,
	      newstr("skitHome: regexp compilation failure: %s", errmsg));
    }
    if (regexec(&regex, executable, 1, matches, 0) == 0) {
	// Match found
	len = matches[0].rm_so;
	execdir = skalloc(sizeof(char) * (len + 1));
	strncpy(execdir, executable, len);
	execdir[len] = '\0';
	regfree(&regex);
    }
    else
    {
	execdir = newstr(".");
    }
    return execdir;
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
#ifdef MEM_DEBUG
    skitFreeMem();
    if (exceptionCurHandler())
	fprintf(stderr, "There is still an exception handler in place!\n");
    if (memchunks_in_use() != 0) {
	showChunks();
	fprintf(stderr, "There are still %d memory chunks allocatedi.\n",
		memchunks_in_use());
    }
    memShutdown();
#endif
}

int
main(int argc,
     char *argv[])
{
    char *execdir = skitHome(argv[0]);
    char *templatedir = newstr("%s/", execdir);
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
    skfree(templatedir);
    skfree(execdir);
    shutdown();
    return 0;
}
