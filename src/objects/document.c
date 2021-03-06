/**
 * @file   document.c
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating Document types.
 */

#include <stdio.h>
#include <string.h>
#include <libxml/xinclude.h>
#include "../skit.h"
#include "../exceptions.h"

static String boolean_str = {OBJ_STRING, "boolean"};


extern char *
nodestr(xmlNode *node)
{
    char tmp[800];
    char *end = &(tmp[0]);
    xmlAttrPtr attr;
    xmlChar *str;
    xmlChar *prop;

    if (node) {
	switch (node->type) {
	case XML_ELEMENT_NODE:
	    sprintf(tmp, "<%s", (char *) node->name);
	    end += strlen(end);
	    for(attr = node->properties; NULL != attr; attr = attr->next) {
		str = (xmlChar *) attr->name;
		prop = xmlGetProp(node, str);
		sprintf(end, " %s=\"%s\"", (char *) str, (char *) prop);
		xmlFree(prop);
		end += strlen(end);
	    }
	    sprintf(end, ">");
	    break;
	case XML_TEXT_NODE:
	    str = xmlNodeGetContent(node);
	    sprintf(tmp, "\"%s\"", (char *) str);
	    xmlFree(str);
	    break;
	default:
	    str = xmlNodeGetContent(node);
	    sprintf(tmp, "<!-- %s -->", (char *) str);
	    xmlFree(str);
	}
	return newstr("%s", tmp);
    }
    else {
	return newstr("nil");
    }
}

extern void
dumpNode(FILE *output, xmlNode *node)
{
    xmlDoc *doc = xmlNewDoc((const xmlChar *) "1.0");
    xmlNode *root = xmlNewNode(NULL, BAD_CAST "root");
    xmlNode *copy = xmlCopyNodeList(node);
    xmlDocSetRootElement(doc, root);
    xmlAddChildList(root, copy);

    xmlChar *xmlbuf;
    int buffersize;

    xmlDocDumpFormatMemory(doc, &xmlbuf, &buffersize, 1);
    fprintf(output, "%s", (char *) xmlbuf);
    xmlFree(xmlbuf);
    xmlFreeDoc(doc);
}

void
dNode(xmlNode *node)
{
    dumpNode(stderr, node);
}

extern void
printNode(FILE *output, char *label, xmlNode *node)
{
    char *str = nodestr(node);

    fprintf(output, "%s %s\n", label, str);
    skfree(str);
}

void
pNode(xmlNode *node)
{
    printNode(stderr, "", node);
}

Node *
nodeNew(xmlNode *node)
{
    Node *result = (Node *) skalloc(sizeof(Node));
    result->type = OBJ_XMLNODE;
    result->node = node;
    return result;
}

static void
setXPathContext(Document *doc)
{
    xmlDocPtr xmldoc = doc->doc;
    doc->xpath_context = xmlXPathNewContext(xmldoc);
}

xmlXPathObject *
xpathEval(Document *doc, xmlNode *node, char *expr)
{
    if (!doc->xpath_context) {
	setXPathContext(doc);
    }
    doc->xpath_context->node = node;
    return xmlXPathEvalExpression((xmlChar *) expr, doc->xpath_context);
}

Document *
documentNew(xmlDocPtr xmldoc, xmlTextReaderPtr reader)
{
    Document *doc;

    doc = (Document *) skalloc(sizeof(Document));
    doc->type = OBJ_DOCUMENT;
    doc->doc = xmldoc;
    doc->reader = reader;
    doc->stylesheet = NULL;
    doc->options = NULL;
    doc->inclusions = NULL;
    doc->xpath_context = NULL;
    if (xmldoc) {
	//fprintf(stderr, "OLD PRIVATE (1): %p\n", xmldoc->_private);
	xmldoc->_private = doc;
    }
    return doc;
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
    return fc;
}

static int
skitfileMatch(const char * URI) 
{
    if ((URI != NULL) && (!strncmp(URI, "skitfile:", 9))) {
        return 1;
    }
    return 0;
}

static String *
filenameFromUri(char *URI)
{
    String *filename;
    String *path;
    char *start;

    if ((URI == NULL) || (strncmp(URI, "skitfile:", 9))) {
        return NULL;
    }
    start = URI + 9;
    while (*start == '/') {
	if (*start == '\0') {
	    RAISE(GENERAL_ERROR, newstr("Disaster in filenameFromUri"));
	}
	start++;
    }
    filename = stringNewByRef(newstr("%s", start));
    path = findFile(filename);
    objectFree((Object *) filename, TRUE);
    return path;
}


static void *
skitfileOpen(const char *URI) 
{
    file_context *fc;
    String *path = filenameFromUri((char *) URI);
    String *my_uri;

    //fprintf(stderr, "Opening %s\n", URI);
    if (!path) {
	RAISE(FILEPATH_ERROR,
	      newstr("Unable to find file: %s", URI));
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

/* This code is slightly dangerous.  If the context->buf contains only
 * part of the skit_inclusion element, things are likely to go awry */
static char
next_char(file_context *context)
{
    while ((context->bufpos >= context->buflen) && (context->fp)) {
	read_item(context);
	if (context->skipping) {
	    /* We skip everything outside the <skit:inclusion> element */
	    if ((strncmp(context->buf, "<skit:inclusion", 15) == 0)||
		(strncmp(context->buf, "<xsl:stylesheet", 15) == 0)) {
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
	    if ((strncmp(context->buf, "</skit:inclusion", 16) == 0) ||
		(strncmp(context->buf, "</xsl:stylesheet", 16) == 0)) {
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

static boolean addDummyElement = FALSE;

static void here()
{
    //fprintf(stderr, "HERE\n");
    addDummyElement = FALSE;
    return;
}

static int
skitfileRead(void *context, char *buffer, int len) 
{
    file_context *fc = (file_context *) context;
    int   count = 0;
    char c;

    if ((context == NULL) || (buffer == NULL) || (len < 0)) {
	return -1;
    }

    while (count < len) {
	c = next_char(fc);
	if (c == EOF) {
	    if (addDummyElement) {
		here();
	    }
	    break;
	}
	buffer[count++] = c;
    }
    return(count);
}


static void
setup_input_readers()
{
    static boolean done = FALSE;
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
    boolean result = FALSE;
    if (xmlTextReaderNodeType(doc->reader) == XML_READER_TYPE_ELEMENT) {
	name = xmlTextReaderName(doc->reader);
	result = streq((char *) name, "skit:options");
	xmlFree(name);
    }
    return result;
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
process_option_node(Document *doc)
{
    String *volatile name = NULL;
    String *volatile dflt = NULL;
    String *volatile value = NULL;
    String *volatile type = NULL;
    String *required = NULL;
    Object *validated_value = NULL;

    BEGIN {
	name = getAttribute(doc->reader, (xmlChar *) "name");
	dflt = getAttribute(doc->reader, (xmlChar *) "default");
	value = getAttribute(doc->reader, (xmlChar *) "value");
	type = getAttribute(doc->reader, (xmlChar *) "type");
	required = getAttribute(doc->reader, (xmlChar *) "required");
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

	    if (required) {
		validated_value = validateParamValue(&boolean_str, 
						     required);
		optionlistAdd(doc->options, stringDup(name), 
			      stringNew("required"), validated_value);
		objectFree((Object *) required, TRUE);
		required = NULL;
	    }
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
    String *volatile name = getAttribute(doc->reader, (xmlChar *) "for");
    String *volatile alias = getAttribute(doc->reader, (xmlChar *) "value");
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
    xmlChar *volatile name;

    BEGIN {
	if (xmlTextReaderNodeType(doc->reader) == XML_READER_TYPE_ELEMENT) {
	    name = xmlTextReaderName(doc->reader);
	    if (streq((char *) name, "option")) {
		process_option_node(doc);
	    }
	    else if (streq((char *) name, "alias")) {
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
    Document *volatile doc = NULL;
    enum state_t {EXPECTING_OPTIONS, PROCESSING_OPTIONS, DONE} state;
    int ret = 0;
    int option_node_depth = 0;
    xmlTextReaderPtr reader;

    state = EXPECTING_OPTIONS;
    BEGIN {
	setup_input_readers();
	reader = xmlReaderForFile(path->value, NULL, 0);
	if (reader) {
	    doc = documentNew(NULL, reader);
	}
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
	    default:;   	/* To eliminate compiler warning. */
	    }
	    xmlTextReaderPreserve(doc->reader);
	    ret = xmlTextReaderRead(doc->reader);
	}
    }
    EXCEPTION(ex);
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
    doc->doc->_private = doc;
    return doc;
}

Document *
simpleDocFromFile(String *path)
{
    xmlTextReaderPtr reader;
    Document *doc = NULL;
    int ret;

    setup_input_readers();
    addDummyElement = TRUE;
    if (reader = xmlReaderForFile(path->value, NULL, 0)) {
	doc = documentNew(NULL, reader);
    }

    if (doc) {
	ret = xmlTextReaderRead(doc->reader);
	while (ret == 1) {
	    xmlTextReaderPreserve(doc->reader);
	    ret = xmlTextReaderRead(doc->reader);
	}
	if (ret != 0) {
	    objectFree((Object *) doc, TRUE);
	    return NULL;
	}
	doc->doc = xmlTextReaderCurrentDoc(doc->reader);
    }
    
    //objectFree((Object *) doc, TRUE);
    //xmlMemoryDump();
    return doc;
}


void 
documentFree(Document *doc, boolean free_contents)
{
    static int counter = 0;
    counter++;
    assert(doc && (doc->type == OBJ_DOCUMENT), 
	   "documentFree: doc is not a document");
    //fprintf(stderr, "free contents: %d, counter: %d\n", free_contents, counter);
    //dbgSexp(doc);
    if (free_contents) {
	/* Once we have parsed the stylesheet, it seems that the
	 * original doc should not be freed directly.  This is what I
	 * have  inferred from behaviour rather than read in the
	 * documentation, so I could be mistaken. */
	 
	if (doc->xpath_context) {
	    xmlXPathFreeContext(doc->xpath_context);
	}
	if (doc->reader) {
	    xmlFreeTextReader(doc->reader);
	}

	if (doc->stylesheet) {
	    xsltFreeStylesheet(doc->stylesheet);
	}

	//if (counter != 5) {
	if (doc->doc) {
	    xmlFreeDoc(doc->doc);
	}
	//}
	if (doc->options) {
	    objectFree((Object *) doc->options, TRUE);
	}

	if (doc->inclusions) {
	    objectFree((Object *) doc->inclusions, TRUE);
	}
    }
    skfree(doc);
}

char *
documentStr(Document *doc) 
{
    xmlChar *xmlbuf;
    int buffersize;
    char *result;

    if (doc->doc) {
	//xmlSetGenericErrorFunc(NULL, testErrorHandler);

	xmlDocDumpFormatMemory(doc->doc, &xmlbuf, &buffersize, 1);
	// Convert the result into something we can track within skit,
	// and free it.
	result = newstr("%s", (char *) xmlbuf);
	xmlFree(xmlbuf);
    }
    else {
	result = newstr("");
    }

    return result;
}

static xmlNode *
getFirstNode(Document *doc)
{
    xmlNode *root = xmlDocGetRootElement(doc->doc);
    xmlNode *node;
    
    if (!root) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Cannot access XML Document"));
    }

    node = root->children;
    while (node) {
	if (node->type == XML_ELEMENT_NODE) {
	    return node; 
	}
	node = node->next;
    }
    return NULL;
}

xmlNode *
getNextNode(xmlNode *node)
{
    while (node && (node->type != XML_ELEMENT_NODE)) {
	node = node->next;
    }
    return node;
}

static void 
printThis(FILE *fp, xmlNode *node)
{
    xmlChar *value;
    xmlNode *kid;
	
    if (value = xmlGetProp(node, (xmlChar *) "text")) {
	fprintf(fp, "%s", value);
	xmlFree(value);
    }
    else {
	/* No text attribute, so print any text elements instead */
	kid = node->children;
	while (kid) {
	    if (xmlNodeIsText(kid)) {
		value = xmlNodeGetContent(kid);
		fprintf(fp, "%s", value);
		xmlFree(value);
	    }
	    kid = kid->next;
	}
    }
}

static void
printPrintable(FILE *fp, xmlNode *node)
{
    if (node) {
	do {
	    if (streq((char *) node->name, "print")) {
		printThis(fp, node);
	    }
	    printPrintable(fp, getNextNode(node->children));
	} while (node = getNextNode(node->next));
    }    
}

void
documentPrint(FILE *fp, Document *doc)
{
    xmlNode *node = getFirstNode(doc);

    printPrintable(fp, node);
}

void
documentPrintXML(FILE *fp, Document *doc)
{
    xmlChar *xmlbuf;
    int buffersize;

    if (doc->doc) {
	xmlDocDumpFormatMemory(doc->doc, &xmlbuf, &buffersize, 1);
	fputs((char *) xmlbuf, fp);
	xmlFree(xmlbuf);
    }
}

static Document *cur_document = NULL;

/* Documents that have not processed their xinclude directives are
 * considered unfinished and will also not have had their reader objects
 * freed.
 */
void
finishDocument(Document *doc)
{
    cur_document = doc;
    if (doc->reader) {
	if (xmlXIncludeProcess(doc->doc) < 0) {
	    fprintf(stderr, "XInclude processing failed\n");
	    exit(1);
	}
	if (doc->reader) {
	    xmlFreeTextReader(doc->reader);
	    doc->reader = NULL;
	}
    }
    //fprintf(stderr, "OLD PRIVATE (2): %p\n", doc->doc->_private);
    doc->doc->_private = doc;
    cur_document = NULL;
}

static void
recordDocumentSource(Document *doc,
		     String *URI,
		     String *path)
{
    String *key = stringNew(URI->value);
    String *mypath = stringNew(path->value);
    Cons *contents = consNew((Object *) mypath, NULL);
    Object *old;
    if (!doc->inclusions) {
	doc->inclusions = hashNew(TRUE);
    }
    old = hashAdd(doc->inclusions, (Object *) key, (Object *) contents);
    if (old) {
	objectFree(old, TRUE);
    }
}

Cons *
getDocumentInclusion(Document *doc, String *URI)
{
    if (doc && doc->inclusions) {
	return (Cons *) hashGet(doc->inclusions, (Object *) URI);
    }
    return NULL;
}

static void
recordDocumentSkippedLines(Document *doc, String *URI, int lines)
{
    Int4 *my_lines;
    Cons *contents = getDocumentInclusion(doc, URI);
    if (contents) {
	my_lines = int4New(lines);
	contents->cdr = (Object *) my_lines;
    }
}

void
recordCurDocumentSource(String *URI, String *path)
{
    /* cur_document will not be set if we are dealing with an xsl
     * inclusion.  That's just too bad, so don't worry about it. */
    if (cur_document) {
	recordDocumentSource(cur_document, URI, path);
    }
}

void
recordCurDocumentSkippedLines(String *URI, int lines)
{
    recordDocumentSkippedLines(cur_document, URI, lines);
}

Document *
findDoc(String *filename)
{
    String *doc_path = findFile(filename);
    Document *doc;
    if (!doc_path) {
	Vector *roots = (Vector *) symbolGetValue("template-paths");
	Object *ver = symbolGetValue("dbver");
	dbgSexp(roots);
	dbgSexp(ver);
	RAISE(FILEPATH_ERROR, 
	      newstr("findDoc: cannot find \"%s\"", filename->value));
    }
    doc = docFromFile(doc_path);
    objectFree((Object *) doc_path, TRUE);
    if (!doc) {
	RAISE(FILEPATH_ERROR, 
	      newstr("findDoc: failed to open \"%s\"", filename->value));
    }
    finishDocument(doc);
    return doc;
}

boolean 
docIsPrintable(Document *doc)
{
    xmlNode *node = getFirstNode(doc);
    if (node) {
	return streq((char *) node->name, "printable");
    }
    return FALSE;
}

boolean 
docHasDeps(Document *doc)
{
    xmlNode *node = getFirstNode(doc);
    if (!node) {
	return FALSE;
    }

    if (streq((char *) node->name, "printable")) {
	node = getNextNode(node->next);
    }

    if (node && streq((char *) node->name, "params")) {
	node = getNextNode(node->next);
    }
    if (node && streq((char *) node->name, "dbobject")) {
	return TRUE;
    }
    return FALSE;
}

static Object *
findClusterNode(Object *this, Object *ignore)
{
    UNUSED(ignore);

    if (streq((char *) ((Node *) this)->node->name, "cluster")) {
	return (Object *) nodeNew(((Node *) this)->node);
    }
    return NULL;
}

static Node *
getClusterNode(Document *doc)
{
    return (Node *) xmlTraverse(doc->doc->children, &findClusterNode, NULL);
}

void
readDocDbver(Document *doc)
{
    Node *volatile node = getClusterNode(doc);
    String *volatile version_str = NULL;
    Object *volatile obj = NULL;
    char *sexp = NULL;

    BEGIN {
	if (node) {
	    version_str = nodeAttribute(node->node, "version");
	    sexp = newstr("(setq dbver-from-source (version '%s'))", 
			  version_str->value);
	    obj = evalSexp(sexp);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	if (sexp) {
	    skfree(sexp);
	}
	objectFree((Object *) node, TRUE);
	objectFree((Object *) version_str, TRUE);
	objectFree((Object *) obj, TRUE);
    }
    END;
}

/* Perform a depth-first traversal of an xml node tree from start,
 * applying traverser at each node.   If traverser returns an object,
 * traversal will terminate and the object will be returned to the
 * caller.
 */
Object *
xmlTraverse(xmlNode *start, TraverserFn *traverser, Object *param)
{
    xmlNode *cur = getNextNode(start);
    Node node = {OBJ_XMLNODE, NULL};
    node.node = cur;
    Object *result;
    result = (*traverser)((Object *) &node, param);
    cur = getNextNode(cur->children);
    while (cur && (!result)) {
	result = xmlTraverse(cur, traverser, param);
	cur = getNextNode(cur->next);
    }
    return result;
}



#ifdef deprecated

static void
assertXpathObj(void *obj, char *info)
{
    if (!obj) {
	RAISE(XPATH_EXCEPTION, newstr("%s", info));
    }
}

/* Deprecate the use of this: it seems flaky.  Use xmlTraverse instead. */
Object *
xpathEach(Document *doc, String *xpath,
	  TraverserFn *traverser, Object *param)
{
    volatile xmlXPathContextPtr context = NULL;
    volatile xmlXPathObjectPtr xpath_obj = NULL; 
    volatile *tmp_str = NULL;
    xmlNodeSetPtr nodelist;
    Node node = {OBJ_XMLNODE, NULL};
    int nodecount;
    int i;

    //dbgSexp(doc);
    //fprintf(stderr, "START\n");
    BEGIN {
	context = xmlXPathNewContext(doc->doc);
	assertXpathObj(context, "xpathEach: unable to establish context");
	xpath_obj = xmlXPathEvalExpression(
	    (xmlChar *) xpath->value, context);
	assertXpathObj(xpath_obj, 
		       tmp_str = newstr("xpathEach: failed to evaluate \"%s\"",
					xpath->value));

	nodelist = xpath_obj->nodesetval;
	nodecount = (nodelist) ? nodelist->nodeNr : 0;
    
	for(i = 0; i < nodecount; ++i) {
	    node.node = nodelist->nodeTab[i];
	    //dbgNode(node.node);
	    assertXpathObj(node.node, "xpathEach: missing node");
	    if(node.node->type == XML_ELEMENT_NODE) {
		param = (*traverser)((Object *) &node, param);
	    }
	    else {
		RAISE(NOT_IMPLEMENTED_ERROR, 
		      newstr("xpathEach: cannot handle node type %d",
			     node.node->type));
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	if (tmp_str) {
	    skfree(tmp_str);
	}

	if (xpath_obj) {
	    xmlXPathFreeObject(xpath_obj);
	}
	if (context) {
	    xmlXPathFreeContext(context); 
	}
    }
    END;
    //fprintf(stderr, "FINISH\n");

    return param;
}
#endif

String *
nodeAttribute(xmlNodePtr node, 
	      char *name)
{
    xmlChar *value = xmlGetProp(node, (xmlChar *) name);
    String *result;

    if (value) {
	result = stringNew((char *) value);
	xmlFree(value);
	return result;
    }
    return NULL;
}

boolean
nodeHasAttribute(xmlNodePtr node, char *name)
{
    xmlChar *value = xmlGetProp(node, (xmlChar *) name);

    if (value) {
	xmlFree(value);
	return TRUE;
    }
    return FALSE;
}


static String *
templateFilePath(String *filename, String *path, String *default_filename)
{
    char *pathname;
    FILE *fp = NULL;
    String *foundpath = NULL;

    if (filename) {
	/* Try filename as supplied */
	if (fp = fopen(filename->value, "r")) {
	    foundpath = stringNew(filename->value);
	}
	else {
	    /* Try filename prepended with path */
	    pathname = newstr("%s/%s", path->value, filename->value);

	    if (fp = fopen(pathname, "r")) {
		foundpath = stringNewByRef(pathname);
	    }
	    else {
		RAISE(GENERAL_ERROR, 
		      newstr("Cannot find header/footer file %s", 
			     filename->value));
	    }
	}
    }
    else if (default_filename) {
	/* Try default_filename as supplied */
	if (fp = fopen(default_filename->value, "r")) {
	    foundpath = stringNew(default_filename->value);
	}
	else {
	    /* Try default_filename prepended with path */
	    pathname = newstr("%s/%s", path->value, default_filename->value);
	    if (fp = fopen(pathname, "r")) {
		foundpath = stringNewByRef(pathname);
	    }
	    else {
		skfree(foundpath);
	
		/* Try searching the template directory tree for
		 * default_filename */
		foundpath = findFile(default_filename);
		if (fp != fopen(foundpath->value, "r")) {
		    objectFree((Object *) foundpath, TRUE);
		    RAISE(GENERAL_ERROR, 
			  newstr("Cannot find header/footer file %s", 
				 default_filename->value));
		}
	    }
	}
    }
    if (fp) {
	fclose(fp);
	return foundpath;
    }
    return NULL;
}

static Document *
readTemplateDoc(String *filename, String *path, String *default_filename)
{
    String *volatile filepath = templateFilePath(filename, path, 
						 default_filename);
    Document *doc = NULL;
    BEGIN {
	doc = simpleDocFromFile(filepath);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) filepath, TRUE);
    }
    END;
    return doc;
}

static String *prev_path = NULL;
static String *prev_templatename = NULL;
static Document *scatter_template = NULL;

static boolean
pathsMatch(String *path)
{
    if (prev_path) {
	if (streq(prev_path->value, path->value)) {
	    return TRUE;
	}
	objectFree((Object *) prev_path, TRUE);
    }
    prev_path = stringNew(path->value);
    return FALSE;
}

static boolean
templateMatch(String *template)
{
    if (prev_templatename) {
	if (streq(prev_templatename->value, template->value)) {
	    return TRUE;
	}
	objectFree((Object *) prev_templatename, TRUE);
    }
    prev_templatename = stringNew(template->value);
    return FALSE;
}

Document *
scatterTemplate(String *path)
{
    static String *default_template = NULL;
    String *filename = (String *) symbolGetValue("template2");

    if (!filename) {
	return NULL;
    }

    if (!default_template) {
	default_template = (String *) symbolGetValue("default_template");
    }

    if (pathsMatch(path) && templateMatch(filename)) {
	return scatter_template;
    }
    /* If we get here, we must read the header file */
    objectFree((Object *) scatter_template, TRUE);
    scatter_template = readTemplateDoc(filename, path, default_template);
    return scatter_template;
}

void
documentFreeMem()
{
    objectFree((Object *) prev_path, TRUE);
    objectFree((Object *) prev_templatename, TRUE);
    objectFree((Object *) scatter_template, TRUE);
    prev_path = NULL;
    prev_templatename = NULL;
    scatter_template = NULL;
}

xmlNode *
getText(xmlNode *node)
{
    while (node && (node->type != XML_TEXT_NODE)) {
	node = node->next;
    }
    return node;
}

#define DEPENDENCIES_STR "dependencies"
#define DEPENDENCY_STR "dependency"
#define DEPENDENCY_SET_STR "dependency-set"

boolean
isDependencySet(xmlNode *node)
{
    return streq((char *) node->name, DEPENDENCY_SET_STR);
}

boolean
isDependency(xmlNode *node)
{
    return streq((char *) node->name, DEPENDENCY_STR);
}

boolean
isDependencies(xmlNode *node)
{
    return streq((char *) node->name, DEPENDENCIES_STR);
}

boolean
isDepNode(xmlNode *node)
{
    return isDependency(node) || isDependencySet(node) || isDependencies(node);
}

xmlNode *
nextDependency(xmlNode *start, xmlNode *prev)
{
    xmlNode *node;
    if (prev) {
	node = firstElement(prev->next);
    }
    else {
	node = firstElement(start);
    }
    while (node && !isDepNode(node)) {
	node = firstElement(node->next);
    }
    return node;
}

static xmlNode *
firstDepNode(xmlNode *node)
{
    while (node && !isDepNode(node)) {
	node = firstElement(node->next);
    }
    return node;
}

xmlNode *
nextDepFromTree(xmlNode *start, xmlNode *prev)
{
    xmlNode *node;
    if (prev) {
	/* Try for a kid */
	node = firstDepNode(prev->children);
	if (!node) {
	    /* Try for a sibling. */
	    node = firstDepNode(prev->next);
	}
	if (!node) {
	    /* Go up the tree looking for siblings until we should go no
	     * further. */ 
	    prev = prev->parent;
	    while (!node && (prev != start->parent)) {
		node = firstDepNode(prev->next);
		prev = prev->parent;
	    }
	}
    }
    else {
	node = firstDepNode(start);
    }
    return node;
}



