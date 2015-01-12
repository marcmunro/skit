/**
 * @file   check_relaxng.c
 * \code
 *     Copyright (c) 2011 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
 * \endcode
 * @brief  
 * Unit tests for checking relaxng validation of templates, etc
 */


#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <regex.h>
#include "../src/skit.h"
#include "../src/exceptions.h"
#include "suites.h"

Document *
getDoc(char *name)
{
    String *volatile docname = stringNew(name);
    Document *doc = NULL;
    BEGIN {
	doc = findDoc(docname);
	readDocDbver(doc);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) docname, TRUE);
    }
    END;
    return doc;
}

static int
validateDoc(Document *doc, Document *rng_doc)
{
    int result;
    xmlRelaxNGParserCtxtPtr ctx;
    xmlRelaxNGPtr schema;
    xmlRelaxNGValidCtxtPtr validator;

    (void) xmlRelaxNGInitTypes();
    ctx = xmlRelaxNGNewDocParserCtxt(rng_doc->doc);
    schema = xmlRelaxNGParse(ctx);
    validator = xmlRelaxNGNewValidCtxt(schema);

    result = xmlRelaxNGValidateDoc(validator, doc->doc);

    xmlRelaxNGFreeValidCtxt(validator);
    xmlRelaxNGFree(schema);
    xmlRelaxNGFreeParserCtxt(ctx);
    (void) xmlRelaxNGCleanupTypes();
    return result;
}

static int
validate(char *doc_str, char *rng_str)
{
    Document *doc_template;
    Document *rng_doc;
    int result;

    rng_doc = getDoc(rng_str);
    doc_template = getDoc(doc_str);
    //dbgSexp(doc_template);
    result = validateDoc(doc_template, rng_doc);

    objectFree((Object *) rng_doc, TRUE);
    objectFree((Object *) doc_template, TRUE);

    return result;
}

START_TEST(check_list)
{
    int result;

    initTemplatePath(".");

    result = validate("list.xml", "template.rng");
    fail_if(result != 0,
	    "check_list: list.xml fails to validate");

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(check_deps)
{
    int result;

    initTemplatePath(".");

    result = validate("add_deps.xml", "template.rng");
    fail_if(result != 0,
	    "check_deps: add_deps.xml fails to validate");

    FREEMEMWITHCHECK;
}
END_TEST

Suite *
relaxng_suite(void)
{
    Suite *s = suite_create("RelaxNG");

    /* Core test case */
    TCase *tc_core = tcase_create("RelaxNGx");
    ADD_TEST(tc_core, check_list);
    ADD_TEST(tc_core, check_deps);
				
    suite_add_tcase(s, tc_core);

    return s;
}


