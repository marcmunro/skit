/**
 * @file   xmlfile.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Read and process xmlfiles
 *
 */

#include <stdio.h>
#include <string.h>
#include <glob.h>
#include <libgen.h>
#include "skit_lib.h"
#include "exceptions.h"
#include <libxml/xmlreader.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>


static String empty_str = {OBJ_STRING, ""};
static String boolean_str = {OBJ_STRING, "boolean"};
static Document *cur_template;


/* Tuplestack contains a stack (list) of tuples.  The head of the stack
 * is the current tuple, also available from the tuple symbol.  Each
 * successive element in the stack is a tuple from a higher level of
 * nesting. */
static void
tuplestackPush(Object *tuple)
{
    Symbol *tuplestack = symbolGet("tuplestack");
    Symbol *tuple_sym = symbolGet("tuple");
    tuple_sym->svalue = tuple;  /* We do this directly so that we don't
				 * try to free the previous contents. */

    //fprintf(stderr, "TUPLEPUSH: tuple = %p\n", tuple);
    (void) consPush((Cons **) &(tuplestack->svalue),  tuple);
}

static void
tuplestackPop()
{
    Symbol *tuplestack = symbolGet("tuplestack");
    Symbol *tuple_sym = symbolGet("tuple");
    Cons *stack;
    Object *tuple = NULL;
    
    (void) consPop((Cons **) &(tuplestack->svalue));
    if (stack = (Cons *) tuplestack->svalue) {
	tuple = stack->car;
    }
    tuple_sym->svalue = tuple;  /* We do this directly so that we don't
				 * try to free the previous contents. */
    //fprintf(stderr, "TUPLEPOP: tuple = %p\n", tuple);
}

static Object *
curTuple()
{
    Object *result = symbolGetValue("tuple");
    return result;
}

static char *
nodeName(xmlNode *node)
{
    if (node) return (char *) node->name;
    return "nil";
}

static void
addAttribute(xmlNodePtr node, 
	     String *name,
	     String *value)
{
    xmlAttrPtr attr;

    if (name && value) {
	attr = xmlNewProp(node, name->value, value->value);
    }
}

static void
addText(xmlNodePtr node, String *value)
{
    xmlNodePtr text;

    text = xmlNewText((xmlChar *) value->value);
    xmlAddChild(node, text);
}

Document *
applyXSLStylesheet(Document *src, Document *stylesheet)
{
    xmlDocPtr result = NULL;
    Document *doc;
    xsltTransformContextPtr ctxt;
    const char *params[1] = {NULL};

    //dbgSexp(stylesheet);
    //dbgSexp(src);
    //fprintf(stderr, "===============DONE==============\n");
    if ((!stylesheet->stylesheet) && stylesheet->doc) {
	stylesheet->stylesheet = xsltParseStylesheetDoc(stylesheet->doc);
	stylesheet->doc = NULL;
    }

    if (!stylesheet->stylesheet) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Unable to find or build stylesheet"));
    }


    ctxt = xsltNewTransformContext(stylesheet->stylesheet, src->doc);
    registerXSLTFunctions(ctxt);

    if (result = xsltApplyStylesheetUser(stylesheet->stylesheet, 
					 src->doc, params, NULL, 
					 NULL, ctxt)) {
	xsltFreeTransformContext(ctxt);
	doc = documentNew(result, NULL);
	//dbgSexp(doc);
	return doc;
    }
    return NULL;
}

static Hash *skit_processors = NULL;

typedef xmlNode *(xmlFn)(xmlNode *template_node, 
			 xmlNode *parent_node, int depth);

static xmlNode *processChildren(xmlNode *template_node, 
				xmlNode *parent_node, int depth);

static xmlNode *processRemaining(xmlNode *template_node, 
				 xmlNode *parent_node, int depth);

static xmlNode *
firstElement(xmlNode *start)
{
    xmlNode *result;
    for (result = start; result && result->type != XML_ELEMENT_NODE; 
	 result = result->next) {
    }
    return result;
}

static Document *
docForNode(xmlNode *node) {
    xmlNode *root_node;
    xmlDocPtr doc;
    
    if (node) {
	if (node->doc) {
	    return node->doc->_private;
	}
	/* This node is not currently part of a document.  Let's create a
	 * document for it. */
	root_node = node;
	while (root_node->parent) {
	    root_node = root_node->parent;
	}
	doc = xmlNewDoc((const xmlChar *) "1.0");
	xmlDocSetRootElement(doc, root_node);

	return documentNew(doc, NULL);
    }
    return NULL;
}


/* Handle a skit:stylesheet element */
static xmlNode *
stylesheetFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    xmlNode *node = NULL;
    String *result_attr = nodeAttribute(template_node, "result");
    Object *result_reqd;
    Document *doc = NULL;

    if (result_attr) {
	result_reqd  = validateParamValue(&boolean_str, result_attr);
    }

    BEGIN {
	node = processChildren(template_node, parent_node, 1);

	if (result_attr) {
	    if (!result_reqd) {
		/* This is a dumb way of freeing a node, but it ensures
		 * that everything associated with the node is actually
		 * freed. */ 
		if (doc = docForNode(node)) {
		    objectFree((Object *) doc, TRUE);
		}
		else {
		    xmlUnlinkNode(node);
		    xmlFree(node);
		}
		node = NULL;
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) result_attr, TRUE);
    }
    END;
    return node;
}

/* Handle a skit:xxxx element that is to be ignored 
 * skit:options is an example of such an element 
 */
static xmlNode *
ignoreFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    return NULL;
}

static boolean
hasExprAttribute(xmlNode *node, Object **p_result, char *attribute_name)
{
    String *expr;
    Object *value = NULL;
    if (expr = nodeAttribute(node, attribute_name)) {
	BEGIN {
	    value = evalSexp(expr->value);
	}
	EXCEPTION(ex) /* NOTE: Adding the following clause causes memory
	    errors.  Evidently there is a problem with the exception
	    handling macros in combination of WHEN clauses and FINALLY
	    TODO: Investigate this further.
	    */
	{
	    WHEN(LIST_ERROR) {
		char *newtext = newstr("%s in expr:\n%s", 
				       ex->text, expr->value);
		objectFree((Object *) expr, TRUE);
		RAISE(LIST_ERROR, newtext);
	    } 
	}
	FINALLY {
	    objectFree((Object *) expr, TRUE);
	}
	END;
	*p_result = value;
	return TRUE;
    }
    *p_result = NULL;
    return FALSE;
}

static Object *
getExprAttribute(xmlNode *node, char *attribute_name)
{
    Object *result = NULL;
    boolean ignore = hasExprAttribute(node, &result, attribute_name);
    return result;
}

static boolean
hasExpr(xmlNode *node, Object **p_result)
{
    return hasExprAttribute(node, p_result, "expr");
}

static Object *
getExpr(xmlNode *node)
{
    Object *value;
    (void) hasExpr(node, &value);
    return value;
}

static String *
fieldValueForTemplate(xmlNode *template_node)
{
    String *field = NULL;
    Object *value = NULL;
    Object *actual;
    String *strvalue = NULL;

    if (hasExpr(template_node, &value)) {
	if (value) {
	    /* If value is a String then we return it.  If it is not
	     * then we use objectSexp.  If it is a string reference we
	     * must make a copy. */

	    actual = dereference(value);
	    if (actual->type != OBJ_STRING) {
		strvalue = stringNewByRef(objectSexp(actual));
		objectFree(value, TRUE);
	    }
	    else if (value->type == OBJ_OBJ_REFERENCE) {
		strvalue = (String *) objectCopy(dereference(value));
		objectFree(value, TRUE);
	    }
	    else {
		strvalue = (String *) value;
	    }
	}
    }
    else {
	/* Get string from the current tuple's field as named in 
	   the name or field attribute. */
	field = nodeAttribute(template_node, "field");
	
	if (!field) {
	    field = nodeAttribute(template_node, "name");
	}
	
	BEGIN {
	    strvalue = (String *) objSelect(curTuple(), (Object *) field);
	}
	EXCEPTION(ex);
	WHEN(NOT_IMPLEMENTED_ERROR) {
	    char *exstr;
	    char *tmp = objectSexp((Object *) field);
	    objectFree((Object *) field, TRUE);
	    exstr = newstr("Unable to select %s from current foreach record",
			   tmp);
	    skfree(tmp);

	    RAISE(XML_PROCESSING_ERROR, exstr);
	}
	END;
    }

    objectFree((Object *) field, TRUE);
    return strvalue;
}

/* Handle a skit:attr or akit:attribute element.  The context node,
 * containing the node being processed by our parent, will be modified
 * to contain a new attribute with its value set as defined by this
 * element.
 * An attr element:
 * - must contain a name
 * - may contain an expr entry
 */
static xmlNode *
attributeFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *name = nodeAttribute(template_node, "name");
    String *str = NULL;

    if (!name) {
	RAISE(XML_PROCESSING_ERROR,
	      newstr("No name field provided for skit_attr"));
    }
    BEGIN {
	str = fieldValueForTemplate(template_node);
	addAttribute(parent_node, name, str);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) name, TRUE);
	objectFree((Object *) str, TRUE);
    }
    END;
    return NULL;
}

static xmlNode *
textFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *str = NULL;

    if (str = fieldValueForTemplate(template_node)) {
	addText(parent_node, str);
	objectFree((Object *) str, TRUE);
    }
    return NULL;
}

static xmlNode *
execFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    Object *result = getExpr(template_node);
    objectFree((Object *) result, TRUE);
    return NULL;
}

static Object *debug_obj = NULL;

static void
xmlfiledebug(Object *obj)
{
    debug_obj = obj;
    fprintf(stderr, "XMLFILEDEBUG\n");
}

static Cons *mapvars = NULL;

static void
newMapSymbol(String *name)
{
    Symbol *mapsym = NULL;
    mapsym = symbolNew(name->value);
    setScopeForSymbol(mapsym);
    mapvars = consNew((Object *) mapsym, (Object *) mapvars);
}

static void
popMapSymbol()
{
    Cons *prev = (Cons *) mapvars->cdr;
    objectFree((Object *) mapvars, FALSE);
    mapvars = prev;
}

static void
appendToMapVar(Object *obj)
{
    Symbol *sym;
    if (!mapvars) {
	RAISE(XML_PROCESSING_ERROR,
	      newstr("No map variable in play for skit:result"));
    }
    sym = (Symbol *) mapvars->car;
    if (sym->svalue) {
	consAppend((Cons *) sym->svalue, obj);
    }
    else {
	sym->svalue = (Object *) consNew(obj, NULL);
    }
}

static xmlNode *
execResult(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    Object *obj;
    String *expr = nodeAttribute(template_node, "expr");
    if (!expr) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("expr must be specified for skit:result"));
    }
    BEGIN {
	obj = evalSexp(expr->value);
	appendToMapVar(obj);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) expr, TRUE);
    }
    END;
    return NULL;
}

static xmlNode *
iterate(Object *collection, String *filter,
	xmlNode *template_node, xmlNode *parent_node, int depth)
{
    Object *tuple;
    xmlNode *first_child = NULL;
    xmlNode *child;
    Object *result;
    Object *placeholder = NULL;
    boolean do_it = TRUE;
    String *varname = nodeAttribute(template_node, "var");
    String *key = nodeAttribute(template_node, "key");
    String *idxname = nodeAttribute(template_node, "index");
    String *mapname = nodeAttribute(template_node, "map_to");
    Symbol *varsym = NULL;
    Symbol *idxsym = NULL;
    Int4 *idx = NULL;

    if (mapname) {
	newMapSymbol(mapname);
    }

    if (varname || idxname) {
	newSymbolScope();
    }   
    if (varname) {
	varsym = symbolNew(varname->value);
	setScopeForSymbol(varsym);
    }
    if (idxname) {
	idxsym = symbolNew(idxname->value);
	setScopeForSymbol(idxsym);
	idx = int4New(0);
	idxsym->svalue = (Object *) idx;
    }

    BEGIN {
	while (tuple = objNext(collection, &placeholder), placeholder) {
	    if (varsym) {
		varsym->svalue = tuple;
	    }
	    if (idx) {
		idx->value++;
	    }
	    tuplestackPush(tuple);
	    
	    if (filter) {
		result = evalSexp(filter->value);
		do_it = (result != NULL);
		objectFree(result, TRUE);
	    }
	    
	    BEGIN {
		//char *tmp = objectSexp(tuple);
		//if (streq(tmp, "'regress=C/regress'")) {
		//    xmlfiledebug(tuple);
		//}
		//skfree(tmp);
		//fprintf(stderr, "TUPLE: %p\n", tuple);
		//printSexp(stderr, "TUPLE: ", tuple);
		if (do_it) {
		    child = processChildren(template_node, parent_node, 
					    depth + 1);
		    if (!first_child) {
			first_child = child;
		    }
		    //fprintf(stderr, "TUPLE2: %p\n", tuple);
		    //printSexp(stderr, "TUPLE2: ", tuple);
		}
	    }
	    EXCEPTION(ex);
	    //fprintf(stderr, "EXCEPTION ITERATE(1)\n");
	    FINALLY {
		objectFree(tuple, TRUE);
		tuplestackPop();
	    }
	    END;
	    if (!parent_node) {
		/* No parent implies child is a root node.  There
		 * may only  be one root, so exit now.  TODO:
		 * Consider whether to raise an exception if another
		 * child is found here. */ 
		break;
	    }
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION ITERATE(2)\n");
    FINALLY {
	// TODO: This direct handling of symbol svalues below is dumb.
	// See if this can be handled better by dropSymbolScope 
	if (varsym) {
	    varsym->svalue = NULL;
	}
	if (idxsym) {
	    idxsym->svalue = NULL;
	}
	if (varname || idxname) {
	    dropSymbolScope();
	}
	if (mapname) {
	    popMapSymbol();
	    objectFree((Object *) mapname, TRUE);
	}
	objectFree((Object *) key, TRUE);
	objectFree((Object *) idxname, TRUE);
	objectFree((Object *) varname, TRUE);
	objectFree((Object *) idx, TRUE);
	objectFree(placeholder, TRUE);
    }
    END;

    return first_child;
}

static xmlNode *
execRunsql(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *filename = nodeAttribute(template_node, "file");
    String *varname = nodeAttribute(template_node, "to");
    String *hashkey = nodeAttribute(template_node, "hash");
    String *filetext = NULL;
    Cursor *cursor = NULL;
    String *sqltext = NULL;
    Connection *conn;
    Tuple *tuple;
    xmlNode *child = NULL;
    Symbol *sym;
    Object *params = NULL;
    BEGIN {
	if (!filename) {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("File must be specified for runsql"));
	}
	filetext = readFile(filename);

	if (!filetext) {
	    RAISE(FILEPATH_ERROR,
		  newstr("Unable to find sql file: %s\n", filename->value));
	}
	sqltext = trimSqlText(filetext);
    
	conn = sqlConnect();
	params = getExprAttribute(template_node, "params");
	cursor = sqlExec(conn, sqltext, params);
	
	//printSexp(stderr, "CURSOR: ", cursor);
	if (varname) {
	    sym = symbolNew(varname->value);
	    setScopeForSymbol(sym);
	    if (hashkey) {
		cursorIndex(cursor, hashkey);
	    } 
	    symbolSet(varname->value, (Object *) cursor);
	}
	else {
	    child = iterate((Object *) cursor, NULL, 
			    template_node, parent_node, depth);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) cursor, TRUE);
	cursor = NULL;
    }
    FINALLY {
	if (!varname) {
	    /* If a variable was defined, the cursor will be freed when
	     * that variable goes out of scope, otherwise free it now. */
	    objectFree((Object *) cursor, TRUE);
	}
	objectFree((Object *) filename, TRUE);
	objectFree((Object *) varname, TRUE);
	objectFree((Object *) hashkey, TRUE);
	objectFree((Object *) sqltext, TRUE);
	objectFree((Object *) filetext, TRUE);
	objectFree((Object *) params, TRUE);
    }
    END;
    
    return child;
}

static xmlNode *
execForeach(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *fromname = nodeAttribute(template_node, "from");
    String *expr;
    String *filter = nodeAttribute(template_node, "filter");
    Object *collection;
    xmlNode *child = NULL;
    boolean doit = TRUE;
    Tuple *tuple;
    Object *result;

    BEGIN {
	if (fromname) {
	    collection = symbolGetValue(fromname->value);
	}
	else {
	    expr = nodeAttribute(template_node, "expr");
	    if (!expr) {
		RAISE(XML_PROCESSING_ERROR, 
		      newstr("from or expr must be specified for foreach"));
	    }
	    collection = evalSexp(expr->value);
	}
	
	if (collection) {
	    if (!isCollection(collection)) {
		    RAISE(XML_PROCESSING_ERROR, 
			  newstr("from variable %s does not contain a "
				 "collection",  fromname->value));
	    }
	    child = iterate(collection, filter, template_node, 
			    parent_node, depth);
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION FOREACH\n");
    FINALLY {
	objectFree((Object *) fromname, TRUE);
	objectFree((Object *) filter, TRUE);
    }
    END;
    
    return child;
}


static xmlNode *
execKids(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    return processChildren(template_node, parent_node, depth + 1);
}

static xmlNode *
execIf(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *expr = nodeAttribute(template_node, "test");
    Object *expr_result;
    xmlNode *result = NULL;
    BEGIN {
	if (expr) {
	    if (expr_result = evalSexp(expr->value)) {
		result = processChildren(template_node, parent_node, depth + 1);
		objectFree((Object *) expr_result, TRUE);
	    }
	}
	else {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("no \"test\" attribute provided for skit:if"));
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION EXECIF\n");
    FINALLY {
	objectFree((Object *) expr, TRUE);
    }
    END;
    return result;
}

static xmlNode *
execLet(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    xmlNode *result;
    newSymbolScope();
    BEGIN {
	result = processChildren(template_node, parent_node, depth + 1);
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION LET\n");
    FINALLY {
	dropSymbolScope();
    }
    END;
    return result;
}

static xmlNode *
execVar(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *name = nodeAttribute(template_node, "name");
    Object *value;
    Symbol *sym;

    if (!name) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Name must be specified for var"));
    }

    sym = symbolGet(name->value);
    
    if (sym) {
	objectFree(sym->svalue, TRUE);
    }
    else {
	sym = symbolNew(name->value);
    }
    setScopeForSymbol(sym);
    objectFree((Object *) name, TRUE);
    sym->svalue = getExpr(template_node);
    return NULL;
}

static xmlNode *
execException(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *text = nodeAttribute(template_node, "text");
    char *contents;

    if (!text) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("text must be specified for exception"));
    }
    contents = text->value;
    objectFree((Object *) text, FALSE);

    RAISE(XML_PROCESSING_ERROR, contents);

    return NULL;
}

static xmlNode *
execDeclareFunction(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *name = nodeAttribute(template_node, "name");
    Node *node = nodeNew(template_node);
    Symbol *name_sym;

    if (!name) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("name attribute must be provided for function"));
    }
    
    name_sym = symbolNew(name->value);
    objectFree(name_sym->svalue, TRUE);
    name_sym->svalue = (Object *) node;
    objectFree((Object *) name, TRUE);
    return NULL;
}

static void
getParam(xmlNode *template_node, xmlNode *cur_node)
{
    String *name = nodeAttribute(cur_node, "name");
    String *dflt = NULL;
    String *expr = NULL;
    Object *value = NULL;
    Symbol *sym;

    if (!name) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("name attribute must be provided for parameter"));
    }

    BEGIN {
	if (expr = nodeAttribute(template_node, name->value)) {
	    value = evalSexp(expr->value);
	}
	else {
	    if (dflt = nodeAttribute(cur_node, "default")) {
		value = evalSexp(dflt->value);
	    }
	    else {
		RAISE(XML_PROCESSING_ERROR, 
		      newstr("mandatory parameter %s not provided", 
			     name->value));
	    }
	}
	sym = symbolGet(name->value);
    
	if (!sym) {
	    sym = symbolNew(name->value);
	}
	setScopeForSymbol(sym);
	sym->svalue = value;
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION GETPARAM\n");
    FINALLY {
	objectFree((Object *) name, TRUE);
	objectFree((Object *) dflt, TRUE);
	objectFree((Object *) expr, TRUE);
    }
    END;
}

static xmlNode *
handleFunctionParams(xmlNode *template_node, xmlNode *function_node)
{
    xmlNode *current = function_node->children;
    xmlNs *ns;
    char *nodename;

    /* Skip over any leading text, comment, etc nodes */

    while (current) {
	if (current->type != XML_ELEMENT_NODE) {
	    current = current->next;
	}
	else if ((ns = current->ns) && 
		 streq(ns->prefix, "skit") &&
		 streq((char *) current->name, "parameter")) {
	    getParam(template_node, current);
	    current = current->next;
	}
	else {
	    break;
	}
    }
    return current;
}

static xmlNode *
execExecuteFunction(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *name = nodeAttribute(template_node, "name");
    Node *node;
    Symbol *name_sym;
    xmlNode *function_start;
    xmlNode *result;

    if (!name) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("name attribute must be provided for function call"));
    }
    
    name_sym = symbolGet(name->value);
    if (!name_sym) {
	char *errstr = newstr("No such function: %s", name->value);
	objectFree((Object *) name, TRUE);
	RAISE(XML_PROCESSING_ERROR, errstr);
    }
    node = (Node *) name_sym->svalue;
    objectFree((Object *) name, TRUE);

    newSymbolScope();
    BEGIN {
	function_start = handleFunctionParams(template_node, node->node);
	result = processRemaining(function_start, parent_node, depth + 1);
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION EXEC FUNC\n");
    FINALLY {
	dropSymbolScope();
    }
    END;
    return result;
}

static xmlNode *
execXSLproc(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *stylesheet_name = nodeAttribute(template_node, "stylesheet");
    String *input = nodeAttribute(template_node, "input");
    Document *stylesheet = NULL;
    Document *source_doc = NULL;
    Document *result_doc = NULL;
    xmlNode *scratch;
    xmlNode *result;
    xmlNode *root_node;

    BEGIN {
	if (!stylesheet_name) {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("stylesheet attribute must be provided for xslproc"));
	}

	stylesheet = findDoc(stylesheet_name);

	if (input && (streq(input->value, "pop"))) {
	    source_doc = docStackPop();
	    if (!source_doc) {
		RAISE(PARAMETER_ERROR,
		      newstr("Failed to pop document for xsl processing"));
	    }
	}
	else {
	    root_node = processChildren(template_node, NULL, depth + 1);
	    source_doc = docForNode(root_node);
	}
	//dbgSexp(source_doc);
	//dbgSexp(stylesheet);
	result_doc = applyXSLStylesheet(source_doc, stylesheet);
	if (!result_doc) {
	       RAISE(XML_PROCESSING_ERROR,
	      newstr("Failed to process stylesheet: %s", 
	    	 stylesheet_name->value));
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) source_doc, TRUE);
	objectFree((Object *) stylesheet, TRUE);
	objectFree((Object *) stylesheet_name, TRUE);
	objectFree((Object *) input, TRUE);
    }
    END;

    scratch = xmlDocGetRootElement(result_doc->doc);
    result = xmlCopyNode(scratch, 1);  // Adding this seems to make it
				      // safe.  WTF?
    //xmlUnlinkNode(result);          // This should be enough but is not!
    objectFree((Object *) result_doc, TRUE);
    return result;
}

static 
addSavedTextNode(xmlNode *parent, xmlNode *text)
{
    if (text) {
	xmlAddChild(parent, xmlCopyNode(text, 2));
    }
}

static xmlNode *copyNodeContents(xmlNode *source);

/* Note that we do not copy text nodes that precede an element that we
 * are not going to copy. */ 
static
copyKidsContents(xmlNode *parent, xmlNode *kid)
{
    xmlNode *newkid;
    xmlNode *text_node = NULL;

    while (kid) {
	if (kid->type == XML_TEXT_NODE) {
	    text_node = kid;
	}
	else {
	    if (newkid = copyNodeContents(kid)) {
		addSavedTextNode(parent, text_node);
		xmlAddChild(parent, newkid);
	    }
	    text_node = NULL;
	}
	kid = kid->next;
    }
    addSavedTextNode(parent, text_node);
}

/* copy any kids, recursively, that are not dbobject or dependency
 * nodes. 
 */
static xmlNode *
copyNodeContents(xmlNode *source)
{
    xmlNode *new;
    xmlNode *kids = NULL;

    if (source->type == XML_ELEMENT_NODE) {
	if (streq(source->name, "dbobject") ||
	    streq(source->name, "dependencies"))  {
	    return NULL;
	}
	else {
	    kids = source->children;
	}
    }
    new = xmlCopyNode(source, 2);
    copyKidsContents(new, kids);
    return new;
}

/* Copy the contents of the dbobject node */
static xmlNode *
copyObjectNode(xmlNode *source)
{
    xmlNode *new = xmlCopyNode(source, 2);

    copyKidsContents(new, source->children);
    return new;
}

static char *
actionName(DagNode *node)
{
    /* Simplest test is againt the 3rd character of the fqn */
    switch (node->fqn->value[2]) {
    case 'r': return "arrive";
    case 'i': return "build";
    case 'o': return "drop";
    case 'p': return "depart";
    case 'f': return "diff";
    }
    RAISE(GENERAL_ERROR,
	  newstr("actionName: cannot identify action from fqn \"%s\"", 
		 node->fqn->value));
}

static void
addAction(xmlNode *node, char *action)
{
    Vector *navigation = NULL;
    xmlNewProp(node, "action", action);
}

void
addNavigationNodes(xmlNode *parent_node, DagNode *cur, DagNode *target)
{
    Vector *navigation = NULL;
    DagNode *nnode;
    xmlNode *curnode;
    int i;

    BEGIN {
	navigation = navigationToNode(cur, target);
	for (i = 0; i < navigation->elems; i++) {
	    nnode = (DagNode *) navigation->contents->vector[i];
	    curnode = copyObjectNode(nnode->dbobject);
	    addAction(curnode, actionName(nnode));
	    xmlAddChild(parent_node, curnode);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) navigation, TRUE);
    }
    END;
}

void
treeFromVector(xmlNode *parent_node, Vector *sorted_nodes)
{
    DagNode *prev = NULL;
    DagNode *dnode;
    xmlNode *curnode;
    int i;

    for (i = 0; i < sorted_nodes->elems; i++) {
	dnode = (DagNode *) sorted_nodes->contents->vector[i];
	addNavigationNodes(parent_node, prev, dnode);
	curnode = copyObjectNode(dnode->dbobject);
	addAction(curnode, actionName(dnode));
	xmlAddChild(parent_node, curnode);
	prev = dnode;
    }
    addNavigationNodes(parent_node, prev, NULL);
}

static xmlNode *
execGensort(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *input = nodeAttribute(template_node, "input");
    Document *source_doc = NULL;
    Document *result_doc = NULL;
    Hash *dagnodes = NULL;
    Vector *sorted = NULL;
    xmlNode *root;
    xmlDocPtr xmldoc;

    BEGIN {
	if (input && (streq(input->value, "pop"))) {
	    source_doc = docStackPop();
	}
	sorted = gensort(source_doc);
	xmldoc = xmlNewDoc(BAD_CAST "1.0");
	root = parent_node? parent_node: xmlNewNode(NULL, BAD_CAST "root");
	xmlDocSetRootElement(xmldoc, root);
	result_doc = documentNew(xmldoc, NULL);

	treeFromVector(root, sorted);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) sorted, TRUE);
	objectFree((Object *) input, TRUE);
	objectFree((Object *) source_doc, TRUE);
    }
    END;
    return root;
}

static void
addProcessor(char *name, xmlFn *processor)
{
    String *sname = stringNew(name);
    FnReference *ref = fnRefNew(processor);
    if (hashAdd(skit_processors, (Object *) sname, (Object *) ref)) {
	RAISE(GENERAL_ERROR,
	      newstr("addProcessor: Attempt to add duplicate \"%s\"", name));
    }
}

static boolean trees_match(xmlNode *node1, xmlNode *node2);

list_length(xmlAttr *node)
{
    int len = 0;
    while(node) {
	node = node->next;
	len++;
    }
    return len;
}

static boolean
attribute_eq(xmlNode *node1, xmlNode *node2, const xmlChar *name)
{
    xmlChar *text1 = xmlGetProp(node1, name);
    xmlChar *text2 = xmlGetProp(node2, name);
    int eq;
    if (text1 && text2) {
	eq = streq(text1, text2);
    }
    else {
	eq = FALSE;
    }
    if (text1) {
	xmlFree(text1);
    }
    if (text2) {
	xmlFree(text2);
    }
    return eq;
}

static boolean
all_node_attributes_match(xmlNode *node1, xmlNode *node2)
{
    xmlAttr *attr1 = node1->properties;

    if (list_length(attr1) != list_length(node2->properties)) {
	return FALSE;
    }
    while (attr1) {
	if (!attribute_eq(node1, node2, attr1->name)) {
	    return FALSE;
	}
	attr1 = attr1->next;
    }
    return TRUE;   /* No differences found */
}

static boolean
nodes_match(xmlNode *node1, xmlNode *node2)
{
    xmlChar *text1;
    xmlChar *text2;
    int eq;

    /* Check element types are the same. */
    if (node1->type != node2->type) {
	return FALSE;
    }
    switch (node1->type) {
    case XML_ELEMENT_NODE:
	/* Check element names are the same. */
	if (!streq(node1->name, node2->name)) {
	    return FALSE;
	}
	if (!all_node_attributes_match(node1, node2)) {
	    return FALSE;
	}
	break;
    case XML_TEXT_NODE:
	/* Check element text is the same. */
	text1 = xmlNodeGetContent(node1);
	text2 = xmlNodeGetContent(node2);
	eq = streq(text1, text2);
	xmlFree(text1);
	xmlFree(text2);
	if (!eq) {
	    return FALSE;
	}
    }
    return trees_match(node1->children, node2->children);
}

static boolean
is_empty_text_node(xmlNode *node)
{
    xmlChar *text;
    char *c;
    if (node->type == XML_TEXT_NODE) {
	text = xmlNodeGetContent(node);
	for (c = text; *c != '\0'; c++) {
	    if (!isspace(*c)) {
		xmlFree(text);
		return FALSE;
	    }
	}
	xmlFree(text);
	return TRUE;
    }
    return FALSE;
}

static xmlNode *
skip_empty_nodes(xmlNode *node)
{
    while (node && is_empty_text_node(node)) {
	node = node->next;
    }
    return node;
}

static boolean
trees_match(xmlNode *node1, xmlNode *node2)
{
    boolean matched = TRUE;
    while ((node1 = skip_empty_nodes(node1)) && 
	   (node2 = skip_empty_nodes(node2)) && matched)
    {
	if (matched = nodes_match(node1, node2)) {
	    node1 = node1->next;
	    node2 = node2->next;
	}
    }
    return node1 == node2;  /* Nodes match here only if they are both null */
}

static char *
evalText(char *in)
{
    char *expr;
    Object *expr_result;
    Object *actual;
    char *tmp;
    char *tmp2;
    char *expr_start = NULL;
    char *expr_end = NULL;
    int len;
    boolean retain_original = FALSE;
    char *pos = in;
    char *remaining = in;
    char *start = newstr("");
    char *result;
    /* We don't use regexps for this as we do not have the necessary
       functions defined in regexp.c and this is a simple task that
       regexps may be overkill for. */

    while (*pos) {
	switch (*pos++) {
	case '\\':
	    /* We are at an escape character - skip the next character
	     * unless it is the end of the string */
	    if (*pos) {
		pos++;
	    }
	    break;
	case '=':
	    if (expr_start) {
		if (*pos == '>') {
		    retain_original = TRUE;
		    expr_end = pos;
		    pos++;
		}
	    }
	    break;
	case '$':
	    if (expr_start) {
		if (!expr_end) {
		    expr_end = pos;
		}
		expr = newstr("%s", expr_start);
		len = expr_end - expr_start - 1;
		expr[len] = '\0';
		expr_result = evalSexp(expr);
		
		tmp = newstr("%s", remaining);
		len = expr_start - remaining - 1;
		tmp[len] = '\0';
		tmp2 = newstr("%s%s", start, tmp);
		skfree(start);
		skfree(tmp);
		start = tmp2;
		actual = dereference(expr_result);
		if (actual) {
		    if (actual->type == OBJ_STRING) {
			tmp = newstr("%s", ((String *) actual)->value);
		    }
		    else {
			tmp = objectSexp(expr_result);
		    }
		    if (retain_original) {
			result = newstr("%s$%s => %s$", start, expr, tmp);
		    }
		    else {
			result = newstr("%s%s", start, tmp);
		    }
		    skfree(start);
		    start = result;
		    remaining = pos;

		    skfree(tmp);
		}
		skfree(expr);
		objectFree(expr_result, TRUE);
		expr_start = NULL;
		expr_end = NULL;
		retain_original = FALSE;
	    }
	    else {
		expr_start = pos;
	    }
	    break;
	}
    }
    result = newstr("%s%s", start, remaining);
    skfree(start);
    return result;
}

static xmlNode *
copyWithEval(xmlNode *node)
{
    xmlNode *new;
    char *text;
    if (node->type == XML_COMMENT_NODE) {
	text = evalText(node->content);
	new = xmlNewComment((xmlChar *) text);
	skfree(text);
    }
    else {
	new = xmlCopyNode(node, 0);
    }
    return new;
}

static xmlNode *
get_header_node (xmlNode *root)
{
    xmlNode *node = root;
    xmlChar *text = NULL;
    while (node->prev) {
	node = node->prev;
    }
    if (node->type != XML_ELEMENT_NODE) {
	return node;
    }	
    return NULL;
}

static void
add_header(xmlNode *root, xmlNode *header_node)
{
    xmlNode *node = header_node;
    xmlNode *new;
    while (header_node && (header_node->type != XML_ELEMENT_NODE)) {
	new = copyWithEval(header_node);
	(void) xmlAddPrevSibling(root, new);
	header_node = header_node->next;
    }
}

static xmlNode *
get_footer_node (xmlNode *root)
{
    return root->next;
}

static void
add_footer(xmlNode *prev, xmlNode *footer_node)
{
    xmlNode *node = footer_node;
    xmlNode *new;
    while (footer_node && (footer_node->type != XML_ELEMENT_NODE)) {
	new = copyWithEval(footer_node);
	(void) xmlAddNextSibling(prev, new);
	prev = new;
	footer_node = footer_node->next;
    }
}

static xmlNode *
writeScatterFile(String *path, String *name, xmlNode *node,
		 Document *template)
{
    String *pathroot = (String *) symbolGetValue("path");
    char *dirpath = newstr("%s/%s", pathroot->value, path->value);
    String *fullpath = stringNewByRef(newstr("%s/%s", dirpath, name->value));
    Document *prev_doc = NULL;
    xmlNode *scatter_root;
    xmlNode *scatter_start;
    xmlNode *prev_root;
    xmlNode *prev_start;
    xmlNode *textnode;
    DiffType diff;
    xmlNode *new_root; 
    Document *new_doc = NULL;
    xmlNode *header_node = NULL;
    xmlNode *footer_node = NULL;
    String *header = NULL;
    String *footer = NULL;
    FILE *scatterfile;
    xmlNode *result = NULL;
    char *result_text = NULL;
    char *result_prefix = NULL;
    Object *verbose = symbolGetValue("verbose");
    Object *checkonly = symbolGetValue("checkonly");
    Object *silent = symbolGetValue("silent");

    BEGIN {
	prev_doc = simpleDocFromFile(fullpath);
	scatter_root = firstElement(node->children);
	if (prev_doc) {
	    /* Find the first node below the dump node in both versions
	     * of the document */
	    prev_root = xmlDocGetRootElement(prev_doc->doc);
	    prev_start = firstElement(prev_root->children);
	    scatter_start = firstElement(scatter_root->children);
	    if (trees_match(prev_start, scatter_start)) {
		diff = IS_SAME;
		result_prefix = "UNCHANGED";
		header = footer = NULL;
	    }
	    else {
		diff = IS_DIFF;
		result_prefix = "MODIFIED";
		header_node = get_header_node(prev_root);
		footer_node = get_footer_node(prev_root);
	    }
	}
	else {
	    diff = IS_NEW;
	    result_prefix = "NEW";
	    makePath(dirpath); /* Ensure the parent dirs exist */

	    if (template) {
		prev_root = xmlDocGetRootElement(template->doc);
		header_node = get_header_node(prev_root);
		footer_node = get_footer_node(prev_root);
	    }
	}

	if (diff != IS_SAME) {
	    new_root = xmlCopyNode(scatter_root, 1);
	    new_doc = docForNode(new_root);
	    if (header_node) {
		add_header(new_root, header_node);
	    }
	    if (footer_node) {
		add_footer(new_root, footer_node);
	    }
	    if (!checkonly) {
		scatterfile = fopen(fullpath->value, "w");
		documentPrintXML(scatterfile, new_doc);
		fclose(scatterfile);
	    }
	}
	
	if (verbose || (diff != IS_SAME)) {
	    /* Only print the difference info for unchanged entries if
	     * the verbose flag has been provided. */
	    
	    if (!silent) {
		/* If the silent flag was provided, do not print
		   anything. */
		result = xmlNewNode(NULL, (xmlChar *) "print");
		result_text = newstr("%s: %s\n", result_prefix, 
				     fullpath->value);
		(void) xmlNodeAddContent(result, (xmlChar *) result_text);
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	skfree(dirpath);
	objectFree((Object *) prev_doc, TRUE);
	objectFree((Object *) fullpath, TRUE);
	objectFree((Object *) new_doc, TRUE);
	objectFree((Object *) header, TRUE);
	objectFree((Object *) footer, TRUE);
	if (result_text) {
	    skfree(result_text);
	}
    }
    END;
    
    return result;
}

static xmlNode *
scatterFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *path = nodeAttribute(template_node, "path");
    String *name;
    Document *template = NULL;
    xmlNode *result = NULL;

    if (path) {
	//result = processChildren(template_node, parent_node, 1);
	name = nodeAttribute(template_node, "name");
	if (!name) {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("name attribute must be provided for scatter"));
	}
	BEGIN {
	    template = scatterTemplate(path);
	    result = writeScatterFile(path, name, template_node, template);
	}
	EXCEPTION(ex);
	FINALLY {
	    objectFree((Object *) name, TRUE);
	    objectFree((Object *) path, TRUE);
	}
	END;
    }

    //xmlUnlinkNode(parent_node);
    //xmlFree(parent_node);
    
    return result;
}

static void
copyNodesOld(xmlNode *target, xmlNode *from)
{
    xmlNode *copy;
    xmlNode *text;
    xmlUnlinkNode(from);  /* Unlink and copy seems like overkill, but it
			   * is safe and eliminates undue copying of
			   * namespace definitions */
    copy = xmlCopyNode(from, 1);
    xmlFree(from);
    (void) xmlAddNextSibling(target, copy);
    text = xmlNewText((xmlChar *) "\n");
    (void) xmlAddNextSibling(target, text);
}

/* This is tricky because of the use of the skit namespace.  Attempts to 
 * do this without making a full copy and without xmlDOMWrapCloneNode
 * were completely unsuccessful */
static void
copyNodes(xmlNode *target, xmlNode *from)
{
    int	res;
    xmlNodePtr newptr;
    xmlNode *text;
    res = xmlDOMWrapCloneNode(NULL, NULL, from, &newptr,
			      target->doc, target->parent,
			      1, 0);
    if (res) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("xmlDOMWrapCloneNode has failed: %d\n", res));
    }
    (void) xmlAddNextSibling(target, newptr);
    text = xmlNewText((xmlChar *) "\n");
    (void) xmlAddNextSibling(target, text);
}

static xmlNode *
getElement(xmlNode *node)
{
    while (node && (node->type != XML_ELEMENT_NODE)) {
	node = node->next;
    }
    return node;
}

static boolean
processGatherNodes(xmlNode *node, char *filename);

static void
gatherDirIntoNode(xmlNode *node, char *dirname)
{
    char *globpath = newstr("%s/*.xml", dirname);
    glob_t globbuf;
    String *path;
    Document *gatherdoc;
    xmlNode *root;
    xmlNode *source;
    xmlNode *copy;
    int i;

    BEGIN {
	glob(globpath, 0, NULL, &globbuf);

	if (globbuf.gl_pathc) {
	    /* Some .xml files have been found.  Load each one in turn. */
	    for (i = 0; i < globbuf.gl_pathc; i++) {
		path = stringNewByRef(globbuf.gl_pathv[i]);
		gatherdoc = NULL;
		BEGIN {
		    gatherdoc = simpleDocFromFile(path);
		
		    root = xmlDocGetRootElement(gatherdoc->doc);
		    source = firstElement(root->children);
		    while (source) {
			copyNodes(node, source);
			/* Identify the new node created above */
			node = getElement(node->next);  
			/* RECURSE HERE! */
			(void) processGatherNodes(node, path->value);
			source = getElement(source->next);  
		    }
		}
		EXCEPTION(ex2);
		FINALLY {
		    objectFree((Object *) gatherdoc, TRUE);
		    objectFree((Object *) path, FALSE);
		}
		END;
	    }
	}
	else {
	    /* There were no xml files, so maybe this is a directory of
	     * directories.  Do another glob without the .xml extension */
	    i = strlen(globpath);
	    globpath[i - 4] = '\0';
	    glob(globpath, 0, NULL, &globbuf);
	    for (i = 0; i < globbuf.gl_pathc; i++) {
		gatherDirIntoNode(node, globbuf.gl_pathv[i]);
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	skfree(globpath);
	globfree(&globbuf);
    }
    END;
}

static void
gatherDocsIntoNode(xmlNode *node, char *filename)
{
    char *dirname = newstr("%s", filename);
    int len = strlen(dirname);

    if ((len > 4) && streq(dirname+len-4, ".xml")) {
	dirname[len-4] = '\0';
    }
    else {
	RAISE(PARAMETER_ERROR,
	      newstr("gather: filename (%s) must have a \".xml\" suffix", 
		     filename));
    }

    BEGIN {
	gatherDirIntoNode(node, dirname);
    }
    EXCEPTION(ex);
    FINALLY {
	skfree(dirname);
    }
    END;
}

static boolean
processGatherNodes(xmlNode *node, char *filename)
{
    xmlNode *cur_node = node;
    xmlNode *text_node = NULL;
    xmlNs *ns;
    while (cur_node) {
	if ((cur_node->type == XML_ELEMENT_NODE) &&
	    (ns = cur_node->ns) && 
	    streq(ns->prefix, "skit") &&
	    streq(cur_node->name, "gather")) 
	{
	    gatherDocsIntoNode(cur_node, filename);
	    /* Now we remove the gather node, and any preceding text node. */
	    if (text_node = cur_node->prev) {
		if (text_node->type == XML_TEXT_NODE) {
		    xmlUnlinkNode(text_node);
		    xmlFree(text_node);
		}
	    }
	    xmlUnlinkNode(cur_node);
	    xmlFree(cur_node);
	    return TRUE; /* We have found the single gather node in this
			    file - no need to look any further. */
	}
	if (processGatherNodes(cur_node->children, filename)) {
	    return TRUE;
	}
	cur_node = cur_node->next;
    }
    return FALSE;
}

static int
getNodeIndent(xmlNode *node, boolean prev)
{
    xmlNode *text;
    xmlChar *str;
    xmlChar c;
    int indent = 0;
    if (node) {
	text = prev? node->prev: node->next;
	if (text && (text->type == XML_TEXT_NODE)) {
	    str = text->content;
	    while (c = *str++) {
		if (c == ' ') {
		    indent++;
		}
		else {
		    indent = 0;
		}
	    }
	}
    }
    return indent;
}

static void
addIndent(int len, 
	  xmlNode *node,
	  boolean prev)
{
    char *spaces = skalloc(len + 1);
    xmlNode *text;
    spaces[len] = '\0';
    while (--len>= 0) {
	spaces[len] = ' ';
    }
    text = xmlNewText((xmlChar *) spaces);
    skfree(spaces);
    if (prev) {
	(void) xmlAddPrevSibling(node, text);
    }
    else {
	if (node->next) {
	    (void) xmlAddNextSibling(node->next, text);
	}
    }
}

static xmlNode *
reIndentNode(xmlNode *node, int target_indent)
{
    int len;
    xmlNode *text;
    xmlNode *last_sibling = NULL;
    xmlNode *last_child;
    char *spaces;
    while (node) {
	last_sibling = node;
	len = target_indent - getNodeIndent(node, TRUE);;
	/* Assume we only need to increase and not decrease the amount
	 * of indentation. */
	if (len > 0) {
	    addIndent(len, node, TRUE);
	}
	if (last_child  = reIndentNode(getElement(node->children), 
				       target_indent + 2)) {
	    len = target_indent - getNodeIndent(last_child, FALSE);
	    if (len > 0) {
		addIndent(len, last_child, FALSE);
	    }
	}
	node = getElement(node->next);
    }
    return last_sibling;
}

static void
reIndentDoc(Document *doc)
{
    xmlNode *node = xmlDocGetRootElement(doc->doc);
    /* Assume the outermost element is properly indented, and that it is
       the only element at this level of indentation. */
    reIndentNode(getElement(node->children), 2);
}

void
docGatherContents(Document *doc, String *filename)
{
    xmlNode *node = xmlDocGetRootElement(doc->doc);
    (void) processGatherNodes(node, filename->value);
    reIndentDoc(doc);
    //dbgSexp(doc);
}

static xmlNode *
gatherFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    xmlNode *result = processChildren(template_node, parent_node, 1);
    return result;
}


static xmlNode *
diffFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *diffrules = nodeAttribute(template_node, "rules");
    String *swap = nodeAttribute(template_node, "swap");
    Object *do_swap = evalSexp(swap->value);
    xmlNode *result;

    BEGIN {
	result = doDiff(diffrules, do_swap != NULL);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) swap, TRUE);
	objectFree((Object *) diffrules, TRUE);
    }
    END;

    return result;

#if 0
    Document *source_doc = NULL;
    Document *result_doc;
    Hash *dagnodes = NULL;
    Vector *sorted = NULL;
    xmlNode *root;
    xmlDocPtr xmldoc;

    BEGIN {
	if (input && (streq(input->value, "pop"))) {
	    source_doc = docStackPop();
	}
	sorted = gensort(source_doc);
	xmldoc = xmlNewDoc(BAD_CAST "1.0");
	root = parent_node? parent_node: xmlNewNode(NULL, BAD_CAST "root");
	xmlDocSetRootElement(xmldoc, root);
	result_doc = documentNew(xmldoc, NULL);

	treeFromVector(root, sorted);
	objectFree((Object *) sorted, TRUE);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) input, TRUE);
	objectFree((Object *) source_doc, TRUE);
    }
    END;
    return root;
#endif

}


static xmlNode *
execProcess(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *input = nodeAttribute(template_node, "input");
    Document *source_doc = NULL;
    Document *result_doc = NULL;
    xmlNode *result;
    xmlNode *root_node;

    BEGIN {
	if (input && (streq(input->value, "pop"))) {
	    source_doc = docStackPop();
	    if (!source_doc) {
		RAISE(PARAMETER_ERROR,
		      newstr("Failed to pop document for skit:process"));
	    }
	}
	else {
	    root_node = processChildren(template_node, NULL, depth + 1);
	    source_doc = docForNode(root_node);
	}

	if (!source_doc) {
	       RAISE(XML_PROCESSING_ERROR,
		     newstr("Failed to get contents for skit:process"));
	}
	root_node = xmlDocGetRootElement(source_doc->doc);
	result = processChildren(root_node, parent_node, depth + 1);
	result_doc = docForNode(root_node);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) source_doc, TRUE);
	objectFree((Object *) input, TRUE);
    }
    END;

    return result;
}

static void
initSkitProcessors()
{
    if (!skit_processors) {
	skit_processors = hashNew(TRUE);
	addProcessor("stylesheet", &stylesheetFn);
	addProcessor("options", &ignoreFn);  /* Options are processed in
						a previous partial pass */
	addProcessor("attribute", &attributeFn);
	addProcessor("attr", &attributeFn);
	addProcessor("exec", &execFn);
	addProcessor("runsql", &execRunsql);
	addProcessor("inclusion", &execKids);
	addProcessor("if", &execIf);
	addProcessor("let", &execLet);
	addProcessor("var", &execVar);
	addProcessor("foreach", &execForeach);
	addProcessor("result", &execResult);
	addProcessor("exception", &execException);
	addProcessor("function", &execDeclareFunction);
	addProcessor("exec_function", &execExecuteFunction);
	addProcessor("exec_func", &execExecuteFunction);
	addProcessor("xslproc", &execXSLproc);
	addProcessor("gensort", &execGensort);
	addProcessor("text", &textFn);
	addProcessor("process", &execProcess);
	addProcessor("scatter", &scatterFn);
	addProcessor("gather", &gatherFn);
	addProcessor("diff", &diffFn);
    }
}

static xmlNode *
exec(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *stylesheet_name = nodeAttribute(template_node, "stylesheet");
    String *input = nodeAttribute(template_node, "input");
    Document *stylesheet = NULL;
    Document *source_doc = NULL;
    Document *result_doc = NULL;
    xmlNode *result;
    xmlNode *root_node;

    BEGIN {
	if (!stylesheet_name) {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("stylesheet attribute must be provided for xslproc"));
	}

	stylesheet = findDoc(stylesheet_name);

	if (input && (streq(input->value, "pop"))) {
	    source_doc = docStackPop();
	}
	else {
	    root_node = processChildren(template_node, NULL, depth + 1);
	    source_doc = docForNode(root_node);
	}
	//dbgSexp(source_doc);
	//dbgSexp(stylesheet);
	result_doc = applyXSLStylesheet(source_doc, stylesheet);
	if (!result_doc) {
	    RAISE(XML_PROCESSING_ERROR,
		  newstr("Failed to process stylesheet: %s", 
			 stylesheet_name->value));
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) source_doc, TRUE);
	objectFree((Object *) stylesheet, TRUE);
	objectFree((Object *) stylesheet_name, TRUE);
	objectFree((Object *) input, TRUE);
    }
    END;

    result = xmlDocGetRootElement(result_doc->doc);
    //xmlUnlinkNode(result);  /* Remove the result node from result_doc. */
    objectFree((Object *) result_doc, FALSE);
    return result;
}


void
freeSkitProcessors()
{
    if (skit_processors) {
	objectFree((Object *) skit_processors, TRUE);
    }
}

static xmlNode *
execXmlNodeFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *name = stringNew((char *) template_node->name);
    FnReference *ref;
    xmlFn *fn;
    initSkitProcessors();
    if (ref = (FnReference *) hashGet(skit_processors, (Object *) name)) {
	fn = (xmlFn *) ref->fn;
	//printSexp(stderr, "ACTUAL:", debug_obj);
	//printSexp(stderr, "EXECUTING: ", name);
	objectFree((Object *) name, TRUE);
	return (*fn)(template_node, parent_node, depth);
    }
    else {
	objectFree((Object *) name, TRUE);
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("No processor defined for skit:%s", template_node->name));
    }
}

static xmlNode *
processNode(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    xmlNode *this = NULL;
    xmlNode *kids = NULL;
    xmlNs *ns;
    // TODO: Assert that this is a node!
    //fprintf(stderr, "processNode: template=%s, parent=%s\n", 
    //	    nodeName(template_node), nodeName(parent_node));
    if ((ns = template_node->ns) && streq(ns->prefix, "skit")) {
       this = execXmlNodeFn(template_node, parent_node, depth);
    }
    else if ((ns = template_node->ns) && streq(ns->prefix, "xi")) {
	/* We have found an xi:include node.  This will happen if
	 * inclusions have not yet been processed by finishDocument(),
	 * which should only be because we have deferred the processing
	 * of inclusions until the search path is fully knowable.  By
	 * raising this exception, processChildren will know that it is
	 * now time to perform that deferred processing and will retry
	 * processing this node. */
	RAISE(UNPROCESSED_INCLUSION, 
	      newstr("Unexpected xi inclusion encountered"));
    }
    else {
	this = xmlCopyNode(template_node, 2);
	BEGIN {
	    processChildren(template_node, this, depth + 1);
	}
	EXCEPTION(ex) {
	    //fprintf(stderr, "EXCEPTION PROCESS NODE\n");
	    if (this) xmlFreeNode(this);
	    RAISE();
	}
	END;
    }

    return this;
}

#ifdef THISISFORDEBUGGINGINCLUSIONSTUFF
static xmlChar *
getNodeURI(xmlNode *node)
{
    xmlNode *prev;
    xmlChar *contents;
    xmlChar *name;
    String *href;
    while (node) {
	fprintf(stderr, "XX: %s\n", nodeName(node));
	name = nodeName(node);
	if (name && streq(name, "inclusion")) {
	    prev = node->prev;
	    if (prev->type == XML_XINCLUDE_START) {
		fprintf(stderr, "HERE\n");
		xmlDebugDumpOneNode(stderr, prev, 1);
		if (href = nodeAttribute(prev, "href")) {
		    printSexp(stderr, "INCLUSION: ", (Object *) href);
		    objectFree((Object *) href, TRUE);
		}
	    }
	}
	node = node->parent;
    }
    return NULL;
}
#endif

static char *
nodeLocation(xmlNode *node)
{
    xmlDoc *doc = node->doc;
    xmlChar *uri = xmlNodeGetBase(doc, node);
    String *my_uri = stringNew(uri);
    Document *skitdoc = (Document *) doc->_private;
    Cons *cons = getDocumentInclusion(skitdoc, my_uri);
    char *filename;
    int line_no = node->line;
    //(void) getNodeURI(node);
    //fprintf(stderr, "ZZZ\n");

    if (cons) {
	filename = ((String *) cons->car)->value;
	line_no += ((Int4 *) cons->cdr)->value;
    }
    else {
	filename = (char *) doc->URL;
    }

    objectFree((Object *) my_uri, TRUE);
    xmlFree(uri);
    return newstr("%s:%d (element %s)", filename, line_no, node->name);
}

static xmlNode *
processElement(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    xmlNode *result = NULL;
    char *c;
    /*fprintf(stderr, "processElement: template=%s, parent=%s, base=%s\n", 
            nodeName(template_node), nodeName(parent_node), 
	    xmlNodeGetBase(template_node->doc, template_node)); */
    /* fprintf(stderr, "processElement: node=%s, base=%s\n", 
            nodeName(template_node), 
	    xmlNodeGetBase(template_node->doc, template_node)); */
	    
    BEGIN {
	switch (template_node->type) {
	case XML_ELEMENT_NODE:
	    result = processNode(template_node, parent_node, depth);
	}
    }
    EXCEPTION(ex) {
	char *location = nodeLocation(template_node);
	char *oldtext = ex->text;
	char *newtext = newstr("%s\nat %s", oldtext, location);
	//fprintf(stderr, "EXCEPTION PROCESS ELEMENT\n");
	ex->text = newtext;
	skfree(location);
	skfree(oldtext);
	RAISE();
    }
    END;
    return result;
}

/* If there is a parent add all children to the parent, otherwise
 * return only the first child as this will be the root element of a 
 * document. */
static xmlNode *
processRemaining(xmlNode *remaining, xmlNode *parent_node, int depth)
{
    xmlNode *prev_node = NULL;
    xmlNode *cur_node;
    xmlNode *next_node;
    xmlNode *child;
    boolean failed_once = FALSE;

    if (cur_node = remaining) {
	do {
	    next_node = cur_node->next;
	    BEGIN {
		child = processElement(cur_node, parent_node, depth);
	    }
	    EXCEPTION(ex);
	    WHEN(UNPROCESSED_INCLUSION) {
		/* This will happen because finishDocument() has not
		 * been called.  We don't want to call finishDocument()
		 * prematurely because the search path may be affected
		 * by the contents of the xml template before this
		 * point.  Now, that we have to perform an inclusion, we
		 * must execute finishDocument and, if successful,
		 * continue processing. */

		if (failed_once) {
		    RAISE(XML_PROCESSING_ERROR, newstr("%s", ex->text));
		}
		failed_once = TRUE;
		finishDocument(cur_template);
		if (prev_node) {
		    next_node = prev_node->next;
		}
		else {
		    next_node = cur_node->parent->children;
		}
	    }
	    END;
	    if (child) {
		if (parent_node) {
		    xmlAddChild(parent_node, child);
		}
		else {
		    while (next_node && (next_node->type == XML_TEXT_NODE)) {
			next_node = next_node->next;
		    }
		    if (next_node) {
			RAISE(XML_PROCESSING_ERROR, 
			      newstr("No parent provided for multiple "
				     "children elements.  Next element "
				     "is %s", nodeName(next_node)));
		    }
		    return child;
		}
	    }
	    prev_node = cur_node;
	} while (cur_node = next_node);
    }
    return NULL;
}

static xmlNode *
processChildren(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    return processRemaining(template_node->children, parent_node, depth);
}

/* This is the entry point for processing a template file. */
Document *
processTemplate(Document *template)
{
    xmlNode *root;
    xmlDocPtr doc; 
    xmlNode *newroot;
    Document *result = NULL;

    cur_template = template;
    root = xmlDocGetRootElement(template->doc);
    if (newroot = processNode(root, NULL, 1)) {
	doc = newroot->doc;
	if (!doc) {
	    doc = xmlNewDoc("1.0");
	    xmlDocSetRootElement(doc, newroot);
	}
	result = documentNew(doc, NULL);
    }
    return result;
}

static Object *
addParamAttribute(Object *obj, Object *params_node)
{
    String *key = (String *) ((Cons *) obj)->car;
    Cons *value = (Cons *) ((Cons *) obj)->cdr;
    char *param = NULL;
    xmlNode *node;

    switch (value->type) {
    case OBJ_SYMBOL:
	if (((Symbol *) value)->svalue) {
	    param = newstr("true");
	}
	else {
	    param = newstr("false");
	}
	break;
    case OBJ_STRING:
	param = newstr("%s", ((String *) value)->value);
	break;
    case OBJ_INT4:
	param = newstr("%d", ((Int4 *) value)->value);
	break;
    }
    if (param) {
	node = ((Node *) params_node)->node;
	(void)  xmlNewProp(node, key->value, param);
	skfree(param);
    }

    return (Object *) value;  /* Return the original contents of the hash
			       * entry */
}


void
addParamsNode(Document *doc, Object *params)
{
    xmlNode *root = xmlDocGetRootElement(doc->doc);
    xmlNode *first;
    xmlNode *param_node;
    Node *node;

    if (!root) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Unable to retrieve document root"));
    }

    param_node = xmlNewNode(NULL, (xmlChar *) "params");
    if (first = root->children) {
	xmlAddPrevSibling(first, param_node);
    }
    else {
	/* Looks like there are no contents here, so add params node
	 * as root's first child */
	xmlAddChild(root, param_node);
    }

    node = nodeNew(param_node);
    hashEach((Hash *) params, addParamAttribute, (Object *) node);
    objectFree((Object *) node, FALSE);

    return;
}


