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
	    printSexp(stderr, "   --> ", node->dependencies->vector[i]);
	}
    }

    return (Object *) node;
}

static void
check_build_order(Vector *results, char *list_str)
{
    Object *tmp;
    String *name;
    DagNode *node;
    Cons *list = (Cons *) objectFromStr(list_str);
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
	    node = (DagNode *) results->vector[i];
	    if (streq(node->fqn->value, name->value)) {
		/* We have a match, go on to the next name */
		break;
	    }
	    i++;
	    if (i >= results->elems) {
		/* The last name in the list was not found. */
		fail(newstr("Error: %s is out of order", name->value));
	    }
	}

	tmp = (Object *) list;
	list = (Cons *) list->cdr;
	objectFree((Object *) name, TRUE);
	objectFree(tmp, FALSE);
    }
}


START_TEST(check_gensort)
{
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
    ignore = evalSexp(tmp = newstr("(setq drop t)"));
    objectFree(ignore, TRUE);
    skfree(tmp);

    doc = getDoc("test/data/gensource1.xml");
    results = gensort(doc);
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

Suite *
tsort_suite(void)
{
    Suite *s = suite_create("tsort");

    /* Core test case */
    TCase *tc_core = tcase_create("tsort");
    ADD_TEST(tc_core, check_gensort);
    ADD_TEST(tc_core, navigation);
				
    suite_add_tcase(s, tc_core);

    return s;
}


