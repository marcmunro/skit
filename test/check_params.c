/**
 * @file   check_params.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Unit tests for processing command line parameters
 */


#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <regex.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"

void
myfail(char *str)
{
    fprintf(stderr, "FAIL: %s\n", str);
    raise(SIGABRT);
}

boolean
contains(char *src, char *expr)
{
    regex_t regex;
    int errcode;
    boolean result;
    if (errcode = regcomp(&regex, expr, REG_EXTENDED)) {
	// Something went wrong with the regexp compilation
	char errmsg[200];
	(void) regerror(errcode, &regex, errmsg, 199);
	myfail(newstr("contains: regexp compilation failure: %s", errmsg));
    }
    
    result = regexec(&regex, src, 0, NULL, 0) == 0;
    regfree(&regex);
    return result;
}

static char *
errdetails(char *bufname, char *buffer, char *expr, char *errstr,
    char *not)
{
    if (errstr) {
	return newstr("%s\n<<<%s>>>\n", errstr, buffer);
    } else {
	return newstr("/%s/ %sfound in %s\n<<<%s>>>\n", 
		      expr, not, bufname, buffer);
    }
}

void
fail_if_contains(char *bufname, char *buffer, char *expr, char *errstr)
{
    if (contains(buffer, expr)) {
	fprintf(stderr, "ABOUT TO FAIL\n");
	myfail(errdetails(bufname, buffer, expr, errstr, ""));
    }
}

void
fail_unless_contains(char *bufname, char *buffer, char *expr, char *errstr)
{
    if (!contains(buffer, expr)) {
	fprintf(stderr, "ABOUT TO FAIL\n");
	myfail(errdetails(bufname, buffer, expr, errstr, "not "));
    }

}

START_TEST(usage)
{
    // Test that a string providing an option name is correctly 
    // converted into a list of keys for that option.
    char *usage = usage_msg();

    fail_unless(contains(usage, "add.deps"),
		"No mention of adddeps");
    FREEMEMWITHCHECK;
}
END_TEST

Cons *
process_args(int argc,
	     char *argv[])
{
    Hash *params;
    Cons *cons = NULL;
    String *action;
    record_args(argc, argv);
    BEGIN {
	while (action = nextAction()) {
	    params = parseAction(action);
	    cons = consNew((Object *) params, (Object *) cons);
	}
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) cons, TRUE);
	RAISE();
    }
    END;

    return cons;
}

void
process_args2(int argc,
	     char *argv[])
{
    Hash *params;
    String *action;
    record_args(argc, argv);
    while (action = nextAction()) {
	params = parseAction(action);
	executeAction(action, params);
    }
}



START_TEST(adddeps_options)
{
    char *args[] = {"./skit", "-a", "test/testfiles/x.xml"};
    Document *doc;
    Cons *param_list;
    Hash *params; 
    String *key;
    String *str;
    Int4 *sources;
    Int4 *wibble;

    initBuiltInSymbols();
    initTemplatePath("./test");
    BEGIN {
	param_list = process_args(3, args);
	params = (Hash *) param_list->car;

	key = stringNew("sources");
	sources = (Int4 *) hashGet(params, (Object *) key);
	fail_if(sources == NULL, "Sources not defined");
	fail_unless(sources->type == OBJ_INT4, "Incorrect sources type");
	fail_unless(sources->value == 1, "Incorrect sources value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("action");
	str = (String *) hashGet(params, (Object *) key);
	fail_if(str == NULL, "Action not defined");
	fail_unless(str->type == OBJ_STRING, "Incorrect action type");
	fail_unless(streq(str->value, "adddeps"), "Incorrect action value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("template_name");
	str = (String *) hashGet(params, (Object *) key);
	fail_if(str == NULL, "Template name not defined");
	fail_unless(str->type == OBJ_STRING, "Incorrect template name type");
	fail_unless(streq(str->value, "./test/templates/add_deps.xml"), 
		    "Incorrect template name value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("wibble");
	wibble = (Int4 *) hashGet(params, (Object *) key);
	fail_if(wibble == NULL, "Wibble not defined");
	fail_unless(wibble->type == OBJ_INT4, "Incorrect wibble type");
	fail_unless(wibble->value == 3, "Incorrect wibble value");
	objectFree((Object *) key, TRUE);
	
	
	objectFree((Object *) param_list, TRUE);
	doc = (Document *) actionStackPop();
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail("Unexpected exception: %s", ex->text);
    }
    END;
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(template_options)
{
    char *args[] = {"./skit", "-t", "add_deps.xml", "test/testfiles/x.xml"};
    Document *doc;
    Cons *param_list;
    Hash *params; 
    String *key;
    String *str;
    Int4 *sources;
    Int4 *wibble;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);
	params = (Hash *) param_list->car;

	key = stringNew("sources");
	sources = (Int4 *) hashGet(params, (Object *) key);
	fail_if(sources == NULL, "Sources not defined");
	fail_unless(sources->type == OBJ_INT4, "Incorrect sources type");
	fail_unless(sources->value == 1, "Incorrect sources value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("action");
	str = (String *) hashGet(params, (Object *) key);
	fail_if(str == NULL, "Action not defined");
	fail_unless(str->type == OBJ_STRING, "Incorrect action type");
	fail_unless(streq(str->value, "template"), "Incorrect action value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("template_name");
	str = (String *) hashGet(params, (Object *) key);
	fail_if(str == NULL, "Template name not defined");
	fail_unless(str->type == OBJ_STRING, "Incorrect template name type");
	fail_unless(streq(str->value, "./test/templates/add_deps.xml"), 
		    "Incorrect template name value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("wibble");
	wibble = (Int4 *) hashGet(params, (Object *) key);
	fail_if(wibble == NULL, "Wibble not defined");
	fail_unless(wibble->type == OBJ_INT4, "Incorrect wibble type");
	fail_unless(wibble->value == 3, "Incorrect wibble value");
	objectFree((Object *) key, TRUE);
	
	
	objectFree((Object *) param_list, TRUE);
	doc = (Document *) actionStackPop();
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail("Unexpected exception: %s", ex->text);
    }
    END;
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(too_many_sources)
{
    char *args[] = {"./skit", "-a", "test/testfiles/x.xml", "y.xml"};
    Document *doc;
    Cons *param_list;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);
	fail("Too many source files not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "too many source files"),
		    "Too many source files not detected(2)");
    }
    END;

    doc = (Document *) actionStackPop();
    objectFree((Object *) doc, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(missing_file)
{
    char *args[] = {"./skit", "-a", "missing.xml"};
    Document *doc;
    Cons *param_list;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);
	fail("Missing file not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "Cannot find file"),
		    "Missing file not detected(2)");
    }
    END;

    doc = (Document *) actionStackPop();
    objectFree((Object *) doc, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(too_few_sources)
{
    char *args[] = {"./skit", "-a"};
    Document *doc;
    Cons *param_list;
    Hash *param_hash;
    String *action_key = stringNew("action");
    String *action;

    initBuiltInSymbols();
    initTemplatePath("./test");
    param_list = process_args(2, args);
    param_hash = (Hash *) param_list->car;
    action = (String *) hashGet(param_hash, (Object *) action_key);

    BEGIN {
	executeAction(action, param_hash);

	fail("Too few sources not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) param_list, FALSE);
	objectFree((Object *) action_key, TRUE);
	fail_unless(contains(ex->text, "Insufficient inputs"),
		    "Too few sources not detected(2)");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(missing_template)
{
    char *args[] = {"./skit", "-t", "missing.xml"};
    Document *doc;
    Cons *param_list;

    initBuiltInSymbols();
    initTemplatePath("./tests");

    BEGIN {
	param_list = process_args(3, args);

	fail("Missing template not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "template missing.xml not found"),
		    "Missing template not detected(2)");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(multiple_options)
{
    char *args[] = {"./skit", "-t", "multiple.xml", "test/testfiles/x.xml"};
    Document *doc;
    Cons *param_list;
    Hash *params; 
    String *key;
    String *str;
    Int4 *sources;
    Symbol *grants;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);
	params = (Hash *) param_list->car;
	
	key = stringNew("sources");
	sources = (Int4 *) hashGet(params, (Object *) key);
	fail_if(sources == NULL, "Sources not defined");
	fail_unless(sources->type == OBJ_INT4, "Incorrect sources type");
	fail_unless(sources->value == 1, "Incorrect sources value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("action");
	str = (String *) hashGet(params, (Object *) key);
	fail_if(str == NULL, "Action not defined");
	fail_unless(str->type == OBJ_STRING, "Incorrect action type");
	
	fail_unless(streq(str->value, "template"), "Incorrect action value");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("template_name");
	str = (String *) hashGet(params, (Object *) key);
	fail_if(str == NULL, "Template name not defined");
	fail_unless(str->type == OBJ_STRING, "Incorrect template name type");
	objectFree((Object *) key, TRUE);
	
	key = stringNew("grants");
	grants = (Symbol *) hashGet(params, (Object *) key);
	fail_if(grants == NULL, "grants not defined");
	fail_unless(grants->type == OBJ_SYMBOL, "Incorrect grants type");
	fail_unless(grants->value != NULL, "Incorrect grants value");
	objectFree((Object *) key, TRUE);
	

	objectFree((Object *) param_list, TRUE);
	doc = (Document *) actionStackPop();
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "Unexpected type wibble"),
		    "Invalid type not detected(2)");
    }
    END;
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(unknown_type)
{
    char *args[] = {"./skit", "-t", "unknowntype.xml", "x.xml"};
    Document *doc;
    Cons *param_list = NULL;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);

	fail("Invalid type not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "Unexpected type wibble"),
		    "Invalid type not detected(2)");
    }
    END;

    objectFree((Object *) param_list, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(no_option_name)
{
    char *args[] = {"./skit", "-t", "noname.xml", "x.xml"};
    Document *doc;
    Cons *param_list = NULL;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);

	fail("Missing option name not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "Option has no name"),
		    "Missing option name not detected(2)");
    }
    END;

    objectFree((Object *) param_list, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(no_alias_name)
{
    char *args[] = {"./skit", "-t", "noname2.xml", "x.xml"};
    Document *doc;
    Cons *param_list = NULL;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);

	fail("Missing alias name not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "Alias has no name"),
		    "Missing alias name not detected(2)");
    }
    END;

    objectFree((Object *) param_list, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(value_and_default)
{
    char *args[] = {"./skit", "-t", "valandd.xml", "x.xml"};
    Document *doc;
    Cons *param_list = NULL;

    initBuiltInSymbols();
    initTemplatePath("./test");

    BEGIN {
	param_list = process_args(4, args);

	fail("Option with value and default not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "Must provide value \\*or\\* default"),
		    "Option with value and default not detected(2)");
    }
    END;

    objectFree((Object *) param_list, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST


START_TEST(option_usage)
{
    char *out;
    redirect_stdout("option_usage2");
    BEGIN {
	show_usage(stdout);
	out = readfrom_stdout();
	fail_unless_contains("stdout", out, "--add\\[deps\\]", NULL);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail("Unexpected exception: %s", ex->text);
    }
    END;
    skfree(out);
    end_redirects();
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

START_TEST(extract)
{
    char *args[] = {"./skit", "-t", "extract.xml", "--dbtype=pgtest", 
    //char *args[] = {"./skit", "-t", "extract.xml", "--dbtype=postgres", 
		    "--connect", 
		    "dbname = 'skittest' port = '54329'"};
    Document *doc;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();

    BEGIN {
	process_args2(6, args);
	doc = (Document *) actionStackPop();
	printSexp(stderr, "DOC:", (Object *) doc);
	objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

Suite *
params_suite(void)
{
    Suite *s = suite_create("Params");

    /* Core test case */
    TCase *tc_core = tcase_create("Params");
    ADD_TEST(tc_core, usage);
    ADD_TEST(tc_core, adddeps_options);
    ADD_TEST(tc_core, template_options);
    ADD_TEST(tc_core, missing_file);
    ADD_TEST(tc_core, too_many_sources);
    ADD_TEST(tc_core, too_few_sources);
    ADD_TEST(tc_core, missing_template);
    ADD_TEST(tc_core, multiple_options);
    ADD_TEST(tc_core, unknown_type);
    ADD_TEST(tc_core, no_option_name);
    ADD_TEST(tc_core, no_alias_name);
    ADD_TEST(tc_core, value_and_default);
    ADD_TEST(tc_core, option_usage);  
    ADD_TEST(tc_core, extract);
				
    suite_add_tcase(s, tc_core);

    return s;
}


