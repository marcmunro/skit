/**
 * @file   check_main.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
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
#include "../src/skit_lib.h"
#include "suites.h"



START_TEST(hash_suppressions)
{
    printf("SUPPRESSION: glib_hash_suppression addChunk\n");
}
END_TEST

Suite *
suppressions_suite(void)
{
    Suite *s = suite_create("Suppressions");
    TCase *tc_s = tcase_create("Suppressions");
    ADD_TEST(tc_s, hash_suppressions);
    suite_add_tcase(s, tc_s);
    return s;
}

Suite *
base_suite(void)
{
    Suite *s = suite_create("");
    return s;
}

static boolean reporting_only = FALSE;
static boolean nofork = FALSE;
static boolean suppressions = FALSE;
static char suite_name[100];
static char test_name[100];

boolean
string_matches(char *str1, char *str2)
{
    return (streq(str2, "") || streq(str1, str2));
}

void
usage()
{
    fprintf(stderr, "Usage: skit_test -r\n"
	    "       skit_test [-n] [test_suite_name [test_name]]\n"
	    "       skit_test -c\n"
	    " -n is nofork for debugging\n"
	    " -s is for generating suppressions\n"
	    " -r is reporting only for showing available tests\n");
}

void
handle_args(int argc, char *argv[])
{
    String *arg;
    record_args(argc, argv);
    while (arg = read_arg()) {
	if (arg->value[0] == '-') {
	    switch (arg->value[1]) {
	    case 'r': reporting_only = TRUE; break;
	    case 'n': nofork = TRUE; break;
	    case 's': nofork = TRUE; suppressions = TRUE; break;
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

#define ADD_SUITE(name)					\
    if (reporting_only) {				\
	strcpy(suite_name, #name);			\
    }							\
    if (string_matches(#name, suite_name)) {		\
	srunner_add_suite(sr, name##_suite());		\
    }

SRunner *global_sr;

int
main(int argc, char *argv[])
{
    int number_failed;
    SRunner *sr;
    suite_name[0] = '\0';
    test_name[0] = '\0';
    handle_args(argc, argv);
    memShutdown();

    skit_register_signal_handler();
    sr = srunner_create(base_suite());
    global_sr = sr;
    if (nofork) {
	fprintf(stderr, "NOT FORKING!!!!\n");
	srunner_set_fork_status (sr, CK_NOFORK);
    }

    if (suppressions || reporting_only) {
	ADD_SUITE(suppressions);
    }
    if (!suppressions) {
	ADD_SUITE(objects);
	ADD_SUITE(options);
	ADD_SUITE(exceptions);
	ADD_SUITE(filepaths);
	ADD_SUITE(xmlfile);
	ADD_SUITE(params);
    }

    if (!reporting_only) {
	srunner_set_log (sr, "test.log");

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    return 0;
}

