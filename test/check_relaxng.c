/**
 * @file   check_relaxng.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Unit tests for checking relaxng validation of templates, etc
 */


#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <regex.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"

Document *
getDoc(char *name)
{
    String *volatile docname = stringNew(name);
    Document *doc;
    BEGIN {
	doc = findDoc(docname);
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
    Document *list_template;
    Document *rng_doc;
    int result;

    initBuiltInSymbols();
    initTemplatePath(".");

    result = validate("list.xml", "template.rng");
    fail_if(result != 0,
	    "check_list: list.xml fails to validate");

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(check_deps)
{
    Document *list_template;
    Document *rng_doc;
    int result;

    initBuiltInSymbols();
    initTemplatePath(".");

    result = validate("add_deps.xml", "template.rng");
    fail_if(result != 0,
	    "check_deps: add_deps.xml fails to validate");

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(check_connect)
{
    Document *list_template;
    Document *rng_doc;
    int result;

    initBuiltInSymbols();
    initTemplatePath(".");

    result = validate("connect.xml", "template.rng");
    fail_if(result != 0,
	    "check_connect: connect.xml fails to validate");

    FREEMEMWITHCHECK;
}
END_TEST

static void
evalStr(char *str)
{
    char *tmp = newstr(str);
    Object *obj = evalSexp(tmp);
    objectFree(obj, TRUE);
    skfree(tmp);
}


START_TEST(check_extract)
{
    Document *list_template;
    Document *rng_doc;
    int result;

    initBuiltInSymbols();
    initTemplatePath(".");
    evalStr("(setq dbver (version '8.3'))");

    result = validate("zzz.xml", "template.rng");
    fail_if(result != 0,
	    "check_extract: extract.xml fails to validate");

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(check_test2)
{
    Document *list_template;
    Document *rng_doc;
    int result;

    initBuiltInSymbols();
    initTemplatePath(".");
    evalStr("(setq dbver (version '8.3'))");

    result = validate("test.xml", "test.rng");
    fail_if(result != 0,
	    "check_test: test.xml fails to validate");

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
    ADD_TEST(tc_core, check_connect);
    //ADD_TEST(tc_core, check_extract);
    //ADD_TEST(tc_core, check_test2);
				
    suite_add_tcase(s, tc_core);

    return s;
}


