/**
 * @file   libxslt.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provide extension xpath functions for libxslt
 *
 */

#include <stdio.h>
#include <string.h>
#include "skit_lib.h"
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include<libxslt/extensions.h>

static char URI[] ="http://www.bloodnok.com/xml/skit";

void
xsltDBQuoteFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr instr;
    xmlXPathObjectPtr instr2;
    String *result;
    String *str1;
    String *str2 = NULL;

    if (nargs == 2) {
	xmlXPathStringFunction(ctxt, 1);

	if (ctxt->value->type != XPATH_STRING) {
	    xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
			       "dbquote() : invalid arg expecting a string\n");
	    ctxt->error = XPATH_INVALID_TYPE;
	    return;
	}
	instr2 = valuePop(ctxt);

	str2 = stringNewByRef(newstr(instr2->stringval));
    }

    if ((nargs < 1) || (nargs > 2)) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"dbquote() : expects one or two  arguments\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }

    xmlXPathStringFunction(ctxt, 1);

    if (ctxt->value->type != XPATH_STRING) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
			   "dbquote() : invalid arg expecting a string\n");
	ctxt->error = XPATH_INVALID_TYPE;
	return;
    }

    instr = valuePop(ctxt);
    str1 = stringNewByRef(newstr(instr->stringval));
    xmlXPathFreeObject(instr);

    result = sqlDBQuote(str1, str2);

    valuePush(ctxt, xmlXPathNewString((xmlChar *) result->value));
    objectFree((Object *) result, TRUE);
}

void
registerXSLTFunctions(xsltTransformContextPtr ctxt)
{
    xsltRegisterExtFunction(ctxt, (const xmlChar *) "dbquote",
			    (const xmlChar *) URI, xsltDBQuoteFunction);
}


