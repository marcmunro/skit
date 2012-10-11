/**
 * @file   check_except.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides unit tests for exception handling using the check
 * unit testing framework: http://check.sourceforge.net/
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <check.h>
#include <regex.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"

static int handled_by = 0;
static int last_exception = 0;

int 
raiser1(int i)
{
    RAISE(i, newstr("raised by raiser1"));
    return 0;
}

int 
raiser2(int i)
{
    BEGIN {
	RAISE(i, newstr("raised by raiser2"));
    }
    EXCEPTION(ex);
    WHEN(30) {
	handled_by |= 1;
	RETURN(ex->signal);
    }
    WHEN(31) {
	handled_by |= 2;
	RAISE();
    }
    WHEN_OTHERS {
	handled_by |= 4;
    }
    END;
    return 0;
}

// Return directly from within an exception begin block.
int 
called3(int i)
{
    BEGIN {
	RETURN(i);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	handled_by |= 64;
    }
    END;
    return 42;
}

int 
raiser3(int i)
{
    RAISE(called3(i), newstr("raised by raiser3"));
    return 47;
}

int 
catcher(
    int i,
    int (*fn)(int))
{
    handled_by = 0;
    last_exception = 0;
    BEGIN {
	RETURN(fn(i));
    }
    EXCEPTION(e);
    WHEN(98) {
	handled_by |= 8;
	last_exception = e->signal;
	RETURN(e->signal);
    }
    WHEN(99) {
	handled_by |= 16;
	last_exception = e->signal;
	RAISE();
    }
    WHEN_OTHERS {
	handled_by |= 32;
	last_exception = e->signal;
	RETURN(e->signal);
    }
    END;
    return 0;
}


int 
catcher2(volatile int i)
{
    int zero = 0;
    BEGIN {
	switch (i) {
	case 1:
	case 2:	
	case 3: i = i / zero; break;
	case 4:	RETURN(i);
	case 6: 
	case 7: 
	case 8: RAISE(44, newstr("TEST CASE 8"));
	case 9: RAISE();
	case 10: RAISE(20, newstr("TEST CASE 20"));
	case 11: RAISE(21, newstr("TEST CASE 21"));
	case 14: RAISE(114, newstr("TEST CASE 14"));
	case 15: RAISE(115, newstr("TEST CASE 15"));
	}
    }
    EXCEPTION(e);
    WHEN_OTHERS {
	switch (i) {
	case 1: RETURN(e->signal);
	case 2: RETURN(66);
	case 6: RETURN(e->signal + 1);
	case 7: RAISE();
	case 8: RAISE(79, newstr("A DIFFERENT EXCEPTION"));
	case 9: RETURN(e->signal);
	case 10: 
	case 11: 
	case 14: 
	case 15: RAISE();
	}
    }
    END;
    return 0;
}

int 
catcher3(int i)
{
    int result = 0;
    BEGIN {
	result = catcher2(i);
    }
    EXCEPTION(ex);
    WHEN(20) {
	RAISE();
    }
    WHEN(21) {
	RAISE(22, newstr("22"));
    }
    WHEN(115) {
	RAISE();
    }
    WHEN_OTHERS {
	RETURN(ex->signal);
    }
    END;
    return result;
}

int 
catcher4(int i)
{
    int result = 0;
    BEGIN {
	result = catcher3(i);
    }
    EXCEPTION(ex);
    WHEN(115) {
	RAISE();
    }
    WHEN_OTHERS {
	RETURN(ex->signal);
    }
    END;
    return result;
}

int 
ignore3(int i)
{
    int result = 0;
    BEGIN {
	result = catcher2(i);
    }
    EXCEPTION(ex);
    WHEN(116) {
	RAISE();
    }
    END;
    return result;
}
int 
ignore4(int i)
{
    int result = 0;
    BEGIN {
	result = ignore3(i);
    }
    EXCEPTION(ex);
    WHEN(116) {
	RAISE();
    }
    END;
    return result;
}

char *
catcher5(int i)
{
    int result = 0;
    BEGIN {
	result = ignore4(i);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	RETURN(newstr(ex->backtrace));
    }
    END;
    return NULL;
}

// Check that div by zero exception is caught and memory freed properly
START_TEST(exceptions_new1)
{
    int a;
    a = catcher2(1);
    fail_unless(a == 8, "Function result 8 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that div by zero exception is caught and memory freed properly
START_TEST(exceptions_new2)
{
    int a;
    a = catcher2(2);
    fail_unless(a == 66, "Function result 66 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that handled exceptions fall through to main body of code
START_TEST(exceptions_new3)
{
    int a;
    a = catcher2(3);
    fail_unless(a == 0, "Function result 0 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that exception memory is freed properly when no exceptions occur.
START_TEST(exceptions_new4)
{
    int a;
    a = catcher2(4);
    fail_unless(a == 4, "Function result 4 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that when no exceptions ocuur, main body of code continues
START_TEST(exceptions_new5)
{
    int a;
    a = catcher2(5);
    fail_unless(a == 0, "Function result 0 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that exceptions may be raised explicitly by a raise statement
START_TEST(exceptions_new6)
{
    int a;
    a = catcher2(6);
    fail_unless(a == 45, "Function result 45 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that exceptions are re-raised properly in an exception block
START_TEST(exceptions_new7)
{
    int a;
    a = catcher3(7);
    fail_unless(a == 44, "Function result 44 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that exceptions are raised properly from within exception handler
START_TEST(exceptions_new8)
{
    int a;
    a = catcher3(8);
    fail_unless(a == 79, "Function result 79 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that RAISE() with no parameters outside of an exception handler
// causes an error.
START_TEST(exceptions_new9)
{
    int a;
    a = catcher3(9);
    fail_unless(a == UNKNOWN_EXCEPTION, 
		"Function result UNKNOWN_EXCEPTION expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that exception stack looks ok after a bit more nesting
START_TEST(exceptions_new10)
{
    int a;
    a = catcher4(10);
    fail_unless(a == 20, "Function result 20 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Check that exception stack looks ok after re-raising in the middle
// of the nest
START_TEST(exceptions_new11)
{
    int a;
    a = catcher4(11);
    fail_unless(a == 22, "Function result 22 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

static int
do_exceptions_new12(void *ignore)
{
    RAISE();
}


// Check that RAISE() fails properly when no exception handler is defined
START_TEST(exceptions_new12)
{
    char *stderr;
    char *stdout;
    int   signal;
    captureOutput(do_exceptions_new12, NULL, &stdout, &stderr, &signal);

    if (signal != SIGTERM) {
	fail("Expected SIGTERM, got %d\n", signal);
    }
    fail_unless(contains(stderr, "Stupid attempt to re-raise"));
    free(stdout);
    free(stderr);
    FREEMEMWITHCHECK;
}
END_TEST

static int
do_exceptions_new13(void *ignore)
{
    RAISE(71, newstr("LOOKY HERE"));
}

// Check that RAISE(x) fails properly when no exception handler is defined
START_TEST(exceptions_new13)
{
    char *stderr;
    char *stdout;
    int   signal;
    captureOutput(do_exceptions_new13, NULL, &stdout, &stderr, &signal);

    if (signal != SIGTERM) {
	fail("Expected SIGTERM, got %d\n", signal);
    }
    fail_unless(contains(stderr, "LOOKY HERE"));
    free(stdout);
    free(stderr);
    FREEMEMWITHCHECK;
}
END_TEST

static int
do_exceptions_new14(void *ignore)
{
    int a;
    a = catcher2(14);
    fail_unless(a == 22, "Function result 22 expected, got %d\n", a);
}

// Check that RAISE fails properly from an exception block
// when no exception handler is defined
START_TEST(exceptions_new14)
{
    char *stderr;
    char *stdout;
    int   signal;
    captureOutput(do_exceptions_new14, NULL, &stdout, &stderr, &signal);

    fail_unless(contains(stderr, "TEST CASE 14", 
			 "exceptions_new14"));
    free(stdout);
    free(stderr);
    FREEMEMWITHCHECK;
    if (signal != SIGTERM) {
	fail("Expected SIGTERM, got %d\n", signal);
    }
}
END_TEST


static char *
find(char *src, char *expr)
{
    regex_t regex;
    int errcode;
    char *result = NULL;
    regmatch_t pmatch [1]; 

    if (errcode = regcomp(&regex, expr, REG_EXTENDED)) {
	// Something went wrong with the regexp compilation
	char errmsg[200];
	(void) regerror(errcode, &regex, errmsg, 199);
	myfail(newstr("contains: regexp compilation failure: %s", errmsg));
    }
    
    if (regexec(&regex, src, 1, pmatch, 0) == 0) {
	result = src + pmatch[0].rm_so;
    }
    regfree(&regex);
    return result;
}



// Check the resulting text after a nested exception stack
START_TEST(exceptions_new15)
{
    char *result = catcher5(15);
    char *found = result;
    int count = -1;

    while(found) {
	found = find(found+1, "Exception 115:");
	count++;
    }

    fail_unless(count == 4,
		"Incorrect count of excecptions found in backtrace: %d", count);
    skfree(result);
    FREEMEMWITHCHECK;
}
END_TEST


// Check that when_others is called.
START_TEST(exceptions_1)
{
    int a;
    a = catcher(100, raiser1);
    fail_unless(a == 100, "Function result 100 expected, got %d\n", a);
    fail_unless(last_exception == 100, 
		"Exception 100 expected, got %d\n", last_exception);
    fail_unless(handled_by == 32, 
		"Unexpected exception handling %d\n", handled_by);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(exceptions_2)
{
    int a;
    a = catcher(98, raiser1);
    fail_unless(a == 98, "Function result 98 expected, got %d\n", a);
    fail_unless(last_exception == 98, 
		"Exception 98 expected, got %d\n", last_exception);
    fail_unless(handled_by == 8, 
		"Unexpected exception handling %d\n", handled_by);
    FREEMEMWITHCHECK;
}
END_TEST

// Test for unhandled exceptions
static int
do_exceptions_3(void *ignore)
{
    int signal;
    signal = catcher(99, raiser1);
}

START_TEST(exceptions_3)
{
    char *my_stderr = NULL;
    char *my_stdout = NULL;
    int   signal;
    boolean contains_unhandled;

    captureOutput(do_exceptions_3, NULL, &my_stdout, &my_stderr, &signal);
    contains_unhandled = contains(my_stderr, "unhandled exception 99");

    free(my_stdout);
    free(my_stderr);
    FREEMEMWITHCHECK;

    if (signal != SIGTERM) {
	fail("Expected SIGTERM, got %d\n", signal);
    }
    
    fail_unless(contains_unhandled);
}
END_TEST

// Test for nested exceptions: handled
START_TEST(exceptions_4)
{
    int a;
    a = catcher(30, raiser2);
    fail_unless(a == 30, "Function result 30 expected, got %d\n", a);
    fail_unless(last_exception == 0, 
		"Exception 0 expected, got %d\n", last_exception);
    fail_unless(handled_by == 1, 
		"Unexpected exception handling %d\n", handled_by);
    FREEMEMWITHCHECK;
}
END_TEST

// Test for nested exceptions: re-raised
START_TEST(exceptions_5)
{
    int a;
    a = catcher(31, raiser2);
    fail_unless(a == 31, "Function result 31 expected, got %d\n", a);
    fail_unless(last_exception == 31, 
		"Exception 31 expected, got %d\n", last_exception);
    fail_unless(handled_by == 34, 
		"Unexpected exception handling %d\n", handled_by);
    FREEMEMWITHCHECK;
}
END_TEST

// Test for nested exceptions: handled in when_others
START_TEST(exceptions_6)
{
    int a;
    a = catcher(32, raiser2);
    fail_unless(a == 0, "Function result 0 expected, got %d\n", a);
    fail_unless(last_exception == 0, 
		"Exception 0 expected, got %d\n", last_exception);
    fail_unless(handled_by == 4, 
		"Unexpected exception handling %d\n", handled_by);
    FREEMEMWITHCHECK;
}
END_TEST

// Ensure that return from within a begin section does not mess up the
// exception stack.
START_TEST(exceptions_7)
{
    int a;
    a = catcher(98, raiser3);
    fail_unless(a == 98, "Function result 98 expected, got %d\n", a);
    fail_unless(last_exception == 98, 
		"Exception 98 expected, got %d\n", last_exception);
    fail_unless(handled_by == 8, 
		"Unexpected exception handling %d\n", handled_by);
    FREEMEMWITHCHECK;
}
END_TEST

// Test use of FINALLY with a WHEN clause raising an exception
START_TEST(exceptions_when_finally)
{
    int a = 0;
    String *str = stringNew("TEST_STR");

    BEGIN {
	BEGIN {
	    a = raiser1(LIST_ERROR);
	}
	EXCEPTION(ex) {
	    WHEN(LIST_ERROR) {
		a = raiser1(TYPE_MISMATCH);
	    }
	}
	FINALLY {
	    objectFree((Object *) str, TRUE);
	}
	END;
    }
    EXCEPTION(ex) {
	WHEN_OTHERS {
	    a = 98;
	}
    }
    END;

    objectFree((Object *) str, TRUE);
    fail_unless(a == 98, "Function result 98 expected, got %d\n", a);
    FREEMEMWITHCHECK;
}
END_TEST

// Test use of FINALLY with a WHEN clause handling an exception
START_TEST(exceptions_when_finally2)
{
    int a = 0;
    String *str = stringNew("TEST_STR");

    BEGIN {
	BEGIN {
	    a = raiser1(LIST_ERROR);
	}
	EXCEPTION(ex) {
	    WHEN(LIST_ERROR) {
		//dbgSexp(ex);
	    }
	}
	FINALLY {
	    objectFree((Object *) str, TRUE);
	}
	END;
    }
    EXCEPTION(ex) {
	WHEN_OTHERS {
	    a = 98;
	}
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST


Suite *
exceptions_suite(void)
{
    Suite *s = suite_create("Exceptions");

    /* Core test case */
    TCase *tc_core = tcase_create("ECore");
    ADD_TEST(tc_core, exceptions_1);
    ADD_TEST(tc_core, exceptions_2);
    ADD_TEST(tc_core, exceptions_3);
    ADD_TEST(tc_core, exceptions_4);
    ADD_TEST(tc_core, exceptions_5);
    ADD_TEST(tc_core, exceptions_6);
    ADD_TEST(tc_core, exceptions_7);
    ADD_TEST(tc_core, exceptions_new1);
    ADD_TEST(tc_core, exceptions_new2);
    ADD_TEST(tc_core, exceptions_new3);
    ADD_TEST(tc_core, exceptions_new4);
    ADD_TEST(tc_core, exceptions_new5);
    ADD_TEST(tc_core, exceptions_new6);
    ADD_TEST(tc_core, exceptions_new7);
    ADD_TEST(tc_core, exceptions_new8);
    ADD_TEST(tc_core, exceptions_new9);
    ADD_TEST(tc_core, exceptions_new10);
    ADD_TEST(tc_core, exceptions_new11);
    ADD_TEST(tc_core, exceptions_when_finally);
    ADD_TEST(tc_core, exceptions_when_finally2);
    ADD_TEST(tc_core, exceptions_new12);
    ADD_TEST(tc_core, exceptions_new13);
    ADD_TEST(tc_core, exceptions_new14);
    ADD_TEST(tc_core, exceptions_new15);
    suite_add_tcase(s, tc_core);

    return s;
}

