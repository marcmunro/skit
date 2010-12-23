/**
 * @file   check_params.c
 * \code
 *     Copyright (c) 2009, 2010 Marc Munro
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
#include <string.h>
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
	    //printSexp(stderr, "PARAMS", params);
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
	//printSexp(stderr, "PARAMS", params);
	executeAction(action, params);
    }
    finalAction();
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
	doc = docStackPop();
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
	doc = docStackPop();
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

    doc = docStackPop();
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
	param_list = process_args(3, args);
	fail("Missing file not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail_unless(contains(ex->text, "Cannot find file"),
		    "Missing file not detected(2)");
    }
    END;

    doc = docStackPop();
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

START_TEST(incomplete_extract)
{
    char *args[] = {"./skit", "--extract"};
    Document *doc;
    Cons *param_list;
    Hash *param_hash;
    String *action_key = stringNew("action");
    String *action;

    initBuiltInSymbols();
    initTemplatePath(".");
    param_list = process_args(2, args);
    param_hash = (Hash *) param_list->car;
    action = (String *) hashGet(param_hash, (Object *) action_key);

    BEGIN {
	executeAction(action, param_hash);

	fail("Incomplete extract params not detected(1)");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) param_list, FALSE);
	objectFree((Object *) action_key, TRUE);
	fail_unless(contains(ex->text, "No database connection defined"),
		    "Incomplete extract params not detected(2)");
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
	fail_unless(grants->svalue != NULL, "Incorrect grants value");
	objectFree((Object *) key, TRUE);
	

	objectFree((Object *) param_list, TRUE);
	doc = docStackPop();
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

START_TEST(print)
{
    char *args[] = {"./skit", "--print", "test.xml"};
    Document *doc;

    initBuiltInSymbols();
    initTemplatePath(".");

    BEGIN {
	process_args2(3, args);
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

START_TEST(dbtype)
{
    char *args[] = {"./skit", "--dbtype", "postgres"};
    Document *doc;

    initBuiltInSymbols();
    initTemplatePath(".");

    BEGIN {
	process_args2(3, args);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fail("dbtype: failed to set dbtype");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(dbtype_unknown)
{
    char *args[] = {"./skit", "--dbtype", "wibble"};
    Document *doc;

    initBuiltInSymbols();
    initTemplatePath(".");

    BEGIN {
	process_args2(3, args);
	fail("dbtype_unknown: Invalid dbtype not detected(1)");
    }
    EXCEPTION(ex);
    WHEN(PARAMETER_ERROR) {
	fail_unless(contains(ex->text, "dbtype \"wibble\" is not known"),
		    "dbtype_unknown: Invalid dbtype not detected(2)");
    }
    WHEN_OTHERS {
	fail("dbtype_unknown: Invalid dbtype not detected(3)");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(connect)
{
    char *args[] = {"./skit", "--connect", "--dbtype=pgtest", 
		    "dbname = 'skittest' port = '54329'"};
    Symbol *sym;
    char *tmp;
    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();

    BEGIN {
	process_args2(4, args);
	sym = symbolGet("connect");
	fail_unless(sym && sym->svalue,
		    tmp = newstr("connect: connect variable is not defined"));
	skfree(tmp);
    }
    EXCEPTION(ex); 
    WHEN_OTHERS {
	fail(newstr("connect: exception - %s", ex->text));
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(connect2)
{
    char *args[] = {"./skit", "--connect", "--dbtype=postgres", 
		    "dbname='skittest' port = '5432'"};
    Symbol *sym;
    char *tmp;
    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();

    BEGIN {
	process_args2(4, args);
	sym = symbolGet("connect");
	dbgSexp(sym->svalue);
	fail_unless(sym && sym->svalue,
		    tmp = newstr("connect: connect variable is not defined"));
	skfree(tmp);
    }
    EXCEPTION(ex); 
    WHEN_OTHERS {
	fail(newstr("connect: exception - %s", ex->text));
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

typedef struct fileinfo_t {
    fpos_t pos;
    int fd;
} fileinfo_t;    


static fileinfo_t *
redirect(char *destination)
{
    fileinfo_t *fileinfo = skalloc(sizeof(fileinfo_t));
    memset(fileinfo, 0, sizeof(fileinfo_t));
    fgetpos(stdout, &(fileinfo->pos));
    fileinfo->fd = dup(fileno(stdout));
    freopen(destination, "w", stdout);
    return fileinfo;
}

static void
resetdirect(fileinfo_t *fileinfo)
{
    fflush(stdout);
    dup2(fileinfo->fd, fileno(stdout));
    close(fileinfo->fd);
    clearerr(stdout);
    fsetpos(stdout, &(fileinfo->pos));
    skfree(fileinfo);
}


START_TEST(extract)
{
    char *args[] = {"./skit", "-t", "extract.xml", "--dbtype=pgtest", 
    //char *args[] = {"./skit", "-t", "extract.xml", "--dbtype=postgres", 
		    "--connect", 
		    "dbname = 'skittest' port = '54329'",
                    "--print", "--full"};
    //"--list", "-g", "--print", "--full"};
    Document *doc;
    char *bt;
    fileinfo_t *fi;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();
    //showFree(1205);
    //showMalloc(4635);

    BEGIN {
	fi = redirect("/dev/null");
	process_args2(8, args);
	resetdirect(fi);
	//process_args2(10, args);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(print2)
{
    char *args[] = {"./skit", "--print", "--full", 
		    "xx"};
    //"--list", "-g", "--print", "--full"};
    Document *doc;
    char *bt;
    fileinfo_t *fi;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();
    //showFree(1205);
    //showMalloc(306010);
    //trackMalloc(138807);

    BEGIN {
	//fi = redirect("/dev/null");
	process_args2(4, args);
	//resetdirect(fi);
	//process_args2(10, args);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//resetdirect(fi);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "BACKTRACE:%s\n", ex->backtrace);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(gather)
{
    char *args[] = {"./skit", "--list", 
		    "./dbdump/cluster.xml"};
    //"--list", "-g", "--print", "--full"};
    Document *doc;
    char *bt;
    fileinfo_t *fi;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();
    //showFree(1205);
    //showMalloc(1691);
    //trackMalloc(4635);

    BEGIN {
	//fi = redirect("/dev/null");
	process_args2(3, args);
	//resetdirect(fi);
	//process_args2(10, args);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//resetdirect(fi);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "BACKTRACE:%s\n", ex->backtrace);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(scatter)
{
    char *args[] = {"./skit", "-t", "scatter.xml", 
		    "xx", "--path", "./dbdump", "--checkonly"};
    //"--list", "-g", "--print", "--full"};
    Document *doc;
    char *bt;
    fileinfo_t *fi;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();
    //showFree(1205);
    //showMalloc(10583);
    //trackMalloc(4635);

    BEGIN {
	//fi = redirect("/dev/null");
	process_args2(7, args);
	//resetdirect(fi);
	//process_args2(10, args);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//resetdirect(fi);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "BACKTRACE:%s\n", ex->backtrace);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(extract2)
{
    char *args[] = {"./skit", "-t", "extract.xml", "--dbtype=postgres", 
		    "--connect", 
		    "dbname = 'skittest' port = " PGPORT "host = " PGHOST,
                    "--print", "--full"};
    //"--list", "-g", "--print", "--full"};
    Document *doc;
    char *bt;
    fileinfo_t *fi;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();
    //showFree(1205);
    //showMalloc(4635);
    //trackMalloc(138807);

    BEGIN {
	//fi = redirect("/dev/null");
	process_args2(8, args);
	//resetdirect(fi);
	//process_args2(10, args);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//resetdirect(fi);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "BACKTRACE:%s\n", ex->backtrace);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(generate2)
{
    char *args[] = {"./skit", "-t", "extract.xml", "--dbtype=postgres", 
		    "--connect", 
		    "dbname = 'regressdb' port = " PGPORT "host = " PGHOST,
                    "--generate", "--build",  "--print", "--full"};
    //"--list", "-g", "--print", "--full"};
    Document *doc;
    char *bt;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();
    //showFree(1205);
    //showMalloc(299978);

    BEGIN {
	process_args2(8, args);
	//process_args2(10, args);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(diff)
{
    char *args[] = {"./skit", "-t", "diff.xml", "zz", 
		    "zz3",  "--print", "--full"};
    //"--list", "-g", "--print", "--full"};
    Document *doc;
    char *bt;

    initBuiltInSymbols();
    initTemplatePath(".");
    //registerTestSQL();
    //showFree(1205);
    //showMalloc(299978);

    BEGIN {
	process_args2(7, args);
	//process_args2(10, args);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	//RAISE();
	//fail("extract fails with exception");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(generate3)
{
    char *args[] = {"./skit", "--generate", "--build", 
		    "x", "--print", "--full"};
    Document *doc;
    char *bt;

    initBuiltInSymbols();
    initTemplatePath(".");
    showMalloc(97969);
    showFree(114869);

    BEGIN {
	process_args2(4, args);
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

START_TEST(list)
{
    char *args[] = {"./skit", "--list", 
		    "zz3", "--print", "--full"};
    Document *doc;
    char *bt;

    initBuiltInSymbols();
    initTemplatePath(".");
    //showFree(2826);
    //showMalloc(4611);

    BEGIN {
	process_args2(4, args);
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

START_TEST(deps)
{
    char *args[] = {"./skit", "--adddeps",
		    "zz3", "--print", "--full"};
    Document *doc;
    char *bt;

    initBuiltInSymbols();
    initTemplatePath(".");
    showFree(4247);
    //showMalloc(1909);

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

START_TEST(generate)
{
    char *args[] = {"./skit", "--extract", "--dbtype=pgtest", 
		    "--connect", 
		    "dbname = 'skittest' port = '54329'",
                    "--generate", "--drop", "--build", "--print", "--full"};
    Document *doc;
    char *bt;
    fileinfo_t *fi;

    initBuiltInSymbols();
    initTemplatePath(".");
    registerTestSQL();
    //showFree(1205);
    //showMalloc(1986);

    BEGIN {
	fi = redirect("/dev/null");
	process_args2(8, args);
	resetdirect(fi);
	//doc = docStackPop();
	//printSexp(stderr, "DOC:", (Object *) doc);
	//objectFree((Object *) doc, TRUE);
	//fail("extract done!");
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
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
    ADD_TEST(tc_core, incomplete_extract);
    ADD_TEST(tc_core, missing_template);
    ADD_TEST(tc_core, multiple_options);
    ADD_TEST(tc_core, unknown_type);
    ADD_TEST(tc_core, no_option_name);
    ADD_TEST(tc_core, no_alias_name);
    ADD_TEST(tc_core, value_and_default);
    ADD_TEST(tc_core, option_usage);  
    //ADD_TEST(tc_core, extract);
    //ADD_TEST(tc_core, generate);
    //ADD_TEST(tc_core, extract2);  // Used to avoid running regression tests
    //ADD_TEST(tc_core, generate2); // during development of new db objects
    //ADD_TEST(tc_core, diff);
    ADD_TEST(tc_core, scatter);
    ADD_TEST(tc_core, gather);
    ADD_TEST(tc_core, print2);
    ADD_TEST(tc_core, generate3);
    ADD_TEST(tc_core, list);
    ADD_TEST(tc_core, deps);
    ADD_TEST(tc_core, dbtype);
    ADD_TEST(tc_core, dbtype_unknown);
    ADD_TEST(tc_core, connect);
    //ADD_TEST(tc_core, print);
				
    suite_add_tcase(s, tc_core);

    return s;
}


