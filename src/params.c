/**
 * @file   params.c
 * \code
 *     Copyright (c) 2009 Marc Munro
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


static int my_argc;
static int cur_arg = 1;
static char **my_argv = NULL;


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

/* On the first call, record the arguments provided on the command line.
 * On subsequent calls return the next argument from the list,
 * returning NULL when we are done
 */
void
record_args0(int argc, char *argv[])
{
    cur_arg = 1;
    my_argc = argc;
    my_argv = argv;
}

String *
read_arg0()
{
    String *result;
    if (cur_arg < my_argc) {
	result = stringNew(my_argv[cur_arg++]);
	return result;
    }
    return NULL;
}

char *
read_arg_old()
{
    if (cur_arg < my_argc) {
	return my_argv[cur_arg++];
    }
    return NULL;
}

void
unread_arg_old()
{
    cur_arg--;
}

void
unread_arg0(String *arg)
{
    objectFree((Object *) arg, TRUE);
    cur_arg--;
}

char*
usage_msg()
{
    return "Usage: \n"
	"skit [--add[deps] | -a]\n"
	"\n";
}

// Display usage message to dest
void
show_usage(FILE *dest)
{
    fprintf(dest, usage_msg());
}

// Display error and usage message to stderr and then abort
static void
usage_abort(char *errmsg)
{
    fprintf(stderr, "%s\n\n", errmsg);
    fprintf(stderr, usage_msg());
    raise(SIGABRT);
}

static Hash *option_hash;

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
		result = stringNew(arg + 2);  // skip over leading hyphens
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
}


String *
nextAction()
{
    String *arg;
    String *action;
    boolean is_option;
    Hash *option_hash = coreOptionHash();

    (void) nextArg(&arg, &is_option);
    // TODO: We should be checking for the validity of the option as
    // well as the fact that it is an option.
    if (arg) {
	//fprintf(stderr, "ARG \"%s\", is_option %d\n", arg->value, is_option);
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

// This probably needs to be modified based on reading a config file or
// something
void
initTemplatePath(char *arg)
{
    String *path = stringNew(arg);
    Symbol *sym = symbolNew("template-paths");
    Vector *v = vectorNew(10);
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
	      newstr("castParamValue: no value provided"));
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
}

/*
TODO: Figure out what data is required by *all* actions and define
suitable data types to contain that data.  Define protoypes for the
parser and executor functions.  Provide a structure that maps from an
enum into the parser and executors.  Provide a mapping for command-line
arguments to the enums.  Provide dynamic hash tables for fast lookups -
these will be useful elsewhere.  Maybe provide a nice read syntax for
defining those hashes.  

Data required by executors:
  stylesheet name/path/file handle
  stylesheet parameters
  other options
  default database connection (if defined)
  action-specific database connection (if defined)
  They also require access to the actionstack

*/
  
