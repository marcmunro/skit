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
#include "skit_lib.h"
#include "exceptions.h"
#include <libxml/xmlreader.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>


static String empty_str = {OBJ_STRING, ""};
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
    return symbolGetValue("tuple");
}

static String *
filenameFromUri(const char *URI)
{
    String *filename;
    String *path;

    if ((URI == NULL) || (strncmp(URI, "skitfile:", 9))) {
        return NULL;
    }
    filename = stringNewByRef(newstr(URI + 9));
    path = findFile(filename);
    objectFree((Object *) filename, TRUE);
    return path;
}


static char *
nodeName(xmlNode *node)
{
    if (node) return (char *) node->name;
    return "nil";
}

static String *
getAttribute(xmlTextReaderPtr reader,
	     const xmlChar *name)
{
    String *result;
    xmlChar *value;
    if (value = xmlTextReaderGetAttribute(reader, name)) {
	result = stringNew((char *) value);
	xmlFree(value);
	return result;
    }
    return NULL;
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

static int
skitfileMatch(const char * URI) 
{
    if ((URI != NULL) && (!strncmp(URI, "skitfile:", 9))) {
        return 1;
    }
    return 0;
}

#define MAX_READAHEAD 255
typedef struct file_context {
    FILE *fp;
    char  buf[MAX_READAHEAD];
    int   buflen;
    int   bufpos;
    int   buflines;
    int   readlines;
    char  cur_str;
    String *URI;
    boolean  skipping;
} file_context;

static file_context *
new_file_context()
{
    file_context *fc = (file_context *) skalloc(sizeof(file_context));
    fc->fp = NULL;
    fc->buflen = 0;
    fc->bufpos = 0;
    fc->cur_str = 0;
    fc->buflines = 0;
    fc->readlines = 0;
    fc->URI = NULL;
    fc->skipping = TRUE;
}

static boolean
isquote(char c)
{
    return (c == '\'') || (c == '"');
}

/* Read up to MAXREADAHEAD characters into buf, returning complete
 * elements if possible.
 */
static void
read_item(file_context *context)
{
    int i = 0;
    char c;
    if (!context->fp) {
	/* File has been closed, so nothing to do */
	return;
    }
    context->readlines += context->buflines;
    context->buflines = 0;
    while (i < MAX_READAHEAD) {
	c = getc(context->fp);
	if (c == '\n') {
	    /* Count each newline */
	    context->buflines++;
	}
	if (c == EOF) {
	    /* If at EOF */
	    fclose(context->fp);
	    context->fp = NULL;
	    break;
	}
	if (isquote(c)) {
	    if (context->cur_str) {
		if (c == context->cur_str) {
		    /* Matching quote found */
		    context->cur_str = 0;
		}
	    }
	    else {
		/* Open quote found */
		context->cur_str  = c;
	    }
	}
	if ((c == '<') && (context->cur_str == 0) && (i != 0)) {
	    /* Looks like the start of an element, and we are not
	     * at the start of buf */
	    ungetc(c, context->fp);
	    break;
	}
	context->buf[i++] = c;

	if ((c == '>') && (context->cur_str == 0)) {
	    /* Looks like the end of an element */
	    break;
	}
    }
    context->buf[i] = '\0';
    context->bufpos = 0;
    context->buflen = i;
}

static char
next_char(file_context *context)
{
    while ((context->bufpos >= context->buflen) && (context->fp)) {
	read_item(context);
	if (context->skipping) {
	    /* We skip everything outside the <skit:inclusion> element */
	    if (strncmp(context->buf, "<skit:inclusion", 15) == 0) {
		/* We have reached the start of the 'real' inclusion.
		 * Record the number of lines we have skipped up to now.
		 */
		recordCurDocumentSkippedLines(context->URI, 
					      context->readlines);
		context->skipping = FALSE;
	    }
	    else {
		/* Make it look like we've read this entry */
		context->buflen = 0;
	    }
	}
	else {
	    if (strncmp(context->buf, "</skit:inclusion", 16) == 0) {
		/* We are done!  No need to read any more */
		fclose(context->fp);
		context->fp = NULL;
	    }
	}
    }

    if (context->bufpos < context->buflen) {
	return context->buf[context->bufpos++];
    }
    return EOF;
}

static void *
skitfileOpen(const char *URI) 
{
    file_context *fc;
    String *path = filenameFromUri(URI);
    String *my_uri;

    if (!path) {
	return NULL;
    }

    my_uri = stringNew(URI);
    fc = new_file_context();
    fc->fp = fopen(path->value, "r");
    fc->URI = my_uri;

    recordCurDocumentSource(fc->URI, path);
    objectFree((Object *) path, TRUE);
    return (void *) fc;
}


static int
skitfileClose(void * context) 
{
    file_context *fc = (file_context *) context;

    if (context == NULL) {
	return -1;
    }
    if (fc->URI) {
	objectFree((Object *) fc->URI, TRUE);
    }
    if (fc->fp) {
	fclose(fc->fp);
    }
    skfree(fc);
    return 0;
}




static int
skitfileRead(void *context, char *buffer, int len) 
{
    file_context *fc = (file_context *) context;
    FILE *fp = fc->fp;
    int   count = 0;
    char c;

    if ((context == NULL) || (buffer == NULL) || (len < 0)) {
	return -1;
    }

    while (count < len) {
	c = next_char(fc);
	if (c == EOF) {
	    break;
	}
	buffer[count++] = c;
    }
    return(count);
}


static
setup_input_readers()
{
    static done = FALSE;
    if (!done) {
	xmlRegisterDefaultInputCallbacks();

	if (xmlRegisterInputCallbacks(skitfileMatch, skitfileOpen, 
				      skitfileRead, skitfileClose) < 0) {
	    fprintf(stderr, "failed to register skitfile handler\n");
	    exit(1);
	}
	done = TRUE;
    }
}

static boolean
is_options_node(Document *doc)
{
    xmlChar *name;
    int type;
    boolean result = FALSE;
    if (xmlTextReaderNodeType(doc->reader) == XML_READER_TYPE_ELEMENT) {
	name = xmlTextReaderName(doc->reader);
	result = streq(name, "skit:options");
	xmlFree(name);
    }
    return result;
}

static Document *
create_document(String *path)
{
    xmlTextReaderPtr reader;
    setup_input_readers();

    reader = xmlReaderForFile(path->value, NULL, 0);
    if (reader == NULL) {
	return NULL;
    }
    return documentNew(NULL, reader);
}

static void
process_option_node(Document *doc)
{
    String *name = NULL;
    String *dflt = NULL;
    String *value = NULL;
    String *type = NULL;
    Object *validated_value = NULL;

    BEGIN {
	name = getAttribute(doc->reader, "name");
	dflt = getAttribute(doc->reader, "default");
	value = getAttribute(doc->reader, "value");
	type = getAttribute(doc->reader, "type");
	if (name) {
	    if (!type) {
		type = stringNew("flag");
	    }

	    if (value) {
		if (dflt) {
		    RAISE(PARAMETER_ERROR, 
			  newstr("Must provide value *or* default, not both"));
		}
		validated_value = validateParamValue(type, value);
		optionlistAdd(doc->options, stringDup(name), 
			      stringNew("value"), validated_value);
		objectFree((Object *) value, TRUE);
		value = NULL;
	    }

	    if (dflt) {
		validated_value = validateParamValue(type, dflt);
		optionlistAdd(doc->options, stringDup(name), 
			      stringNew("default"), validated_value);
		objectFree((Object *) dflt, TRUE);
		dflt = NULL;
	    }

	    optionlistAdd(doc->options, name, stringNew("type"), 
			  (Object *) type);
	}
	else {
	    RAISE(PARAMETER_ERROR, newstr("Option has no name"));
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION OPTION NODE\n");
    WHEN_OTHERS {
	objectFree((Object *) name, TRUE);
	objectFree((Object *) dflt, TRUE);
	objectFree((Object *) value, TRUE);
	objectFree((Object *) type, TRUE);
	RAISE();
    }
    END;
}

static void
process_alias_node(Document *doc)
{
    String *name = getAttribute(doc->reader, "for");
    String *alias = getAttribute(doc->reader, "value");
    BEGIN {
	if (name) {
	    if (alias) {
		optionlistAddAlias(doc->options, alias, name);
		alias = NULL;
		name = NULL;
	    }
	    else {
		RAISE(PARAMETER_ERROR,
		      newstr("Alias %s has no value.", name->value));
	    }
	}
	else {
	    RAISE(PARAMETER_ERROR, newstr("Alias has no name"));
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION ALIAS NODE\n");
    WHEN_OTHERS {
	objectFree((Object *) name, TRUE);
	objectFree((Object *) alias, TRUE);
	RAISE();
    }
    END;
}

static char *
errorContext(Document *doc)
{
    const xmlChar *uri = xmlTextReaderConstBaseUri(doc->reader);
    xmlNodePtr node = xmlTextReaderCurrentNode(doc->reader);
    return newstr("%s:%d in %s element", uri, node->line, node->name);
}

static void
process_possible_option(Document *doc)
{
    xmlChar *name;

    BEGIN {
	if (xmlTextReaderNodeType(doc->reader) == XML_READER_TYPE_ELEMENT) {
	    name = xmlTextReaderName(doc->reader);
	    if (streq(name, "option")) {
		process_option_node(doc);
	    }
	    else if (streq(name, "alias")) {
		process_alias_node(doc);
	    }
	    xmlFree(name);
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION PROCESS POSSIBLE OPTION\n");
    WHEN_OTHERS {
	char *context = errorContext(doc);
	char *newmsg = newstr("%s\n  AT %s", ex->text, context);
	skfree(context);
	xmlFree(name);
	RAISE(ex->signal, newmsg);
    }
    END;
}

/* Loads an xml document from a file, recording any options present in
 * the Document's options field.  Note that the document is not
 * complete: a call must be made to finishDocument to cause all include
 * directives to be processed.
 */
Document *
docFromFile(String *path)
{
    Document *doc = NULL;
    int ret;
    enum state_t {EXPECTING_OPTIONS, PROCESSING_OPTIONS, DONE} state;
    boolean reading_options = FALSE;
    boolean options_complete = FALSE;
    int     option_node_depth;

    state = EXPECTING_OPTIONS;
    BEGIN {
	doc = create_document(path);
	if (!doc) {
	    RAISE(PARAMETER_ERROR,
		  newstr("Cannot find file %s.", path->value));
	}
	ret = xmlTextReaderRead(doc->reader);
	while (ret == 1) {
	    switch (state) {
	    case EXPECTING_OPTIONS:
		if (is_options_node(doc)) {
		    doc->options = optionlistNew();
		    option_node_depth = xmlTextReaderDepth(doc->reader);
		    state = PROCESSING_OPTIONS;
		}
		break;
	    case PROCESSING_OPTIONS:
		if (xmlTextReaderDepth(doc->reader) == option_node_depth) {
		    state = DONE;
		}
		else {
		    process_possible_option(doc);
		}
	    }
	    xmlTextReaderPreserve(doc->reader);
	    ret = xmlTextReaderRead(doc->reader);
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION DOC FROM FILE\n");
    WHEN_OTHERS {
	objectFree((Object *) doc, TRUE);
	RAISE();
    }
    END;
    if (ret != 0) {
	objectFree((Object *) doc, TRUE);
	return NULL;
    }

    doc->doc = xmlTextReaderCurrentDoc(doc->reader);

    return doc;
}

Document *
applyXSLStylesheet(Document *src, Document *stylesheet)
{
    xmlDocPtr result = NULL;
    const char *params[1] = {NULL};
    if ((!stylesheet->stylesheet) && stylesheet->doc) {
	stylesheet->stylesheet = xsltParseStylesheetDoc(stylesheet->doc);
	stylesheet->doc = NULL;
    }
    result = xsltApplyStylesheet(stylesheet->stylesheet, 
				 src->doc, params);

    return documentNew(result, NULL);
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


/* Handle a skit:stylesheet element */
static xmlNode *
stylesheetFn(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    xmlNode *first_child;
    first_child = processChildren(template_node, parent_node, 1);
    return first_child;
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
hasExpr(xmlNode *node, Object **p_result)
{
    String *expr;
    Object *value;
    if (expr = nodeAttribute(node, "expr")) {
	BEGIN {
	    value = evalSexp(expr->value);
	}
	EXCEPTION(ex);
	//fprintf(stderr, "EXCEPTION HASEXPR \n");
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
getExpr(xmlNode *node)
{
    Object *value;
    (void) hasExpr(node, &value);
    return value;
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
    Object *field = NULL;
    Object *value;
    Object *container;
    String *strvalue = NULL;

    if (!name) {
	RAISE(XML_PROCESSING_ERROR,
	      newstr("No name field provided for skit_attr"));
    }
    if (hasExpr(template_node, &value)) {
	if (value) {
	    if (value->type != OBJ_STRING) {
		if (value->type != OBJ_OBJ_REFERENCE) {
		    strvalue = stringNewByRef(objectSexp(value));
		}
		else {
		    strvalue = (String *) objectCopy(dereference(value));
		}
		objectFree(value, TRUE);
	    }
	    else {
		strvalue = (String *) value;
	    }
	}
    }
    else {
	if (!(field = (Object *) nodeAttribute(template_node, "field"))) {
	    field = objectCopy((Object *) name);
	}
	container = curTuple();
	
	BEGIN {
	    strvalue = (String *) objSelect(container, field);
	}
	EXCEPTION(ex);
	//fprintf(stderr, "EXCEPTION ATTRIBUTEFN\n");
	WHEN(NOT_IMPLEMENTED_ERROR) {
	    char *exstr;
	    char *tmp = objectSexp(field);
	    objectFree((Object *) name, TRUE);
	    objectFree((Object *) field, TRUE);
	    exstr = newstr("Unable to select %s from current foreach record",
			   tmp);
	    skfree(tmp);

	    RAISE(XML_PROCESSING_ERROR, exstr);
	}
	END;
	// TODO: Make sure strvalue is really a string
    }
    addAttribute(parent_node, name, strvalue);
    objectFree((Object *) strvalue, TRUE);
    objectFree((Object *) name, TRUE);
    objectFree((Object *) field, TRUE);
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
    Symbol *sym = NULL;
    if (varname) {
	newSymbolScope();
	sym = symbolGet(varname->value);
	if (!sym) {
	    sym = symbolNew(varname->value);
	}
	setScopeForSymbol(sym);
	objectFree((Object *) varname, TRUE);
    }    

    BEGIN {
	//printSexp(stderr, "COLLECTION: ", collection);
	while (tuple = objNext(collection, &placeholder), placeholder) {
	    if (sym) {
		sym->svalue = tuple;
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
	if (sym) {
	    sym->svalue = NULL;
	    dropSymbolScope();
	}
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
    String *filetext = NULL;
    Cursor *cursor = NULL;
    String *sqltext = NULL;
    Connection *conn;
    Tuple *tuple;
    xmlNode *child = NULL;
    Symbol *sym;
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
	sqltext =  trimSqlText(filetext);
    
	conn = sqlConnect();

	cursor = sqlExec(conn, sqltext, NULL);
	//printSexp(stderr, "CURSOR: ", cursor);
	if (varname) {
	    sym = symbolNew(varname->value);
	    symbolSet(varname->value, (Object *) cursor);
	}
	else {
	    child = iterate((Object *) cursor, NULL, 
			    template_node, parent_node, depth);
	}
    }
    EXCEPTION(ex);
    //fprintf(stderr, "EXCEPTION RUNSQL\n");
    FINALLY {
	if (!varname) {
	    /* If a variable was defined, the cursor will be freed when
	     * that variable goes out of scope, otherwise free it now. */
	    objectFree((Object *) cursor, TRUE);
	}
	objectFree((Object *) filename, TRUE);
	objectFree((Object *) varname, TRUE);
	objectFree((Object *) sqltext, TRUE);
	objectFree((Object *) filetext, TRUE);
    }
    END;
    
    return child;
}

static xmlNode *
execForeach(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *fromname = nodeAttribute(template_node, "from");
    String *filter = nodeAttribute(template_node, "filter");
    Object *cursor;
    xmlNode *child = NULL;
    boolean doit = TRUE;
    Tuple *tuple;
    Object *result;

    BEGIN {
	if (!fromname) {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("var must be specified for foreach"));
	}

	cursor = symbolGetValue(fromname->value);
	if ((!cursor) || (!isCollection(cursor))) {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("from variable %s does not contain a collection", 
			 fromname->value));
	}

	child = iterate(cursor, filter, template_node, parent_node, depth);
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
    
    if (!sym) {
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

    while (current && (current->type != XML_ELEMENT_NODE)) {
	current = current->next;
    }
    while (TRUE) {
	if ((ns = current->ns) && streq(ns->prefix, "skit")) {
	    nodename = (char *) current->name;
	    if (streq(nodename, "parameter")) {
		getParam(template_node, current);
		current = current->next;
		continue;
	    }
	}
	break;
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
    xmlNode *result;

    BEGIN {
	if (!stylesheet_name) {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("stylesheet attribute must be provided for xslproc"));
	}

	stylesheet = findDoc(stylesheet_name);
	objectFree((Object *) stylesheet_name, TRUE);

	if (input && (streq(input->value, "pop"))) {
	    source_doc = (Document *) actionStackPop();
	    result_doc = applyXSLStylesheet(source_doc, stylesheet);
	}
	else {
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("No input specified for xslproc"));
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) source_doc, TRUE);
	objectFree((Object *) stylesheet, TRUE);
	objectFree((Object *) input, TRUE);
    }
    END;

    result = xmlDocGetRootElement(result_doc->doc);
    objectFree((Object *) result_doc, FALSE);
    return result;
}

/* Copy the contents of the dbobject node */
static xmlNode *
copyObjectNode(xmlNode *source)
{
    xmlNode *new = xmlCopyNode(source, 2);
    // TODO: copy any kids, recursively that are not dbobject nodes
    return new;
}

static char *
actionName(DagNode *node)
{
    if (node->fqn->value[0] == 'd') return "drop";
    return "build";
}

static void
addAction(xmlNode *node, char *action)
{
    xmlNewProp(node, "action", action);
}


static void
treeFromVector(xmlNode *parent_node, Vector *sorted_nodes)
{
    DagNode *dnode;
    xmlNode *prev = NULL;
    xmlNode *curnode;
    int i;

    for (i = 0; i < sorted_nodes->elems; i++) {
	dnode = (DagNode *) sorted_nodes->vector[i];
	//navigateToNode(dnode, parent_node,)
	curnode = copyObjectNode(dnode->dbobject);
	addAction(curnode, actionName(dnode));
	xmlAddChild(parent_node, curnode);
    }
}

static xmlNode *
execGensort(xmlNode *template_node, xmlNode *parent_node, int depth)
{
    String *input = nodeAttribute(template_node, "input");
    Document *source_doc = NULL;
    Hash *dagnodes = NULL;
    Vector *sorted = NULL;
    xmlNode *root;

    BEGIN {
	if (input && (streq(input->value, "pop"))) {
	    source_doc = (Document *) actionStackPop();
	}
	sorted = gensort(source_doc);
	root = parent_node? parent_node: xmlNewNode(NULL, BAD_CAST "root");
	treeFromVector(root, sorted);
	objectFree((Object *) sorted, TRUE);
	//RAISE(NOT_IMPLEMENTED_ERROR,
	//      newstr("execgensort is not implemented"));
    }
    EXCEPTION(ex);
    FINALLY {
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
	addProcessor("exception", &execException);
	addProcessor("function", &execDeclareFunction);
	addProcessor("exec_function", &execExecuteFunction);
	addProcessor("exec_func", &execExecuteFunction);
	addProcessor("xslproc", &execXSLproc);
	addProcessor("gensort", &execGensort);
    }
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
		    RAISE(XML_PROCESSING_ERROR, newstr(ex->text));
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

Document *
processTemplate(Document *template)
{
    xmlNode *root;
    xmlDocPtr doc; 
    xmlNode *newroot;
    Document *result;
    cur_template = template;
    root = xmlDocGetRootElement(template->doc);
    newroot = processNode(root, NULL, 1);
    doc = newroot->doc;
    if (!doc) {
	doc = xmlNewDoc("1.0");
	xmlDocSetRootElement(doc, newroot);
    }

    result = documentNew(doc, NULL);

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
	param = newstr(((String *) value)->value);
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


