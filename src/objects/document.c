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
	xmldoc->_private = doc;
    }
    return doc;
}

void 
documentFree(Document *doc, boolean free_contents)
{
    assert(doc && (doc->type == OBJ_DOCUMENT), 
	   "documentFree: doc is not a document");
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

	if (doc->doc) {
	    xmlFreeDoc(doc->doc);
	}

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
    if (!doc->inclusions) {
	doc->inclusions = hashNew(TRUE);
    }
    (void) hashAdd(doc->inclusions, (Object *) key, (Object *) contents);
}

Cons *
getDocumentInclusion(Document *doc, String *URI)
{
    if (doc) {
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
