/**
 * @file   action.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides functions for parsing and executing actions.  An action is a
 * high level skit command implemented from a single xml file.
 * Processing a template file, for example, is an action.
 */

#include <stdio.h>
#include "skit_lib.h"
#include "skit_param.h"
#include "exceptions.h"

static String action_str = {OBJ_STRING, "action"};
static String template_str = {OBJ_STRING, "template"};
static String sources_str = {OBJ_STRING, "sources"};
static String value_str = {OBJ_STRING, "value"};
static String type_str = {OBJ_STRING, "type"};
static String default_str = {OBJ_STRING, "default"};
static String add_deps_str= {OBJ_STRING, "add_deps"};
static String add_deps_filename = {OBJ_STRING, "add_deps.xsl"};
static Document *adddeps_document = NULL;
static Cons *action_stack = NULL;

static void
actionStackPush(Object *obj)
{
    action_stack = consNew(obj, (Object *) action_stack);
}

Object *
actionStackPop()
{
    Cons *front = action_stack;
    Object *result = NULL;
    if (action_stack) {
	action_stack = (Cons *) action_stack->cdr;
	front->cdr = NULL;
	result = front->car;
	objectFree((Object *) front, FALSE);
    }
    return result;
}

/* Count starts at 1 */
static Cons *
actionStackNth(int n)
{
    Cons *nth = action_stack;
    while ((n > 1) && nth) {
	nth = (Cons *) nth->cdr;
	n--;
    }
    return nth;
}

static Document *
getAddDepsDoc()
{
    if (!adddeps_document) {
	String *add_deps_path = findFile(&add_deps_filename);
	if (!add_deps_path) {
	    RAISE(FILEPATH_ERROR, 
		  newstr("Cannot find \"%s\"", add_deps_filename.value));
	}
	adddeps_document = docFromFile(add_deps_path);
	objectFree((Object *) add_deps_path, TRUE);
	finishDocument(adddeps_document);
    }
    return adddeps_document;
}

void
freeStdTemplates()
{
    if (adddeps_document) {
	objectFree((Object *) adddeps_document, TRUE);
    }
    adddeps_document = NULL;
}

/* Load an input file into memory and place it on the stack for
 * subsequent processing.  If 
 */
void
loadInFile(String *filename)
{
    Document *doc = docFromFile(filename);

    actionStackPush((Object *) doc);
}

static int
getIntOption(Cons *optionlist, String *option_name, String *option_value)
{
    Object *obj = optionlistGetOptionValue(optionlist, option_name, 
					   option_value);
    char *tmp;
    char *exception_str;
    if (!obj) {
	RAISE(PARAMETER_ERROR, 
	      newstr("No value %s provided for option %s",
		     option_value->value, option_name->value));
    }
    if (obj->type != OBJ_INT4) {
	exception_str = newstr(
	    "Incorrect type of value %s provided for option %s",
	    tmp = objectSexp(obj), option_name->value);
	skfree(tmp);
	objectFree(obj, TRUE);
	RAISE(PARAMETER_ERROR, exception_str);
    }
    return ((Int4 *) obj)->value;
}

// TODO: Maybe capture exceptions for better error message formatting.
static Object *
getOptionValue(Cons *optionlist, String *option)
{
    Cons *optionalist = (Cons *) optionlistGetOption(optionlist, option);
    String *type;
    Object *value;
    String *param;
    boolean is_option;

    if (!optionalist) {
	return NULL;
    }

    type = (String *) alistGet(optionalist, (Object *) &type_str);
    if (type) {
	type = stringLower(type);
    }

    value = alistGet(optionalist, (Object *) &value_str);
    if (value) {
	RAISE(PARAMETER_ERROR, 
	      newstr("Option %s unexpected", option->value));
    }
    if (streq(type->value, "flag")) {
	/* If this is a flag type, then it's presence indicates a true
	 * value, ie no parameter is expected */
	if (param) {
	    RAISE(PARAMETER_ERROR, 
		  newstr("Value (%s) unexpected for option %s", 
			 param->value, option->value));
	}
	value = (Object *) symbolGet("t");
    }
    else {
	(void) nextArg(&param, &is_option);

	if (is_option || !param) {
	    // MAKE THIS SHOW THE USAGE MESSAGE!
	    objectFree((Object *) type, TRUE);
	    objectFree((Object *) value, TRUE);
	    objectFree((Object *) param, TRUE);
	    RAISE(PARAMETER_ERROR, 
		  newstr("Argument not provided for option %s", 
			 option->value));
	}
	value = validateParamValue(type, param);
	objectFree((Object *) param, TRUE);
    }

    objectFree((Object *) type, TRUE);
    return value;
}


static Object *
addUnprovidedValue(Object *obj, Object *params)
{
    Object *key = ((Cons *) obj)->car;
    Cons *alist = (Cons *) ((Cons *) obj)->cdr;
    Object *value = alistGet(alist, (Object *) &value_str);

    if (!value) {
	value = alistGet(alist, (Object *) &default_str);
    }

    if (value) {
	if (!hashGet((Hash *) params, key)) {
	    hashAdd((Hash *) params, objectCopy(key), objectCopy(value));
	}
    }
    return (Object *) alist;  /* Return the notional contents of the hash
			       * entry */
}


/* Add defaults to the list of params for any unsupplied params that
 * have them. */
static void
addDefaults(Hash *params, Cons *optionlist)
{
    Hash *hash_from_list = (Hash *) consNth(optionlist, 2);
    
    hashEach(hash_from_list, addUnprovidedValue, (Object *) params);
}

/* Read command line args based upon the optionlist parameter, returning
 * a hash of the args.
 */
static Hash *
getTemplateArgs(Cons *optionlist)
{
    String *arg = NULL;
    boolean is_option;
    Object *value = NULL;
    int expected_sources = getIntOption(optionlist, &sources_str, &value_str);
    Hash *params = hashNew(TRUE);

    BEGIN {
	while (nextArg(&arg, &is_option)) {
	    if (is_option) {
		// If this is an option, check its validity and deal with
		// it.  If it is not valid, assume that it is not intended
		// for this action and replace it into the arglist
		
		if (value = getOptionValue(optionlist, arg)) {
		    hashAdd(params, (Object *) arg, value);
		}
		else {
		    /* This option must be for the next action. */
		    unread_arg(arg);
		    break;
		}
	    }
	    else {
		if (expected_sources == 0) {
		    objectFree((Object *) arg, TRUE);
		    objectFree((Object *) params, TRUE);
		    RAISE(PARAMETER_ERROR, newstr("getTemplateArgs: too many "
						  "source files provided"));
		}
		/* Load the input file into memory and place it on the stack
		 * for later processing */
		loadInFile(arg);
		objectFree((Object *) arg, TRUE);
		expected_sources--;
	    }
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) arg, TRUE);
	objectFree((Object *) value, TRUE);
	objectFree(params, TRUE);
    }
    END;

    addDefaults(params, optionlist);
    return params;
}

static Object *
doParseTemplate(String *filename)
{
    String *path;
    char *tmp;
    Hash *params = NULL;
    Object *obj;
    Document *template_doc = NULL;

    BEGIN {
	path = findFile(filename);
	if (!path) {
	    RAISE(PARAMETER_ERROR, newstr("doParseTemplate: template %s "
					  "not found", filename->value));
	}
	
	/* Load the template file, to get the option list. */
	template_doc = docFromFile(path);
	params = getTemplateArgs(template_doc->options);
	hashAdd(params, (Object *) stringNew("template_name"), (Object *) path);
	path = NULL;
	hashAdd(params, (Object *) stringNew("template"), 
		(Object *) template_doc);
	template_doc = NULL;
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) params, TRUE);
	objectFree((Object *) path, TRUE);
	objectFree((Object *) template_doc, TRUE);
	RAISE();
    }
    END;

    return (Object *) params;
}

static Object *
parseTemplate(Object *obj)
{
    String *filename;
    Object *action_info;

    BEGIN {
	filename = read_arg();

	action_info = doParseTemplate(filename);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) filename, TRUE);
    }
    END;

    return action_info;
}

static Object *
parseAdddeps(Object *obj)
{
    String *filename = stringNew("add_deps.xml");
    Object *params;
    BEGIN {
	params = doParseTemplate(filename);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) filename, TRUE);
	RAISE();
    }
    END;
    objectFree((Object *) filename, TRUE);
    return params;
}

static void
defineActionParsers()
{
    static boolean done = FALSE;
    if (!done) {
	Symbol adddepsParser = {OBJ_SYMBOL, "parse_adddeps", 
				&parseAdddeps, NULL};
	(void) symbolCopy(&adddepsParser);
	Symbol templateParser = {OBJ_SYMBOL, "parse_template", 
				 &parseTemplate, NULL};
	(void) symbolCopy(&templateParser);
    }
    done = TRUE;
}

/* Identify the args that may be supplied to an action, and parse those
 * args.  Each action will result in a different parser function being
 * executed.  The parser function is called 'parse_<action>' (where
 * action is the parameter to this function).  The actual C function to
 * be executed for the parser_function is determined by the value of the
 * symbol of the same name (eg 'parse_template').  The mapping of C
 * function to symbol is done in defineActionParsers().
 * The result of this function is a cons cell with car being the
 * partially-loaded and parsed xml document relating to the option, if
 * any, and cdr being a hash containing the parsed parameters for the
 * action.  This cons-cell will be passed as the parameter to execute
 * action.
 */
Hash *
parseAction(String *action)
{
    Symbol *action_parser;
    String *parser_name;
    Hash *params;
    char *tmp = NULL;

    defineActionParsers();

    BEGIN {
	parser_name = stringNewByRef(newstr("parse_%s", action->value));

	if (action_parser = symbolGet(parser_name->value)) {
	    params = (Hash *) symbolExec(action_parser, NULL);
	    hashAdd(params, (Object *) stringDup(&action_str), 
		    (Object *) stringDup(action));
	}
	else {
	    skitFail(newstr("%s not implemented\n", parser_name->value));
	}
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) parser_name, TRUE);
	RAISE();
    }
    END;
    objectFree((Object *) parser_name, TRUE);
    return params;
}



void
preprocessSourceDocs(int sources)
{
    int i;
    Cons *cons;
    Document *src_doc;
    Document *result_doc;
    Document *xslsheet;
    Object *add_deps = symbolGetValue("add_deps");
/*
  CODE WAS:
  hashGet((Hash *) params, 
  (Object *) &add_deps_str);
*/
    for (i = 1; i <= sources; i++) {
	cons = actionStackNth(i);
	src_doc = (Document *) cons->car;
	if (add_deps) {
	    xslsheet = getAddDepsDoc();
	    result_doc = applyXSLStylesheet(src_doc, xslsheet);
	    objectFree((Object *) src_doc, TRUE);
	}
	else {
	    result_doc = src_doc;
	}
	cons->car = (Object *) result_doc;
    }
}

Object *
executeTemplate(Object *params)
{
    Document *template = (Document *) symbolGetValue("template");
    Int4 *sources = (Int4 *) symbolGetValue("sources");
    int action_stack_entries = consLen(action_stack);
    String *action_name;
    Document *result;

    // TODO: Ensure sources is retrieved successfully from the hash
    if (action_stack_entries < sources->value) {
	action_name = (String *) symbolGetValue("action");

	// TODO: Ensure action_name is retrieved successfully from the hash
	RAISE(PARAMETER_ERROR, 
	      newstr("Insufficient inputs for %s", action_name->value));
    }

    preprocessSourceDocs(sources->value);
    
    if (result = processTemplate(template)) {
	actionStackPush((Object *) result);
    }

    return NULL;
}

static void
defineActionExecutors()
{
    static boolean done = FALSE;
    if (!done) {
	Symbol adddepsExecutor = {OBJ_SYMBOL, "execute_adddeps", 
				  &executeTemplate, NULL};
	(void) symbolCopy(&adddepsExecutor);
	Symbol templateExecutor = {OBJ_SYMBOL, "execute_template", 
				   &executeTemplate, NULL};
	(void) symbolCopy(&templateExecutor);
    }
    done = TRUE;
}

static Object *
setVarFromParam(Object *obj, Object *params)
{
    String *key = (String *) ((Cons *) obj)->car;
    Object *value = ((Cons *) obj)->cdr;
    Symbol *sym;

    //printSexp(stderr, "KEY: ", key);
    //printSexp(stderr, "VALUE: ", value);

    if (!(sym = symbolGet(key->value))) {
	/* No symbol defined, so create a new one */
	sym = symbolNew(key->value);
    }
    //printSexp(stderr, "SYM: ", sym);
    //printSexp(stderr, "SYM VALUE: ", sym->value);
    setScopeForSymbol(sym);
    sym->value = value;
    return NULL;
    //((Cons *) obj)->cdr = NULL;
    return value;
}

static void
setVarsFromParams(Hash *params)
{
    hashEach(params, setVarFromParam, NULL);
}

void
executeAction(String *action, Hash *params)
{
    Symbol *action_executor;
    String *executor_name;
    Object *result;
    char *tmp = NULL;
    defineActionExecutors();

    newSymbolScope();
    setVarsFromParams(params);

    symbolSet("params", (Object *) objRefNew((Object *) params));

    executor_name = stringNewByRef(newstr("execute_%s", action->value));
    BEGIN {
	if (action_executor = symbolGet(executor_name->value)) {
	    result = symbolExec(action_executor, NULL);
	}
	else {
	    skitFail(newstr("%s not implemented\n", executor_name->value));
	}
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	finishWithConnection();
	symbolSet("params", NULL);
	objectFree((Object *) params, TRUE);
	objectFree((Object *) executor_name, TRUE);
	dropSymbolScope();
	RAISE();
    }
    END;

    finishWithConnection();
    symbolSet("params", NULL);
    //printSexp(stderr, "PARAMS: ", (Object *) params);
    objectFree((Object *) params, TRUE);
    objectFree((Object *) executor_name, TRUE);
    dropSymbolScope();
}
