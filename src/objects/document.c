/**
 * @file   document.c
 * \code
 *     Copyright (c) 2008 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides functions for manipulating Document types.
 */

#include <stdio.h>
#include "../skit_lib.h"


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
	xmlFreeTextReader(doc->reader);
	doc->reader = NULL;
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
    hashAdd(doc->inclusions, (Object *) key, (Object *) contents);
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
    Int4 *my_lines = int4New(lines);
    Cons *contents = getDocumentInclusion(doc, URI);
    if (contents) {
	contents->cdr = (Object *) my_lines;
	}
}

void
recordCurDocumentSource(String *URI, String *path)
{
    recordDocumentSource(cur_document, URI, path);
}

void
recordCurDocumentSkippedLines(String *URI, int lines)
{
    recordDocumentSkippedLines(cur_document, URI, lines);
}

