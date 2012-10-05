/**
 * @file   check_tsort.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Unit tests for checking dependency sorting functions
 */


#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <regex.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"

static char *
test_build_order(Vector *results, char *list_str)
{
    Object *tmp;
    String *name;
    DagNode *node;
    Cons *list = (Cons *) objectFromStr(list_str);
    char *result;
    assert(list,
	   "check_build_order: failed to build list");
    assert(list->type == OBJ_CONS,
	   "check_build_order: parameter must be a list");
    assert(list->car,
	   "check_build_order: parameter must have contents");

    int i = 0;
    while (list) {
	name = (String *) list->car;
	assert(name->type == OBJ_STRING,
	       "check_build_order: list item is not a string");

	while (i < results->elems) {
	    node = (DagNode *) results->contents->vector[i];
	    if (streq(node->fqn->value, name->value)) {
		/* We have a match, go on to the next name */
		break;
	    }
	    i++;
	    if (i >= results->elems) {
		/* The last name in the list was not found. */
		result = newstr("Error: %s is out of order in %s", 
				name->value, list_str);
		objectFree((Object *) list, TRUE);
		return result;
	    }
	}

	tmp = (Object *) list;
	list = (Cons *) list->cdr;
	objectFree((Object *) name, TRUE);
	objectFree(tmp, FALSE);
    }
    return NULL;
}

static void
check_build_order(Vector *results, char *list_str)
{
    char *err = test_build_order(results, list_str);
    if (err) {
	fail(err);
    }
}

static void
check_build_order_or(Vector *results, ...)
{
    va_list args;
    char *list;
    char *err;
    char *msg = NULL;
    char *prev;

    va_start(args, results);
    while (list = va_arg(args, char *)) {
	if (!(err = test_build_order(results, list))) {
	    if (msg) {
		skfree(msg);
		msg = NULL;
	    }
	    break;
	}
	if (msg) {
	    prev = msg;
	    msg = newstr("%s or %s", prev, err);
	    skfree(prev);
	    skfree(err);
	}
	else {
	    msg = err;
	}
	
    }
    va_end(args);
    if (msg) {
	prev = msg;
	msg = newstr("NO TEST PASSED: %s", prev);
	skfree(prev);
	fail(msg);
    }
}

START_TEST(check_gensort)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	//showMalloc(1104);
	//showFree(140);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	
	doc = getDoc("test/data/gensource1.xml");
	simple_sort = symbolNew("simple-sort");    
	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'dbincluster.cluster.skittest' "
		      "'database.cluster.skittest')");
	check_build_order(results, "('drop.role.cluster.keep' 'drop.cluster' "
		      "'cluster' 'role.cluster.keep')");
	check_build_order(results, "('drop.role.cluster.keep2' 'drop.cluster' "
		      "'cluster' 'role.cluster.keep2')");
	check_build_order(results, "('drop.role.cluster.lose' 'drop.cluster' "
		      "'cluster' 'role.cluster.lose')");
	check_build_order(results, "('drop.role.cluster.marc' 'drop.cluster' "
		      "'cluster' 'role.cluster.marc')");
	check_build_order(results, "('drop.role.cluster.marco' 'drop.cluster' "
		      "'cluster' 'role.cluster.marco')");
	check_build_order(results, "('drop.role.cluster.wibble' 'drop.cluster' "
		      "'cluster' 'role.cluster.wibble')");
	check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'role.cluster.lose' "
		      "'grant.cluster.lose.keep:keep')");
	check_build_order(results, "('role.cluster.keep' "
		      "'grant.cluster.lose.keep:keep')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'role.cluster.regress' "
		      "'tablespace.cluster.tbs2' "
	              "'grant.cluster.tbs2.create:keep2:regress')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'role.cluster.keep2' "
		      "'tablespace.cluster.tbs2' "
	              "'grant.cluster.tbs2.create:keep2:regress')");

	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST

START_TEST(check_gensort2)
{
    /* check_gensort test but using smart version of tsort */

    Document *doc;
    Vector *results;
    Object *ignore;
    char *tmp;
    int result;

    initBuiltInSymbols();
    initTemplatePath(".");
    ignore = evalSexp(tmp = newstr("(setq build t)"));
    objectFree(ignore, TRUE);
    skfree(tmp);
    //showMalloc(1104);
    ignore = evalSexp(tmp = newstr("(setq drop t)"));
    objectFree(ignore, TRUE);
    skfree(tmp);

    doc = getDoc("test/data/gensource1.xml");
    results = gensort(doc);
    //printSexp(stderr, "RESULTS: ", (Object *) results);

    check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'dbincluster.cluster.skittest' "
		      "'database.cluster.skittest')");
    check_build_order(results, "('drop.role.cluster.keep' 'drop.cluster' "
		      "'cluster' 'role.cluster.keep')");
    check_build_order(results, "('drop.role.cluster.keep2' 'drop.cluster' "
		      "'cluster' 'role.cluster.keep2')");
    check_build_order(results, "('drop.role.cluster.lose' 'drop.cluster' "
		      "'cluster' 'role.cluster.lose')");
    check_build_order(results, "('drop.role.cluster.marc' 'drop.cluster' "
		      "'cluster' 'role.cluster.marc')");
    check_build_order(results, "('drop.role.cluster.marco' 'drop.cluster' "
		      "'cluster' 'role.cluster.marco')");
    check_build_order(results, "('drop.role.cluster.wibble' 'drop.cluster' "
		      "'cluster' 'role.cluster.wibble')");
    check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'role.cluster.lose' "
		      "'grant.cluster.lose.keep:keep')");
    check_build_order(results, "('role.cluster.keep' "
		      "'grant.cluster.lose.keep:keep')");
    check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'role.cluster.regress' "
		      "'tablespace.cluster.tbs2' "
	              "'grant.cluster.tbs2.create:keep2:regress')");
    check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'role.cluster.keep2' "
		      "'tablespace.cluster.tbs2' "
	              "'grant.cluster.tbs2.create:keep2:regress')");

    objectFree((Object *) results, TRUE);
    objectFree((Object *) doc, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST


START_TEST(navigation)
{
    Document *src_doc;
    Document *result_doc;
    Vector *sorted;
    Object *ignore;
    char *tmp;
    int result;
    xmlNode *root;
    xmlDocPtr xmldoc;
    Symbol *simple_sort;
    //showMalloc(7040);
    //trackMalloc(7040);

    initBuiltInSymbols();
    initTemplatePath(".");
    ignore = evalSexp(tmp = newstr("(setq build t)"));
    objectFree(ignore, TRUE);
    skfree(tmp);
    ignore = evalSexp(tmp = newstr("(setq drop t)"));
    objectFree(ignore, TRUE);
    skfree(tmp);

    src_doc = getDoc("test/data/gensource1.xml");
    simple_sort = symbolNew("simple-sort");    
    sorted = gensort(src_doc);

    xmldoc = xmlNewDoc(BAD_CAST "1.0");
    root = xmlNewNode(NULL, BAD_CAST "root");
    xmlDocSetRootElement(xmldoc, root);
    result_doc = documentNew(xmldoc, NULL);

    treeFromVector(root, sorted);

    //dbgSexp(result_doc);

    objectFree((Object *) sorted, TRUE);
    objectFree((Object *) src_doc, TRUE);
    objectFree((Object *) result_doc, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(navigation2)
{
    /* As navigation test but using smart version of tsort */

    Document *src_doc;
    Document *result_doc;
    Vector *sorted;
    Object *ignore;
    char *tmp;
    int result;
    xmlNode *root;
    xmlDocPtr xmldoc;

    initBuiltInSymbols();
    initTemplatePath(".");
    ignore = evalSexp(tmp = newstr("(setq build t)"));
    objectFree(ignore, TRUE);
    skfree(tmp);
    ignore = evalSexp(tmp = newstr("(setq drop t)"));
    objectFree(ignore, TRUE);
    skfree(tmp);

    src_doc = getDoc("test/data/gensource1.xml");
    sorted = gensort(src_doc);

    xmldoc = xmlNewDoc(BAD_CAST "1.0");
    root = xmlNewNode(NULL, BAD_CAST "root");
    xmlDocSetRootElement(xmldoc, root);
    result_doc = documentNew(xmldoc, NULL);

    treeFromVector(root, sorted);

    // dbgSexp(result_doc);

    objectFree((Object *) sorted, TRUE);
    objectFree((Object *) src_doc, TRUE);
    objectFree((Object *) result_doc, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST


START_TEST(check_cyclic_gensort)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	//showMalloc(6795);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	
	doc = getDoc("test/data/gensource2.xml");
	simple_sort = symbolNew("simple-sort");    
	results = gensort(doc);
	//showVectorDeps(results);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'dbincluster.cluster.skittest' "
		      "'database.cluster.skittest')");
	check_build_order(results, "('drop.role.cluster.keep' "
			  "'role.cluster.keep')");
	check_build_order(results, "('drop.role.cluster.keep2' "
		      "'role.cluster.keep2')");
	check_build_order(results, "('drop.role.cluster.lose' "
		      "'role.cluster.lose')");
	check_build_order(results, "('drop.role.cluster.marc' "
		      "'role.cluster.marc')");
	check_build_order(results, "('drop.role.cluster.marco' "
		      "'role.cluster.marco')");
	check_build_order(results, "('drop.role.cluster.wibble' "
		      "'role.cluster.wibble')");
	check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'role.cluster.lose' "
		      "'grant.cluster.lose.keep:keep')");
	check_build_order(results, "('role.cluster.keep' "
		      "'grant.cluster.lose.keep:keep')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'role.cluster.regress' "
	              "'grant.cluster.tbs2.create:keep2:regress')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'tablespace.cluster.tbs2' "
	              "'grant.cluster.tbs2.create:keep2:regress')");

	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'rebuild.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v1' "
			     "'rebuild.viewbase.skittest.public.v2')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'rebuild.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'rebuild.viewbase.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'drop.view.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'drop.view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('rebuild.viewbase.skittest.public.v1' "
			     "'view.skittest.public.v1')",
			     "('rebuild.viewbase.skittest.public.v2' "
			      "'view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('rebuild.viewbase.skittest.public.v1' "
			     "'view.skittest.public.v2')",
			     "('rebuild.viewbase.skittest.public.v2' "
			     "'view.skittest.public.v1')",
			      NULL);

	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST

/* As check_cyclic_gensort but using smart sort */
START_TEST(check_cyclic_gensort2)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	//showMalloc(1104);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	
	doc = getDoc("test/data/gensource2.xml");
	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'dbincluster.cluster.skittest' "
		      "'database.cluster.skittest')");
	check_build_order(results, "('drop.role.cluster.keep' "
		      "'role.cluster.keep')");
	check_build_order(results, "('drop.role.cluster.keep2' "
		      "'role.cluster.keep2')");
	check_build_order(results, "('drop.role.cluster.lose' "
		      "'role.cluster.lose')");
	check_build_order(results, "('drop.role.cluster.marc' "
		      "'role.cluster.marc')");
	check_build_order(results, "('drop.role.cluster.marco' "
		      "'role.cluster.marco')");
	check_build_order(results, "('drop.role.cluster.wibble' "
		      "'role.cluster.wibble')");
	check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'role.cluster.lose' "
		      "'grant.cluster.lose.keep:keep')");
	check_build_order(results, "('role.cluster.keep' "
		      "'grant.cluster.lose.keep:keep')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'role.cluster.regress' "
		      "'tablespace.cluster.tbs2' "
	              "'grant.cluster.tbs2.create:keep2:regress')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'tablespace.cluster.tbs2' "
	              "'grant.cluster.tbs2.create:keep2:regress')");
	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'rebuild.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v1' "
			     "'rebuild.viewbase.skittest.public.v2')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'rebuild.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'rebuild.viewbase.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'drop.view.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'drop.view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('rebuild.viewbase.skittest.public.v1' "
			     "'view.skittest.public.v1')",
			     "('rebuild.viewbase.skittest.public.v2' "
			      "'view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('rebuild.viewbase.skittest.public.v1' "
			     "'view.skittest.public.v2')",
			     "('rebuild.viewbase.skittest.public.v2' "
			     "'view.skittest.public.v1')",
			      NULL);

	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST

START_TEST(check_cyclic_exception)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	//showMalloc(432);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	
	doc = getDoc("test/data/gensource3.xml");
	//dbgSexp(doc);
	results = gensort(doc);

	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fail("check_cyclic_exception: No exception raised!");
    }
    EXCEPTION(ex);
    WHEN(TSORT_CYCLIC_DEPENDENCY) {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("check_cyclic_exception: unexpected exception");
    }
}
END_TEST

START_TEST(diff)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(981);
	doc = getDoc("test/data/gensource_diff.xml");
	simple_sort = symbolNew("simple-sort");    
	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('diff.tablespace.cluster.tbs2'"
			  "'diff.role.cluster.regress')");

	check_build_order(results, "('drop.role.cluster.lose'"
			  "'exists.cluster')");

	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST

START_TEST(diff2)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(1104);
	doc = getDoc("test/data/gensource_diff.xml");
	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('diff.tablespace.cluster.tbs2'"
			  "'diff.role.cluster.regress')");

	check_build_order(results, "('drop.role.cluster.lose'"
			  "'exists.cluster')");

	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST

START_TEST(depset)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(531);
	//showFree(724);

	doc = getDoc("test/data/gensource_depset.xml");
	simple_sort = symbolNew("simple-sort");    
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);

	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);


	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST

START_TEST(depset2)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	doc = getDoc("test/data/gensource_depset.xml");
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);

	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);


	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST


START_TEST(depset_rebuild)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(531);
	//showFree(724);

	doc = getDoc("test/data/gensource_depset_rebuild.xml");
	simple_sort = symbolNew("simple-sort");    
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);

	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);


	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST

START_TEST(fallback)
{
    Document *volatile doc = NULL;
    Vector *volatile results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(531);
	//showFree(724);

	doc = getDoc("test/data/fallback.xml");
	simple_sort = symbolNew("simple-sort");    
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);

	results = gensort(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);


	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST


Suite *
tsort_suite(void)
{
    Suite *s = suite_create("tsort");
    TCase *tc_core = tcase_create("tsort");

    /* The tests with a 2 suffix use smart_tsort rather than standard
     * tsort */
    ADD_TEST(tc_core, check_gensort);
    ADD_TEST(tc_core, check_gensort2);
    ADD_TEST(tc_core, navigation);
    ADD_TEST(tc_core, navigation2);
    ADD_TEST(tc_core, check_cyclic_gensort);
    ADD_TEST(tc_core, check_cyclic_gensort2);
    ADD_TEST(tc_core, check_cyclic_exception);
    //ADD_TEST(tc_core, diff);
    //ADD_TEST(tc_core, diff2);
    ADD_TEST(tc_core, depset);
    ADD_TEST(tc_core, depset2);
    ADD_TEST(tc_core, depset_rebuild);
    ADD_TEST(tc_core, fallback);
				
    suite_add_tcase(s, tc_core);

    return s;
}

// TODO:
// Check why qname of dbobject for privs is not provided.  Maybe
// eliminate qnames that are not being used.

// check privs required for creation of all objects:
// schema
// table
// sequence
// operator
// operator class
// operator family
// conversion
// cast
// function
// view
// comments
