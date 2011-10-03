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

static Object *
showDeps(Object *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;
    int i;
    printSexp(stderr, "NODE: ", (Object *) node);
    if (node->dependencies) {
	for (i = 0; i < node->dependencies->elems; i++) {
	    printSexp(stderr, "   --> ", 
		      node->dependencies->contents->vector[i]);
	}
    }

    return (Object *) node;
}

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
		result = newstr("Error: %s is out of order", name->value);
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
    Document *doc = NULL;
    Vector *results = NULL;
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
	
	doc = getDoc("test/data/gensource1.xml");
	simple_sort = symbolNew("simple-sort");    
	results = gensort2(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'build.dbincluster.cluster.skittest' "
		      "'build.database.cluster.skittest')");
	check_build_order(results, "('drop.role.cluster.keep' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep')");
	check_build_order(results, "('drop.role.cluster.keep2' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep2')");
	check_build_order(results, "('drop.role.cluster.lose' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.lose')");
	check_build_order(results, "('drop.role.cluster.marc' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marc')");
	check_build_order(results, "('drop.role.cluster.marco' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marco')");
	check_build_order(results, "('drop.role.cluster.wibble' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.wibble')");
	check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'build.role.cluster.lose' "
		      "'build.grant.cluster.lose.keep:keep')");
	check_build_order(results, "('build.role.cluster.keep' "
		      "'build.grant.cluster.lose.keep:keep')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.regress' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.keep2' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");

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
    results = gensort2(doc);
    //printSexp(stderr, "RESULTS: ", (Object *) results);

    check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'build.dbincluster.cluster.skittest' "
		      "'build.database.cluster.skittest')");
    check_build_order(results, "('drop.role.cluster.keep' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep')");
    check_build_order(results, "('drop.role.cluster.keep2' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep2')");
    check_build_order(results, "('drop.role.cluster.lose' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.lose')");
    check_build_order(results, "('drop.role.cluster.marc' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marc')");
    check_build_order(results, "('drop.role.cluster.marco' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marco')");
    check_build_order(results, "('drop.role.cluster.wibble' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.wibble')");
    check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'build.role.cluster.lose' "
		      "'build.grant.cluster.lose.keep:keep')");
    check_build_order(results, "('build.role.cluster.keep' "
		      "'build.grant.cluster.lose.keep:keep')");
    check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.regress' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");
    check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.keep2' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");

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
    sorted = gensort2(src_doc);

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
    sorted = gensort2(src_doc);

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
    Document *doc = NULL;
    Vector *results = NULL;
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
	//owMalloc(3412);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	
	doc = getDoc("test/data/gensource2.xml");
	simple_sort = symbolNew("simple-sort");    
	results = gensort2(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'build.dbincluster.cluster.skittest' "
		      "'build.database.cluster.skittest')");
	check_build_order(results, "('drop.role.cluster.keep' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep')");
	check_build_order(results, "('drop.role.cluster.keep2' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep2')");
	check_build_order(results, "('drop.role.cluster.lose' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.lose')");
	check_build_order(results, "('drop.role.cluster.marc' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marc')");
	check_build_order(results, "('drop.role.cluster.marco' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marco')");
	check_build_order(results, "('drop.role.cluster.wibble' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.wibble')");
	check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'build.role.cluster.lose' "
		      "'build.grant.cluster.lose.keep:keep')");
	check_build_order(results, "('build.role.cluster.keep' "
		      "'build.grant.cluster.lose.keep:keep')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.regress' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.keep2' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");
	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'build.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v1' "
			     "'build.viewbase.skittest.public.v2')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'build.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'build.viewbase.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'drop.view.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'drop.view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('build.viewbase.skittest.public.v1' "
			     "'build.view.skittest.public.v1')",
			     "('build.viewbase.skittest.public.v2' "
			      "'build.view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('build.viewbase.skittest.public.v1' "
			     "'build.view.skittest.public.v2')",
			     "('build.viewbase.skittest.public.v2' "
			     "'build.view.skittest.public.v1')",
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
    Document *doc = NULL;
    Vector *results = NULL;
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
	results = gensort2(doc);
	//printSexp(stderr, "RESULTS: ", (Object *) results);

	check_build_order(results, "('drop.database.cluster.skittest' "
		      "'drop.dbincluster.cluster.skittest' "
		      "'build.dbincluster.cluster.skittest' "
		      "'build.database.cluster.skittest')");
	check_build_order(results, "('drop.role.cluster.keep' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep')");
	check_build_order(results, "('drop.role.cluster.keep2' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.keep2')");
	check_build_order(results, "('drop.role.cluster.lose' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.lose')");
	check_build_order(results, "('drop.role.cluster.marc' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marc')");
	check_build_order(results, "('drop.role.cluster.marco' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.marco')");
	check_build_order(results, "('drop.role.cluster.wibble' 'drop.cluster' "
		      "'build.cluster' 'build.role.cluster.wibble')");
	check_build_order(results, "('drop.grant.cluster.lose.keep:keep' "
		      "'build.role.cluster.lose' "
		      "'build.grant.cluster.lose.keep:keep')");
	check_build_order(results, "('build.role.cluster.keep' "
		      "'build.grant.cluster.lose.keep:keep')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.regress' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");
	check_build_order(results, 
		      "('drop.grant.cluster.tbs2.create:keep2:regress' "
		      "'drop.tablespace.cluster.tbs2' "
		      "'build.role.cluster.keep2' "
		      "'build.tablespace.cluster.tbs2' "
	              "'build.grant.cluster.tbs2.create:keep2:regress')");
	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'build.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v1' "
			     "'build.viewbase.skittest.public.v2')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'build.viewbase.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'build.viewbase.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('drop.viewbase.skittest.public.v1' "
			     "'drop.view.skittest.public.v1')",
			     "('drop.viewbase.skittest.public.v2' "
			     "'drop.view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('build.viewbase.skittest.public.v1' "
			     "'build.view.skittest.public.v1')",
			     "('build.viewbase.skittest.public.v2' "
			      "'build.view.skittest.public.v2')",
			     NULL);
	check_build_order_or(results, 
			     "('build.viewbase.skittest.public.v1' "
			     "'build.view.skittest.public.v2')",
			     "('build.viewbase.skittest.public.v2' "
			     "'build.view.skittest.public.v1')",
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
    Document *doc = NULL;
    Vector *results = NULL;
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
	
	doc = getDoc("test/data/gensource3.xml");
	results = gensort2(doc);

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
    Document *doc = NULL;
    Vector *results = NULL;
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
	simple_sort = symbolNew("simple-sort");    
	results = gensort2(doc);
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
    Document *doc = NULL;
    Vector *results = NULL;
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
	results = gensort2(doc);
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
    Document *doc = NULL;
    Vector *results = NULL;
    Object *ignore;
    char *tmp;
    int result;
    Symbol *simple_sort;
    boolean failed = FALSE;
    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	doc = getDoc("test/data/gensource_depset.xml");
	simple_sort = symbolNew("simple-sort");    
	ignore = evalSexp(tmp = newstr("(setq build t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);
	ignore = evalSexp(tmp = newstr("(setq drop t)"));
	objectFree(ignore, TRUE);
	skfree(tmp);

	results = gensort2(doc);
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
    Document *doc = NULL;
    Vector *results = NULL;
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

	results = gensort2(doc);
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
    ADD_TEST(tc_core, diff);
    ADD_TEST(tc_core, diff2);
    ADD_TEST(tc_core, depset);
    ADD_TEST(tc_core, depset2);
				
    suite_add_tcase(s, tc_core);

    return s;
}


