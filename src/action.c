/**
 * @file   action.c
 * \code
 *     Copyright (c) 2009, 2010, 2011 Marc Munro
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
#include <string.h>
#include "skit_lib.h"
#include "skit_param.h"
#include "exceptions.h"

static String action_str = {OBJ_STRING, "action"};
static String sources_str = {OBJ_STRING, "sources"};
static String value_str = {OBJ_STRING, "value"};
static String type_str = {OBJ_STRING, "type"};
static String required_str = {OBJ_STRING, "required"};
static String default_str = {OBJ_STRING, "default"};
static String add_deps_filename = {OBJ_STRING, "add_deps.xsl"};
static String rm_deps_filename = {OBJ_STRING, "rm_deps.xsl"};
static String global_str = {OBJ_STRING, "global"};
static String arg_str = {OBJ_STRING, "arg"};
static Document *adddeps_document = NULL;
static Document *rmdeps_document = NULL;
static Document *fallback_processor = NULL;

static Cons *docstack = NULL;

void
docStackPush(Document *doc)
{
    if (doc->type == OBJ_DOCUMENT) {
	readDocDbver(doc);
    }
    docstack = consNew((Object *) doc, (Object *) docstack);
}

Document *
docStackPop()
{
    Cons *front = docstack;
    Object *result = NULL;
    if (docstack) {
	docstack = (Cons *) docstack->cdr;
	front->cdr = NULL;
	result = front->car;
	objectFree((Object *) front, FALSE);
    }
    if (result && (result->type == OBJ_DOCUMENT)) {
	readDocDbver((Document *) result);
    }

    return (Document *) result;
}

static void
docStackFree()
{
    Object *obj;
    while (obj = (Object *) docStackPop()) {
	objectFree(obj, TRUE);
    }
}

static Object *
docStackHead()
{
    if (docstack) {
	return (Object *) docstack->car;
    }
    return NULL;
}

// TOO: deprecate
/* Count starts at 1 */
static Cons *
docStackNth(int n)
{
    Cons *nth = docstack;
    while ((n > 1) && nth) {
	nth = (Cons *) nth->cdr;
	n--;
    }
    return nth;
}

Document *
getFallbackProcessor()
{
    if (!fallback_processor) {
	fallback_processor = 
	    findDoc((String*) symbolGetValue("fallback_processor"));
    }
    return fallback_processor;
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

void
applyXSL(Document *xslsheet)
{
    Document *src_doc;
    Document *result_doc;

    src_doc = (Document *) docStackPop();
    result_doc = applyXSLStylesheet(src_doc, xslsheet);
    objectFree((Object *) src_doc, TRUE);
    docStackPush(result_doc);
}

void
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
	adddeps_document = NULL;
    }

    if (rmdeps_document) {
	objectFree((Object *) rmdeps_document, TRUE);
	rmdeps_document = NULL;
    }

    if (fallback_processor) {
	objectFree((Object *) fallback_processor, TRUE);
	fallback_processor = NULL;
    }
}

/* Load an input file into memory and place it on the stack for
 * subsequent processing.
 */
void
loadInFile(String *filename)
{
    Document *volatile doc = docFromFile(filename);
    if (!doc) {
	RAISE(PARAMETER_ERROR, 
	      newstr("Failed to load xml document %s", filename->value));
    }
    BEGIN {
	docGatherContents(doc, filename);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) doc, TRUE);
	RAISE();
    }
    END;
    docStackPush(doc);
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
addUnprovidedValue(Cons *entry, Object *params)
{
    Object *key = entry->car;
    Cons *alist = (Cons *) entry->cdr;
    Object *value = alistGet(alist, (Object *) &value_str);

    if (!value) {
	value = alistGet(alist, (Object *) &default_str);
    }

    if (value) {
	if (!hashGet((Hash *) params, key)) {
	    (void) hashAdd((Hash *) params, objectCopy(key), objectCopy(value));
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

static Object *
checkUnprovidedValue(Cons *entry, Object *params)
{
    Object *key = entry->car;
    Cons *alist = (Cons *) entry->cdr;
    Object *required = alistGet(alist, (Object *) &required_str);
    Object *value;
    if (required) {
	/* This arg is required, so check that it is provided in params */
	value = hashGet((Hash *) params, (Object *) key);
	if (!value) {
	    RAISE(PARAMETER_ERROR, 
		  newstr("getOptionlistArgs: required arg "
			 "\"%s\" not provided", ((String *) key)->value));
	}
    }
    return (Object *) alist;  /* Return the notional contents of the hash
			       * entry */
}


static void
checkRequired(Hash *params, Cons *optionlist)
{
    Hash *hash_from_list = (Hash *) consNth(optionlist, 2);
    
    hashEach(hash_from_list, checkUnprovidedValue, (Object *) params);
}

/* Read command line args based upon the optionlist parameter, returning
 * a hash of the args.
 */
static Hash *
getOptionlistArgs(Cons *optionlist)
{
    String *volatile arg = NULL;
    Object *volatile value = NULL;
    Hash *volatile params = hashNew(TRUE);
    boolean is_option;
    Object *old;
    int expected_sources = getIntOption(optionlist, &sources_str, &value_str);
    int expected_args = 0;
    String *fullname;
    
    BEGIN {
	if (expected_sources == 0) {
	    if (hasArg(optionlist)) {
		expected_args = 1;
	    }
	}
	while (nextArg((String **) &arg, &is_option)) {
	    if (is_option) {
		/* If this is an option, check its validity and deal with
		 * it.  If it is not valid, assume that it is not intended
		 * for this action and replace it into the arglist */
		
		if (value = getOptionValue(optionlist, arg)) {
		    fullname = optionlistGetOptionName(optionlist, arg);
		    fullname = stringNew(fullname->value);
		    old = hashAdd(params, (Object *) fullname, value);
		    objectFree(old, TRUE);  // maybe raise an error?
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
			old = hashAdd(params, (Object *) stringNew("arg"), 
				      (Object *) arg);
			objectFree(old, TRUE);
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
    checkRequired(params, optionlist);
    return params;
}

static Object *
doParseTemplate(String *filename)
{
    String *volatile path;
    Hash *volatile params = NULL;
    Document *volatile template_doc = NULL;
    Object *old;

    BEGIN {
	path = findFile(filename);
	if (!path) {
	    RAISE(PARAMETER_ERROR, newstr("doParseTemplate: template %s "
					  "not found", filename->value));
	}
	
	/* Load the template file, to get the option list. */
	template_doc = docFromFile(path);
	params = getOptionlistArgs(template_doc->options);
	old = hashAdd(params, (Object *) stringNew("template_name"), 
		      (Object *) path);
	objectFree(old, TRUE);  // Maybe we should raise an error?
	path = NULL;
	old = hashAdd(params, (Object *) stringNew("template"), 
		      (Object *) template_doc);
	objectFree(old, TRUE);  // Maybe we should raise an error?
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
    String *volatile filename;
    Object *action_info;
    UNUSED(obj);

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
execParseTemplate(char *name)
{
    Object *action_info;
    String *volatile filename;

    BEGIN {
	filename = stringNew(name);

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
    UNUSED(obj);
    return execParseTemplate("extract.xml");
}

static Object *
parseScatter(Object *obj)
{
    UNUSED(obj);
    return execParseTemplate("scatter.xml");
}

static Object *
parseDiff(Object *obj)
{
    UNUSED(obj);
    return execParseTemplate("diff.xml");
}

static Object *
parseGenerate(Object *obj)
{
    UNUSED(obj);
    return execParseTemplate("generate.xml");
}

static Object *
parseConnect(Object *obj)
{
    UNUSED(obj);
    return execParseTemplate("connect.xml");
}

static Object *
parseAdddeps(Object *obj)
{
    UNUSED(obj);
    return execParseTemplate("add_deps.xml");
}

static Object *
parsePrint(Object *obj)
{
    Hash *args = getOptionlistArgs(printOptionList());
    UNUSED(obj);
    return (Object *) args;
}

static Object *
parseList(Object *obj)
{
    String *volatile filename = stringNew("list.xml");
    Object *params;
    UNUSED(obj);
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
    (void) hashAdd(hash, (Object *) stringNew("global"), (Object *) t);
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

    UNUSED(obj);
    if (!dbtype) {
	RAISE(PARAMETER_ERROR,
	      newstr("dbtype requires an argument"));    
    }
    if (checkDbtypeIsRegistered(dbtype)) {
	result = hashNew(TRUE);
	key = stringNew("dbtype");
	(void) hashAdd(result, (Object *) key, (Object *) dbtype);
	makeGlobal(result);
	return (Object *) result;
    }
    else {
	str = newstr("dbtype \"%s\" is not known to skit\n", dbtype->value);
	objectFree((Object *) dbtype, TRUE);
	RAISE(PARAMETER_ERROR, str);
	return NULL;
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
	defineActionSymbol("parse_generate", &parseGenerate);
	defineActionSymbol("parse_template", &parseTemplate);
	defineActionSymbol("parse_connect", &parseConnect);
	defineActionSymbol("parse_print", &parsePrint);
	defineActionSymbol("parse_adddeps", &parseAdddeps);
	defineActionSymbol("parse_dbtype", &parseDbtype);
	defineActionSymbol("parse_list", &parseList);
	defineActionSymbol("parse_scatter", &parseScatter);
	defineActionSymbol("parse_diff", &parseDiff);
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
    String *volatile parser_name;
    Symbol *action_parser;
    Hash *params;
    Object *old;

    defineActionParsers();

    BEGIN {
	parser_name = stringNewByRef(newstr("parse_%s", action->value));

	if (action_parser = symbolGet(parser_name->value)) {
	    params = (Hash *) symbolExec(action_parser, NULL);
	    old = hashAdd(params, (Object *) stringDup(&action_str), 
			  (Object *) stringDup(action));
	    objectFree(old, TRUE);  // Maybe we should raise an error?
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
	cons = docStackNth(i);
	src_doc = (Document *) cons->car;
	if (add_deps && (!docHasDeps(src_doc))) {
	    xslsheet = getAddDepsDoc();
	    result_doc = applyXSLStylesheet(src_doc, xslsheet);
	    objectFree((Object *) src_doc, TRUE);
	}
	else {
	    result_doc = src_doc;
	}
	rmParamsNode(result_doc);
	addParamsNode(result_doc, params);
	cons->car = (Object *) result_doc;
    }
}

static Object *
executePrint(Object *params)
{
    Int4 *sources = (Int4 *) dereference(symbolGetValue("sources"));
    int docstack_entries = consLen(docstack);
    boolean print_full;
    boolean print_xml;
    boolean has_deps;
    Document *doc;
    UNUSED(params);

    if (!sources) {
	RAISE(GENERAL_ERROR, 
	      newstr("No sources definition found for action \"print\""));
    }
    if (docstack_entries < sources->value) {
	RAISE(PARAMETER_ERROR, 
	      newstr("Insufficient inputs for action \"print\""));
    }

    print_full = dereference(symbolGetValue("full")) && TRUE;
    print_xml = dereference(symbolGetValue("xml")) && TRUE;

    doc = (Document *) docStackHead();
    has_deps = docHasDeps(doc);

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
    doc = (Document *) docStackPop();

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
    int docstack_entries = consLen(docstack);
    String *action_name = (String *) dereference(symbolGetValue("action"));
    Document *result;
    boolean retain_deps;
    xmlNode *root;

    retain_deps = dereference(symbolGetValue("retain_deps")) && TRUE;

    if (!sources) {
	RAISE(GENERAL_ERROR, 
	      newstr("No sources definition found for action \"%s\"",
		     action_name? action_name->value: "template"));
    }

    if (docstack_entries < sources->value) {
	RAISE(PARAMETER_ERROR, 
	      newstr("Insufficient inputs for %s", 
		     action_name? action_name->value: "template"));
    }

    preprocessSourceDocs(sources->value, params);
    
    if (result = processTemplate(template)) {
	rmParamsNode(result);
	if (retain_deps) {
	    root = xmlDocGetRootElement(result->doc);
	    (void) xmlNewProp(root, (const xmlChar *) "retain_deps", 
			      (xmlChar *) "true");
	}
	docStackPush(result);
    }

    return NULL;
}

static Object *
executeDoNothing(Object *params)
{
    UNUSED(params);
    return NULL;
}

static Object *
executeConnect(Object *params)
{
    String *arg = (String *) symbolGetValue("arg");
    Symbol *connect = symbolNew("connect");
    UNUSED(params);

    if (arg) {
	symSet(connect, (Object *) stringNew(arg->value));
    }
    else {
	symSet(connect, (Object *) stringNew(""));
    }

    (void) sqlConnect();
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
	defineActionSymbol("execute_generate", &executeTemplate);
	defineActionSymbol("execute_scatter", &executeTemplate);
	defineActionSymbol("execute_diff", &executeTemplate);
    }
    done = TRUE;
}

/* This assigns to a symbol a value from the param hash.
 */
static Object *
setVarFromParam(Cons *entry, Object *params)
{
    String *key = (String *) entry->car;
    Object *value = entry->cdr;
    Symbol *sym;

    UNUSED(params);
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
    String *volatile executor_name;
    Symbol *action_executor;
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
	    (void) symbolExec(action_executor, (Object *) params);
	}
	else{
	    RAISE(NOT_IMPLEMENTED_ERROR,
	    	  newstr("%s not implemented\n", executor_name->value));
	}
	//checkSymbols();
    }
    EXCEPTION(ex) {
	docStackFree();
    }
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
    if (docstack) {
	symbolSet("sources", (Object *) int4New(1));
	(void) executePrint(NULL);
    }
    if (docstack) {
	docStackFree();
	RAISE(PARAMETER_ERROR, 
	      newstr("Unprocessed documents still exist on the stack"));
    }
}

