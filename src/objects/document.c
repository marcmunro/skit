/**
 * @file   document.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating Document types.
 */

#include <stdio.h>
#include "../skit_lib.h"
#include "../exceptions.h"

static String boolean_str = {OBJ_STRING, "boolean"};



Node *
nodeNew(xmlNode *node)
{
    Node *result = (Node *) skalloc(sizeof(Node));
    result->type = OBJ_XMLNODE;
    result->node = node;
    return result;
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


static void *
skitfileOpen(const char *URI) 
{
    file_context *fc;
    String *path = filenameFromUri(URI);
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
    fprintf(stderr, "HERE\n");
    addDummyElement = FALSE;
    return;
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
    String *name = NULL;
    String *dflt = NULL;
    String *value = NULL;
    String *type = NULL;
    String *required = NULL;
    Object *validated_value = NULL;

    BEGIN {
	name = getAttribute(doc->reader, "name");
	dflt = getAttribute(doc->reader, "default");
	value = getAttribute(doc->reader, "value");
	type = getAttribute(doc->reader, "type");
	required = getAttribute(doc->reader, "required");
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

static char testErrors[32769];
static int testErrorsSize = 0;

static void
testErrorHandler(void *ctx  ATTRIBUTE_UNUSED, const char *msg, ...) {
    va_list args;
    int res;

    if (testErrorsSize >= 32768)
        return;
    va_start(args, msg);
    res = vsnprintf(&testErrors[testErrorsSize],
                    32768 - testErrorsSize,
		    msg, args);
    va_end(args);
    if (testErrorsSize + res >= 32768) {
        /* buffer is full */
	testErrorsSize = 32768;
	testErrors[testErrorsSize] = 0;
    } else {
        testErrorsSize += res;
    }
    testErrors[testErrorsSize] = 0;
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
	result = newstr((char *) xmlbuf);
	xmlFree(xmlbuf);
    }
    else {
	result = newstr("");
    }

    return result;
}

xmlNode *
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
    node = node->next;
    while (node) {
	if (node->type == XML_ELEMENT_NODE) {
	    return node; 
	}
	node = node->next;
    }
    return NULL;
}

xmlNode *
nextPrintableNode(xmlNode *node)
{
    while (node = getNextNode(node)) {
	if (streq(node->name, "print")) {
	    return node;
	}
    }
    return NULL;
}

void
documentPrint(FILE *fp, Document *doc)
{
    xmlNode *node = getFirstNode(doc);
    xmlNode *kid;
    xmlChar *value;

    while (node = nextPrintableNode(node)) {
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
}

void
documentPrintXML(FILE *fp, Document *doc)
{
    xmlChar *xmlbuf;
    int buffersize;

    if (doc->doc) {
	xmlDocDumpFormatMemory(doc->doc, &xmlbuf, &buffersize, 1);
	fputs(xmlbuf, fp);
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

void
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
	RAISE(FILEPATH_ERROR, 
	      newstr("findDoc: cannot find \"%s\"", filename->value));
    }
    doc = docFromFile(doc_path);
    objectFree((Object *) doc_path, TRUE);
    finishDocument(doc);
    return doc;
}

boolean 
docIsPrintable(Document *doc)
{
    xmlNode *node = getFirstNode(doc);
    if (node) {
	return streq(node->name, "printable");
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

    if (streq(node->name, "printable")) {
	node = getNextNode(node);
    }

    if (node) {
	return streq(node->name, "dbobject");
    }
    return FALSE;
}

static Object *
setFoundNode(Object *node, Object *result)
{
    ((Node *) result)->node = ((Node *) node)->node;
    return result;
}

static Node *
getClusterNode(Document *doc)
{
    String *xpath_expr = stringNew("//cluster");
    Node *node = nodeNew(NULL);

    BEGIN {
	(void) xpathEach(doc, xpath_expr, &setFoundNode, (Object *) node);
    }
    EXCEPTION(ex) {
	objectFree((Object *) node, TRUE);
    }
    FINALLY {
	objectFree((Object *) xpath_expr, TRUE);
    }
    END;

    return node;
}

void
readDocDbver(Document *doc)
{
    Node *node = getClusterNode(doc);
    String *version_str = NULL;
    char *sexp = NULL;
    Object *obj = NULL;

    BEGIN {
	if (node->node) {
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

static void
assertXpathObj(void *obj, char *info)
{
    if (!obj) {
	RAISE(XPATH_EXCEPTION, newstr(info));
    }
}

Object *
xpathEach(Document *doc, String *xpath,
	  TraverserFn *traverser, Object *param)
{
    xmlXPathContextPtr context = NULL;
    xmlXPathObjectPtr xpath_obj = NULL; 
    xmlNodeSetPtr nodelist;
    Node node = {OBJ_XMLNODE, NULL};
    char *tmp_str = NULL;
    int nodecount;
    int i;

    //dbgSexp(doc);
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

    return param;
}

String *
nodeAttribute(xmlNodePtr node, 
	      const xmlChar *name)
{
    xmlChar *value = xmlGetProp(node, name);
    String *result;

    if (value) {
	result = stringNew((char *) value);
	xmlFree(value);
	return result;
    }
    return NULL;
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
    String *filepath = templateFilePath(filename, path, default_filename);
    Document *doc;
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

