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
xsltDBQuoteFunctionOld(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr instr;
    char *tmp;

    if (nargs != 1) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"dbquote() : expects one argument\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }

    xmlXPathStringFunction(ctxt, 1);

    if (ctxt->value->type != XPATH_STRING) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
			   "dbquot() : invalid arg expecting a string\n");
	ctxt->error = XPATH_INVALID_TYPE;
	return;
    }

    /* Now just return the parameter */
    instr = valuePop(ctxt);
    tmp = newstr(instr->stringval);
    valuePush(ctxt, xmlXPathNewString((xmlChar *) tmp));
    skfree(tmp);
    xmlXPathFreeObject(instr);
}

void
xsltDBQuoteFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr instr;
    String *result;
    String *str1;
    char *tmp;

    if (nargs != 1) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
		"dbquote() : expects one argument\n");
	ctxt->error = XPATH_INVALID_ARITY;
	return;
    }

    xmlXPathStringFunction(ctxt, 1);

    if (ctxt->value->type != XPATH_STRING) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
			   "dbquot() : invalid arg expecting a string\n");
	ctxt->error = XPATH_INVALID_TYPE;
	return;
    }

    instr = valuePop(ctxt);
    str1 = stringNewByRef(newstr(instr->stringval));
    xmlXPathFreeObject(instr);

    result = sqlDBQuote(str1, NULL);

    valuePush(ctxt, xmlXPathNewString((xmlChar *) result->value));
    objectFree((Object *) result, TRUE);
}

void
registerXSLTFunctions(xsltTransformContextPtr ctxt)
{
    xsltRegisterExtFunction(ctxt, (const xmlChar *) "dbquote",
			    (const xmlChar *) URI, xsltDBQuoteFunction);
}


