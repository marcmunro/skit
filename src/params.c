/**
 * @file   params.c
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides parameter handling functions, etc for dealing with the skit
 * command line interface.
 */

#include <stdio.h>
#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"
#include "skit_param.h"
#include "skit_environment.h"


static Cons *arglist = NULL;

void
record_args(int argc, char *argv[])
{
    int i;
    Cons **next = &arglist;
    objectFree((Object *) arglist, TRUE);

    for (i = 1; i < argc; i++) {
	*next = consNew((Object *) stringNew(argv[i]), NULL);
	next = (Cons **) &((*next)->cdr);
    }
}

String *
read_arg()
{
    Cons *argcons = arglist;
    String *arg = NULL;
    if (argcons) {
	arg = (String *) argcons->car;
	arglist = (Cons *) argcons->cdr;
	skfree((Object *) argcons);
    }
    return arg;
}

void
unread_arg(String *arg, boolean is_option)
{
    String *my_arg;
    if (is_option) {
	/* Need to prepend the '-' flag indicators to arg */
	if (strlen(arg->value) == 1) {
	    my_arg = stringNewByRef(newstr("-%s", arg->value));
	}
	else {
	    my_arg = stringNewByRef(newstr("--%s", arg->value));
	}
	objectFree((Object *) arg, TRUE);
    }
    else {
	my_arg = arg;
    }
    arglist = consNew((Object *) my_arg, (Object *) arglist);
}

char*
usage_msg()
{
    return "Usage: \n"
	"skit [--add[deps] | -a]\n"
	"\n";
}

/* Display usage message to dest */
void
show_usage(FILE *dest)
{
    fprintf(dest, usage_msg());
}

/* Display error and usage message to stderr and then abort */
static void
usage_abort(char *errmsg)
{
    fprintf(stderr, "%s\n\n", errmsg);
    fprintf(stderr, usage_msg());
    exit(1);
}

String *
nextArg(String **p_arg, boolean *p_option)
{
    String *argstr;
    char *arg;
    char *equals;
    String *param;
    String *result = NULL;

    *p_option = FALSE;
    if (argstr = read_arg()) {
	arg = argstr->value;
	if (arg[0] == '-') {
	    *p_option = TRUE;
	    if (arg[1] == '-') {
		/* Option is preceded by '--'.  Check whether it contains
		 * an '=' character which would indicate a parameter */
		if (equals = strchr(arg + 2, '=')) {
		    *equals = '\0';
		    param = stringNew(equals + 1); 
		    unread_arg(param, FALSE);
		}
		result = stringNew(arg + 2);  /* skip over leading
						 hyphens */
		objectFree((Object *) argstr, TRUE);
	    }
	    else {
		result = stringNewByRef(newstr("%c", arg[1]));
		if (strlen(arg + 2)) {
		    param = stringNew(arg + 2);
		    unread_arg(param, FALSE);
		}
		objectFree((Object *) argstr, TRUE);
	    }
	}
	else {
	    result = argstr;
	}
    }
    *p_arg = result;
    return result;
}


String *
nextAction()
{
    String *arg;
    String *action;
    boolean is_option;
    Hash *option_hash = coreOptionHash();

    (void) nextArg(&arg, &is_option);
    if (arg) {
	if (is_option) {
	    if (action = (String *) hashGet(option_hash, (Object *) arg)) {
		objectFree((Object *) arg, TRUE);
		return action;
	    }
	    usage_abort(newstr("Unexpected option %s\n", arg->value));
	}
	usage_abort(newstr("Expecting option, got %s\n", arg->value));
    }
    return NULL;
}

/* Provide a list of places to look for templates, starting from arg and
 * ending with the system's installed templates directory.  
 */
void
initTemplatePath(char *arg)
{
    String *path = stringNew(arg);
    Symbol *sym = symbolNew("template-paths");
    Vector *v = vectorNew(10);
    String *tmp;
    vectorPush(v, (Object *) path);
    tmp = homedir();
    path = stringNewByRef(newstr("%s/%s", tmp->value, "skit"));
    objectFree((Object *) tmp, TRUE);
    vectorPush(v, (Object *) path);
    path = stringNewByRef(newstr(DATA_DIR));
    vectorPush(v, (Object *) path);

    sym->svalue = (Object *) v;
}

Object *
validateParamValue(String *type, String *value)
{
    String *mytype;
    String *myvalue;
    if (!(value && value->value)) {
	RAISE(MISSING_VALUE_ERROR,
	      newstr("castParamValue: no value provided for %s", type->value));
    }

    mytype = stringLower(type);
    
    if (streq(mytype->value, "boolean") ||
	streq(mytype->value, "bool") ||
	streq(mytype->value, "flag")) 
    {
	myvalue = stringLower(value);
	if ((streq(myvalue->value, "true")) ||
	    (streq(myvalue->value, "t")) ||
	    (streq(myvalue->value, "yes")) ||
	    (streq(myvalue->value, "y")) ||
	    (streq(myvalue->value, "1")))
	{
	    objectFree((Object *) mytype, TRUE);
	    objectFree((Object *) myvalue, TRUE);
	    return (Object *) symbolGet("t");
	}
	if ((streq(myvalue->value, "false")) ||
	    (streq(myvalue->value, "f")) ||
	    (streq(myvalue->value, "no")) ||
	    (streq(myvalue->value, "n")) ||
	    (streq(myvalue->value, "0")))
	{
	    objectFree((Object *) mytype, TRUE);
	    objectFree((Object *) myvalue, TRUE);
	    return NULL;
	}
	else {
	    objectFree((Object *) mytype, TRUE);
	    objectFree((Object *) myvalue, TRUE);
	    RAISE(TYPE_MISMATCH, 
		  newstr("validateParamValue: %s is not a valid boolean", 
			 value->value));
	}
    }
    else if (streq(mytype->value, "integer") ||
	     streq(mytype->value, "int"))
    {
	objectFree((Object *) mytype, TRUE);
	if (stringMatch(value, "^[+-]?[0-9][0-9]*$")) {
	    return (Object *) int4New(atoi(value->value));
	}
	else {
	    RAISE(TYPE_MISMATCH, 
		  newstr("validateParamValue: %s is not a valid integer", 
			 value->value));
	}
    }
    else if (streq(mytype->value, "string")) {
	objectFree((Object *) mytype, TRUE);
	return (Object *) stringDup(value);
    }
    else {
	char *msg =  newstr("validateParamValue: Unexpected type %s", 
			    value->value);
	objectFree((Object *) mytype, TRUE);
	RAISE(TYPE_MISMATCH, msg);
    }
    /* Should not reach this point */
    RAISE(OUT_OF_BOUNDS, newstr("validateParamValue: coding error"));
    return NULL;
}
