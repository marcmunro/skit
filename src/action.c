/**
 * @file   action.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
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
static String print_str = {OBJ_STRING, "print"};
static String template_str = {OBJ_STRING, "template"};
static String sources_str = {OBJ_STRING, "sources"};
static String value_str = {OBJ_STRING, "value"};
static String type_str = {OBJ_STRING, "type"};
static String default_str = {OBJ_STRING, "default"};
static String add_deps_str= {OBJ_STRING, "add_deps"};  // TODO: deprecate
static String add_deps_filename = {OBJ_STRING, "add_deps.xsl"};
static String rm_deps_filename = {OBJ_STRING, "rm_deps.xsl"};
static String dbtype_str = {OBJ_STRING, "dbtype"};
static String global_str = {OBJ_STRING, "global"};
static String arg_str = {OBJ_STRING, "arg"};
static Document *adddeps_document = NULL;
static Document *rmdeps_document = NULL;
static Cons *action_stack = NULL;

void
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

Object *
actionStackHead()
{
    if (action_stack) {
	return (Object *) action_stack->car;
    }
    return NULL;
}

// TOO: deprecate
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
	adddeps_document = findDoc(&add_deps_filename);
    }
    return adddeps_document;
}

static Document *
getRmDepsDoc()
{
    if (!rmdeps_document) {
	rmdeps_document = findDoc(&rm_deps_filename);
    }
    return rmdeps_document;
}

static void
applyXSL(Document *xslsheet)
{
    Document *src_doc;
    Document *result_doc;

    src_doc = (Document *) actionStackPop();
    result_doc = applyXSLStylesheet(src_doc, xslsheet);
    objectFree((Object *) src_doc, TRUE);
    actionStackPush((Object *) result_doc);
}

static void
addDeps()
{
    applyXSL(getAddDepsDoc());
}

static void
rmDeps()
{
    applyXSL(getRmDepsDoc());
}

void
freeStdTemplates()
{
    if (adddeps_document) {
	objectFree((Object *) adddeps_document, TRUE);
    }
    adddeps_document = NULL;

    if (rmdeps_document) {
	objectFree((Object *) rmdeps_document, TRUE);
    }
    rmdeps_document = NULL;
}

/* Load an input file into memory and place it on the stack for
 * subsequent processing.
 */
void
loadInFile(String *filename)
{
    Document *doc = docFromFile(filename);

    actionStackPush((Object *) doc);
}

/* Determine whether the current action has a non-source file argument */
static boolean
hasArg(Cons *optionlist)
{
    Object *entry = optionlistGetOption(optionlist, &arg_str);
    return entry != NULL;
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
	      newstr("No value provided for option %s", option_name->value));
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
    String *fullname;
    String *type;
    Object *value;
    String *param = NULL;
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
	    fullname = optionlistGetOptionName(optionlist, option);
	    RAISE(PARAMETER_ERROR, 
		  newstr("Argument not provided for option \"%s\"", 
			 fullname->value));
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
getOptionlistArgs(Cons *optionlist)
{
    String *arg = NULL;
    boolean is_option;
    Object *value = NULL;
    int expected_sources = getIntOption(optionlist, &sources_str, &value_str);
    int expected_args = 0;
    Hash *params = hashNew(TRUE);
    String *fullname;
    
    BEGIN {
	if (expected_sources == 0) {
	    if (hasArg(optionlist)) {
		expected_args = 1;
	    }
	}
	while (nextArg(&arg, &is_option)) {
	    if (is_option) {
		// If this is an option, check its validity and deal with
		// it.  If it is not valid, assume that it is not intended
		// for this action and replace it into the arglist
		
		if (value = getOptionValue(optionlist, arg)) {
		    fullname = optionlistGetOptionName(optionlist, arg);
		    fullname = stringNew(fullname->value);
		    hashAdd(params, (Object *) fullname, value);
		    objectFree((Object *) arg, TRUE);
		    value = NULL; /* This is freed by freeing params! */
		}
		else {
		    /* This option must be for the next action. */
		    unread_arg(arg, TRUE);
		    break;
		}
	    }
	    else {
		if (expected_sources == 0) {
		    if (expected_args > 0) {
			expected_args--;
			hashAdd(params, 
				(Object *) stringNew("arg"), (Object *) arg);
		    }
		    else {
			RAISE(PARAMETER_ERROR, 
			      newstr("getOptionlistArgs: too many "
				     "source files provided"));
		    }
		}
		else {
		    /* Load the input file into memory and place it on the
		     * stack for later processing */
		    loadInFile(arg);
		    objectFree((Object *) arg, TRUE);
		    expected_sources--;
		}
	    }
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) arg, TRUE);
	objectFree((Object *) value, TRUE);
	objectFree((Object *) params, TRUE);
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
	params = getOptionlistArgs(template_doc->options);
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
parseExtract(Object *obj)
{
    String *filename;
    Object *action_info;

    BEGIN {
	filename = stringNew("extract.xml");

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
parseConnect(Object *obj)
{
    String *filename;
    Object *action_info;

    BEGIN {
	filename = stringNew("connect.xml");

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

static Object *
parsePrint(Object *obj)
{
    Hash *args = getOptionlistArgs(printOptionList());
    return (Object *) args;
}

static Object *
parseList(Object *obj)
{
    String *filename = stringNew("list.xml");
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

/* Set the "global" element in hash to t.  This will cause variables
 * defined for this action to be made global rather than in-scope only
 * for the current action.
 */
static void
makeGlobal(Hash *hash)
{
    Symbol *t = symbolGet("t");
    hashAdd(hash, (Object *) stringNew("global"), (Object *) t);
}

/* Get, and delete, the "global" element in hash.  If t, then variables
 * for this action will be made global rather than in-scope only
 * for the current action.
 */
static boolean
getGlobal(Hash *hash)
{
    Object *global = hashDel(hash, (Object *) &global_str);
    return global != NULL;
}

static Object *
parseDbtype(Object *obj)
{
    String *key;
    String *dbtype = read_arg();
    char *str;
    Hash *result;

    if (!dbtype) {
	RAISE(PARAMETER_ERROR,
	      newstr("dbtype requires an argument"));    
    }
    if (checkDbtypeIsRegistered(dbtype)) {
	result = hashNew(TRUE);
	key = stringNew("dbtype");
	hashAdd(result, (Object *) key, (Object *) dbtype);
	makeGlobal(result);
	return (Object *) result;
    }
    else {
	str = newstr("dbtype \"%s\" is not known to skit\n", dbtype->value);
	objectFree((Object *) dbtype, TRUE);
	RAISE(PARAMETER_ERROR, str);
    }
}



static void
defineActionSymbol(char *name, ObjectFn *fn)
{
    Symbol *sym = symbolNew(name);
    sym->fn = fn;
}
		   

static void
defineActionParsers()
{
    static boolean done = FALSE;
    if (!done) {
	defineActionSymbol("parse_extract", &parseExtract);
	defineActionSymbol("parse_template", &parseTemplate);
	defineActionSymbol("parse_connect", &parseConnect);
	defineActionSymbol("parse_print", &parsePrint);
	defineActionSymbol("parse_adddeps", &parseAdddeps);
	defineActionSymbol("parse_dbtype", &parseDbtype);
	defineActionSymbol("parse_list", &parseList);
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
	    RAISE(NOT_IMPLEMENTED_ERROR,
		  newstr("%s not implemented\n", parser_name->value));
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

static void
preprocessSourceDocs(int sources, Object *params)
{
    int i;
    Cons *cons;
    Document *src_doc;
    Document *result_doc;
    Document *xslsheet;
    Object *add_deps = dereference(symbolGetValue("add_deps"));

    for (i = 1; i <= sources; i++) {
	cons = actionStackNth(i);
	src_doc = (Document *) cons->car;
	if (add_deps) {
	    // TODO: Make this conditional on no deps existing
	    xslsheet = getAddDepsDoc();
	    result_doc = applyXSLStylesheet(src_doc, xslsheet);
	    objectFree((Object *) src_doc, TRUE);
	}
	else {
	    result_doc = src_doc;
	}
	addParamsNode(result_doc, params);
	cons->car = (Object *) result_doc;
    }
}

static Object *
executePrint(Object *params)
{
    Int4 *sources = (Int4 *) dereference(symbolGetValue("sources"));
    int action_stack_entries = consLen(action_stack);
    boolean print_full;
    boolean print_xml;
    boolean has_deps;
    String *action_name;
    Document *doc;
    char *docstr;

    // TODO: Ensure sources is retrieved successfully from the hash
    if (action_stack_entries < sources->value) {
	action_name = (String *) dereference(symbolGetValue("action"));

	// TODO: Ensure action_name is retrieved successfully from the hash
	RAISE(PARAMETER_ERROR, 
	      newstr("Insufficient inputs for %s", action_name->value));
    }

    print_full = dereference(symbolGetValue("full")) && TRUE;
    print_xml = dereference(symbolGetValue("xml")) && TRUE;

    doc = (Document *) actionStackHead();
    has_deps = docIsPrintable(doc);

    if (print_full) {
	if (!has_deps) {
	    addDeps();
	}
    }
    else {
	if (has_deps) {
	    rmDeps();
	}
    }
    doc = (Document *) actionStackPop();

    if (docIsPrintable(doc) && (!print_xml) && (!print_full)) {
	documentPrint(stdout, doc);
    }
    else {
	documentPrintXML(stdout, doc);
    }
    
    objectFree((Object *) doc, TRUE);

    return NULL;
}

static Object *
executeTemplate(Object *params)
{
    Document *template = (Document *) dereference(symbolGetValue("template"));
    Int4 *sources = (Int4 *) dereference(symbolGetValue("sources"));
    int action_stack_entries = consLen(action_stack);
    String *action_name;
    Document *result;

    // TODO: Ensure sources is retrieved successfully from the hash
    if (action_stack_entries < sources->value) {
	action_name = (String *) dereference(symbolGetValue("action"));

	// TODO: Ensure action_name is retrieved successfully from the hash
	RAISE(PARAMETER_ERROR, 
	      newstr("Insufficient inputs for %s", action_name->value));
    }

    preprocessSourceDocs(sources->value, params);
    
    if (result = processTemplate(template)) {
	actionStackPush((Object *) result);
    }

    return NULL;
}

static Object *
executeDoNothing(Object *params)
{
    return NULL;
}

static Object *
executeConnect(Object *params)
{
    String *arg = (String *) symbolGetValue("arg");
    Symbol *connect = symbolNew("connect");
    symSet(connect, (Object *) stringNew(arg->value));
    return NULL;
}


static void
defineActionExecutors()
{
    static boolean done = FALSE;
    if (!done) {
	defineActionSymbol("execute_extract", &executeTemplate);
	defineActionSymbol("execute_adddeps", &executeTemplate);
	defineActionSymbol("execute_template", &executeTemplate);
	defineActionSymbol("execute_print", &executePrint);
	defineActionSymbol("execute_dbtype", &executeDoNothing);
	defineActionSymbol("execute_connect", &executeConnect);
	defineActionSymbol("execute_list", &executeTemplate);
    }
    done = TRUE;
}

/* This assigns to a symbol a value from the param hash.
 */
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
    setScopeForSymbol(sym);

    if ((value->type == OBJ_STRING) ||
	(value->type == OBJ_INT4)) {
	/* Copy objects that may be used in globals.  Other object
	 * can (probably) safely be simply referenced.
	 */
	symSet(sym, (Object *) objectCopy(value));
	//symSet(sym, (Object *) objRefNew(value));
    }
    else {
	symSet(sym, (Object *) objRefNew(value));
    }

    //printSexp(stderr, "SYM: ", sym);
    //printSexp(stderr, "SYM VALUE: ", sym->svalue);
    //return NULL;
    //((Cons *) obj)->cdr = NULL;
    return value;
}

/* This assigns to symbols all values from the param hash.
 */
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
    boolean global = FALSE;

    defineActionExecutors();
    global = getGlobal(params);
    if (!global) {
	newSymbolScope();
    }
    setVarsFromParams(params);
    executor_name = stringNewByRef(newstr("execute_%s", action->value));

    BEGIN {
	if (action_executor = symbolGet(executor_name->value)) {
	    result = symbolExec(action_executor, (Object *) params);
	}
	else{
	    RAISE(NOT_IMPLEMENTED_ERROR,
	    	  newstr("%s not implemented\n", executor_name->value));
	}
    }
    EXCEPTION(ex);
    FINALLY {
	finishWithConnection();
	objectFree((Object *) executor_name, TRUE);
	if (!global) {
	    dropSymbolScope();
	}
	objectFree((Object *) params, TRUE);
    }
    END;
}

void
finalAction()
{
    if (action_stack) {
	symbolSet("sources", (Object *) int4New(1));
	(void) executePrint(NULL);
    }
    if (action_stack) {
	RAISE(PARAMETER_ERROR, 
	      newstr("Unprocessed documents still exist on the stack"));
    }
}

