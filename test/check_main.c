/**
 * @file   check_main.c
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
 * \endcode
 * @brief  
 * Provides unit tests for low-level object handling using the check
 * unit testing framework: http://check.sourceforge.net/
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <string.h>
#include "../src/skit.h"
#include "../src/exceptions.h"
#include "suites.h"


/* This identifies the suppresions required by all tests, as all tests
 * use glib hash functions (due to the memory subsystem using hashes to
 * record debug info).
 */
START_TEST(hash_suppressions)
{
    printf("SUPPRESSION: glib_hash_suppression addChunk\n");
}
END_TEST

/* When a lot of hash memory is used, glib does some wacky stuff that
 * upsets valgrind.  This test uses enough memory to trigger the problem
 * and allow us to generate suppressions for it.
 */
START_TEST(morehash_suppressions)
{
    printf("SUPPRESSION: glib_hash_many_suppression g_slice_alloc\n");
    Hash *hash;
    Vector *vec = vectorNew(1001);
    int i;
    for (i = 0; i < 1000; i++) {
	hash = hashNew(TRUE);
	hashAdd(hash, (Object *) stringNewByRef(newstr("AAX%dQZX", i)), 
		(Object *) stringNewByRef(newstr("%d", 7*i)));
	hashAdd(hash, (Object *) stringNewByRef(newstr("qAX%dQZX", i)), 
		(Object *) stringNewByRef(newstr("%d", 12347*i)));
	vectorPush(vec, (Object *) hash);
    }
    objectFree((Object *) vec, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

/* This identifies suppressions required when using xmlreader.
 */
START_TEST(xmlreader_suppressions)
{
    xmlTextReaderPtr reader;
    int ret;

    printf("SUPPRESSION: xmlreader_suppression xmlReaderForFile\n");
    reader = xmlReaderForFile("test/templates/noname.xml", NULL, 0);

    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            ret = xmlTextReaderRead(reader);
        }
        xmlFreeTextReader(reader);
    }
    xmlCleanupParser();
}
END_TEST

static Document *
getSupDoc(char *name)
{
    String *docname = stringNew(name);
    Document *doc = findDoc(docname);
    objectFree((Object *) docname, TRUE);
    return doc;
}

static void
validateSupDoc(Document *doc, Document *rng_doc)
{
    xmlRelaxNGParserCtxtPtr ctx;
    xmlRelaxNGPtr schema;
    xmlRelaxNGValidCtxtPtr validator;

    ctx = xmlRelaxNGNewDocParserCtxt(rng_doc->doc);
    schema = xmlRelaxNGParse(ctx);
    validator = xmlRelaxNGNewValidCtxt(schema);

    (void) xmlRelaxNGValidateDoc(validator, doc->doc);

    xmlRelaxNGFreeValidCtxt(validator);
    xmlRelaxNGFree(schema);
    xmlRelaxNGFreeParserCtxt(ctx);
}

/* This identifies suppressions required when using relaxng validation.
 */
START_TEST(relaxng_suppressions)
{
    Document *list_template;
    Document *rng_doc;

    printf("SUPPRESSION: relaxng_suppression validateSupDoc\n");

    initBuiltInSymbols();
    initTemplatePath(".");

    list_template = getSupDoc("list.xml");
    rng_doc = getSupDoc("template.rng");

    validateSupDoc(list_template, rng_doc);

    objectFree((Object *) rng_doc, TRUE);
    objectFree((Object *) list_template, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(sql_suppressions)
{
    char *args[] = {"./skit", "--connect", 
		    "dbname = 'skittest' port = '54329'"};
    Hash *params;
    String *action;

    printf("SUPPRESSION: sql_suppression PQconnectdb\n");

    initBuiltInSymbols();
    initTemplatePath(".");

    record_args(3, args);
    action = nextAction();
    params = parseAction(action);
    executeAction(action, params);
    finalAction();
    sqlConnect();

    FREEMEMWITHCHECK;
}
END_TEST

/* THIS NO LONGER WORKS?!
START_TEST(xml_suppressions)
{
    char *args[] = {"./skit", "-t", "diff.xml"};

    initBuiltInSymbols();
    initTemplatePath(".");
    BEGIN {
	process_args2(3, args);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST
*/


static boolean reporting_only = FALSE;
static boolean nofork = FALSE;
static boolean suppressions = FALSE;
static boolean suppress_sql = FALSE;
static char suite_name[100];
static char test_name[100];

/* The suppressions suite is used to automagically generate suppresions
 * files for valgrind.  See valgrind/make_suppression and test/Makefile
 * for more info.
 * To run the suppressions suite as a normal unit test use -s
 * eg: make unit TESTS="-s"
 * or" skit_test -s
 */
static Suite *
suppressions_suite(void)
{
    Suite *s = suite_create("Suppressions");
    TCase *tc_s = tcase_create("Suppressions");
    
    ADD_TEST(tc_s, hash_suppressions);
    ADD_TEST(tc_s, morehash_suppressions);
    ADD_TEST(tc_s, xmlreader_suppressions);
    ADD_TEST(tc_s, relaxng_suppressions);
    //ADD_TEST(tc_s, xml_suppressions);
    if (suppress_sql) {
	ADD_TEST(tc_s, sql_suppressions);
    }
    suite_add_tcase(s, tc_s);
    return s;
}

static Suite *
base_suite(void)
{
    Suite *s = suite_create("");
    return s;
}

static boolean
string_matches(char *str1, char *str2)
{
    return (streq(str2, "") || streq(str1, str2));
}

static void
usage()
{
    fprintf(stderr, "Usage: skit_test -r\n"
	    "       skit_test [-n] [test_suite_name [test_name]]\n"
	    "       skit_test -s\n"
	    " -n is nofork for debugging\n"
	    " -s is for generating suppressions\n"
	    " -r is reporting only for showing available tests\n");
}

/** 
 * Read flags (-r, -n, -s or -S, and set suite_name and test_name if
 * they were provided as args.
 */
static void
handle_args(int argc, char *argv[])
{
    String *arg;
    record_args(argc, argv);
    while (arg = read_arg()) {
	if (arg->value[0] == '-') {
	    switch (arg->value[1]) {
	    case 'r': reporting_only = TRUE; break;
	    case 'n': nofork = TRUE; break;
	    case 's': nofork = suppressions = TRUE; break;
	    case 'S': nofork = suppressions = suppress_sql = TRUE; break;
	    default: usage(); break;
	    }
	    objectFree((Object *) arg, TRUE);
	}
	else {
	    strcpy(suite_name, arg->value);
	    objectFree((Object *) arg, TRUE);
	    if (arg = read_arg()) {
		strcpy(test_name, arg->value);
		objectFree((Object *) arg, TRUE);
	    }
	    return;
	}
    }
}

/**
 * Called from ADD_TEST() to conditionally add a test case.  The test
 * will only be added if the name matches test_name (possibly provided as
 * an arg to the test run) or if test_name is empty.
 */
boolean
check_test(char *testname, boolean report_this)
{
    if (reporting_only) {
	if (report_this) {
	    printf("%s %s\n", suite_name, testname);
	}
	return FALSE;
    }
    return string_matches(testname, test_name);
}

/**
 * Conditionally add the given suite to the test run.  It is added if
 * the name of the suite matches suite_name, or if suite_name is empty.
 */
#define ADD_SUITE(name)					\
    if (suppressions || reporting_only) {		\
	strcpy(suite_name, #name);			\
    }							\
    if (string_matches(#name, suite_name)) {		\
	srunner_add_suite(global_sr, name##_suite());		\
    }


SRunner *global_sr = NULL;

int
main(int argc, char *argv[])
{
    int number_failed;
    Symbol *fb_proc;
    String *fallback_processor;

    suite_name[0] = '\0';
    test_name[0] = '\0';
    handle_args(argc, argv);
    memShutdown();

    skit_register_signal_handler();
    global_sr = srunner_create(base_suite());

    if (nofork) {
	if ((!suppressions) && (streq(test_name, ""))) {
	    fprintf(stderr, "ERROR: Can only use -n with single tests\n");
	    usage();
	    return EXIT_FAILURE;
	}
	fprintf(stderr, "NOT FORKING!!!!\n");
	srunner_set_fork_status(global_sr, CK_NOFORK);
    }

    if (reporting_only || suppressions) {
	ADD_SUITE(suppressions);
	if (reporting_only) {
	    suppressions = FALSE;
	}
    }
    if (!suppressions) {
	initBuiltInSymbols();
	fb_proc = symbolNew("fallback_processor");
	fallback_processor = stringNew("deps/process_fallbacks.xsl");
	symSet(fb_proc, (Object *) fallback_processor);

	ADD_SUITE(objects);
	ADD_SUITE(options);
	ADD_SUITE(exceptions);
	ADD_SUITE(filepaths);
	ADD_SUITE(xmlfile);
	ADD_SUITE(params);
	ADD_SUITE(relaxng);
	ADD_SUITE(tsort);
	ADD_SUITE(deps);
    }

    if (!reporting_only) {
	srunner_set_log (global_sr, "test.log");

	srunner_run_all(global_sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(global_sr);
	srunner_free(global_sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    return 0;
}

