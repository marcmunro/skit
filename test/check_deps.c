/**
 * @file   check_deps.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Unit tests for checking dependency management
 */


#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <regex.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"

static
void eval(char *str)
{
    Object *ignore;
    char *tmp;

    ignore = evalSexp(tmp = newstr(str));
    objectFree(ignore, TRUE);
    skfree(tmp);
}

static boolean
hasDependency(DagNode *node, DagNode *dep)
{
    Vector *vector = node->dependencies;
    Object *depobj;
    int i;
    if (vector) {
	for (i = 0; i < vector->elems; i++) {
	    depobj = vector->contents->vector[i];
	    if (depobj->type == OBJ_DAGNODE) {
		if (dep == (DagNode *) depobj) {
		    return TRUE;
		}
	    }
	    else if (depobj->type == OBJ_DEPENDENCY) {
		if (dep == ((Dependency *) depobj)->dependency) {
		    return TRUE;
		}
	    }
	}
    }
    return FALSE;
}

static boolean
hasDep(Hash *hash, char *from, char *to)
{
    String *key = stringNewByRef(newstr(from));
    DagNode *fnode = (DagNode *) hashGet(hash, (Object *) key);
    DagNode *tnode;
    if (!fnode) {
	fail("Cannot find source node %s ", key->value);
    }

    objectFree((Object *) key, TRUE);

    key = stringNewByRef(newstr(to));
    tnode = (DagNode *) hashGet(hash, (Object *) key);

    if (!tnode) {
	fail("Cannot find dependency %s ", key->value);
    }
    
    objectFree((Object *) key, TRUE);
    return hasDependency(fnode, tnode);
}

static void
requireDep(Hash *hash, char *from, char *to)
{
    if (!hasDep(hash, from, to)) {
	fail("No dep exists from %s to %s", from, to);
    }
}

static void
requireNoDep(Hash *hash, char *from, char *to)
{
    if (hasDep(hash, from, to)) {
	fail("Unwanted dep exists from %s to %s", from, to);
    }
}

static boolean
hasDeps(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *fromnode;
    String *key;
    int dep_elems;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	requireDep(hash, from, to);
	count++;
    }
    va_end(params);
    key = stringNewByRef(from);
    fromnode = (DagNode *) hashGet(hash, (Object *) key);

    if (!fromnode) {
	fail("Cannot find %s ", key->value);
    }
    objectFree((Object *) key, FALSE);
    
    dep_elems = (fromnode->dependencies)? fromnode->dependencies->elems: 0;

    return dep_elems == count;
}

static void
requireDeps(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *fromnode;
    String *key;
    int dep_elems;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	requireDep(hash, from, to);
	count++;
    }
    va_end(params);
    key = stringNewByRef(from);
    fromnode = (DagNode *) hashGet(hash, (Object *) key);

    if (!fromnode) {
	fail("Cannot find %s ", key->value);
    }
    objectFree((Object *) key, FALSE);
    
    dep_elems = (fromnode->dependencies)? fromnode->dependencies->elems: 0;
	
    if (dep_elems != count) {
	fail("Not all dependencies accounted for in %s (expecting %d got %d)", 
	     fromnode->fqn->value, count, fromnode->dependencies->elems);
    }
}

static void
requireOptionalDeps(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *fromnode;
    DagNode *tonode;
    DagNode *sub;
    String *key;
    int dep_elems = 0;
    Vector *required = vectorNew(10);
    int i;
    boolean abandoned = FALSE;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	vectorPush(required, (Object*) stringNew(to));
	count++;
    }
    va_end(params);
    key = stringNewByRef(from);
    fromnode = (DagNode *) hashGet(hash, (Object *) key);
    if (!fromnode) {
	fail("Cannot find %s ", key->value);
    }

    objectFree((Object *) key, FALSE);
    sub = fromnode->subnodes;

    while (sub) {
	dep_elems = (sub->dependencies)? sub->dependencies->elems: 0;
	abandoned = FALSE;
	EACH(required, i) {
	    key = (String *) ELEM(required, i);
	    tonode = (DagNode *) hashGet(hash, (Object *) key);
	    if (!hasDependency(sub, tonode)) {
		abandoned = TRUE;
		break;
	    }
	}
	if (!abandoned) {
	    /* Everything must have matched.  Yippee! */
	    break;
	}
	sub = sub->subnodes;
    }
    objectFree((Object *) required, TRUE);
	
    if (abandoned) {
	fail("Not all optional dependencies accounted for in %s "
	     "(expecting %d)", fromnode->fqn->value, count);
    }
}


static char *
xnodeName(char *basename, Vector *nodes)
{
    DagNode *node;
    int i;
    for (i = 0; i < nodes->elems; i++) {
	node = (DagNode *) nodes->contents->vector[i];
	if (strncmp(basename, node->fqn->value, strlen(basename)) == 0) {
	    return node->fqn->value;
	}
    }
    return NULL;
}


/* Trivially check the inBuildTypeBitSet function.
 */
START_TEST(build_type_bitsets)
{
    BuildTypeBitSet btbs = BUILD_NODE_BIT + DIFF_NODE_BIT;

    if (inBuildTypeBitSet(btbs, DROP_NODE)) {
	fail("Unexpected DROP_NODE found in bitset");
    }
    if (inBuildTypeBitSet(btbs, DEPART_NODE)) {
	fail("Unexpected DEPART_NODE found in bitset");
    }
    if (!inBuildTypeBitSet(btbs, BUILD_NODE)) {
	fail("Failed to find BUILD_NODE in bitset");
    }
    if (!inBuildTypeBitSet(btbs, DIFF_NODE)) {
	fail("Failed to find DIFF_NODE in bitset");
    }
}
END_TEST

/* Test basic dependencies, using dependency sets, with xnodes in place, */
START_TEST(depset_deps1)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	eval("(setq build t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = nodesFromDoc(doc);
	//showVectorDeps(nodes);
	nodes_by_fqn = hashByFqn(nodes);

	requireDeps(nodes_by_fqn, "role.cluster.r1", "cluster", 
		    "role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r2", "cluster", 
		    "role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r3", "cluster", NULL);

	requireOptionalDeps(nodes_by_fqn, "role.cluster.r3", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r4", NULL);

	requireOptionalDeps(nodes_by_fqn, "role.cluster.r4", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r5", NULL);

	requireDeps(nodes_by_fqn, "role.cluster.r5", "cluster", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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


/* Test basic build dependencies, using dependency sets, after
 * processing xnodes. */ 
START_TEST(depset_dag1_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	eval("(setq build t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = nodesFromDoc(doc);

	prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	nodes_by_fqn = hashByFqn(nodes);

	requireDeps(nodes_by_fqn, "cluster", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r1", "role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r2", "role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r3", "role.cluster.r4", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r5", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r4", "role.cluster.r5", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	dbgSexp(nodes);
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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

/* Test basic (inverted) drop dependencies, using dependency sets, after
 * processing xnodes. */ 
START_TEST(depset_dag1_drop)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	eval("(setq drop t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = nodesFromDoc(doc);

	prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	nodes_by_fqn = hashByFqn(nodes);

	requireDeps(nodes_by_fqn, "cluster", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r5", "role.cluster.r4", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r4", "role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r3", 
		    "role.cluster.r2", "role.cluster.r1", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r2", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r1", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	dbgSexp(nodes);
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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


/* Test dependency handling for rebuilds. */ 
START_TEST(depset_dag2_rebuild)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(572);

	eval("(setq build t)");
	doc = getDoc("test/data/gensource_depset_rebuild.xml");
	nodes = nodesFromDoc(doc);

	prepareDagForBuild((Vector **) &nodes);
	nodes_by_fqn = hashByFqn(nodes);
	//showVectorDeps(nodes);

	requireDeps(nodes_by_fqn, "cluster", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r5", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r4", "role.cluster.r5", 
		    "drop.role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r3", 
		    "role.cluster.r4", "drop.role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r1", 
		    "role.cluster.r3", "drop.role.cluster.r1", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r2", 
		    "role.cluster.r3", "drop.role.cluster.r2", NULL);
	requireDeps(nodes_by_fqn, "drop.role.cluster.r3", 
		    "drop.role.cluster.r1", "drop.role.cluster.r2", NULL);
	requireDeps(nodes_by_fqn, "drop.role.cluster.r1", NULL);
	requireDeps(nodes_by_fqn, "drop.role.cluster.r2", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "WHATWHATWHATWHATWHAT?\n");
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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

/* Test dependency handling for rebuilds, with a default action of drop. */ 
START_TEST(depset_dag2_rebuild2)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(813);
	//showFree(572);
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource_depset_rebuild.xml");
	nodes = nodesFromDoc(doc);

	prepareDagForBuild((Vector **) &nodes);
	nodes_by_fqn = hashByFqn(nodes);
	
	//showVectorDeps(nodes);

	requireDeps(nodes_by_fqn, "cluster", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r5", 
		    "drop.role.cluster.r5", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r4", 
		    "role.cluster.r5", "drop.role.cluster.r4", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r3", 
		    "role.cluster.r4", "drop.role.cluster.r3",
		    "role.cluster.r1", "role.cluster.r2", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r1", NULL);
	requireDeps(nodes_by_fqn, "role.cluster.r2", NULL);

	requireDeps(nodes_by_fqn, "drop.role.cluster.r3", 
		    "role.cluster.r1", "role.cluster.r2", NULL);
	requireDeps(nodes_by_fqn, "drop.role.cluster.r4", 
		    "drop.role.cluster.r3", NULL);
	requireDeps(nodes_by_fqn, "drop.role.cluster.r5", 
		    "drop.role.cluster.r4", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	dbgSexp(nodes);
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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

START_TEST(cyclic_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(813);
	//showFree(572);
	eval("(setq build t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = nodesFromDoc(doc);
	prepareDagForBuild((Vector **) &nodes);
	nodes_by_fqn = hashByFqn(nodes);
	//showVectorDeps(nodes);
	
	/* The following assumes that v1 will be the node that gets a
	 * cycle breaker.  This does not have to be the case, and 
	 * other sets of options should be allowed and checked for by
	 * these tests.  TODO: Add those extra tests, or consider other
	 * criteria for passing. */

	requireDeps(nodes_by_fqn, "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc", NULL);

	if (hasDeps(nodes_by_fqn, "view.skittest.public.v3", 
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc",
		    "privilege.cluster.marc.superuser", NULL) ||
	    hasDeps(nodes_by_fqn, "view.skittest.public.v3", 
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc",
		    "grant.skittest.public.usage:public:regress", NULL) ||
	    hasDeps(nodes_by_fqn, "view.skittest.public.v3", 
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc",
		    "grant.skittest.public.usage:public:regress", 
		    "privilege.cluster.marc.superuser", NULL)) 
	{
	    /* All is well is this case */
	}
	else {
	    fail("No complete dependency set found for %s",
		 "view.skittest.public.v3");
	}

	if (hasDeps(nodes_by_fqn, "view.skittest.public.v2", 
		    "view.skittest.public.v3",
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc", 
		    "privilege.cluster.marc.superuser", NULL) ||
	    hasDeps(nodes_by_fqn, "view.skittest.public.v2", 
		    "view.skittest.public.v3",
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc", 
		    "grant.skittest.public.usage:public:regress",
		    "privilege.cluster.marc.superuser", NULL) ||
	    hasDeps(nodes_by_fqn, "view.skittest.public.v2", 
		    "view.skittest.public.v3",
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc", 
		    "grant.skittest.public.usage:public:regress", NULL))
	{
	    /* All is well is this case */
	}
	else {
	    fail("No complete dependency set found for %s",
		 "view.skittest.public.v2");
	}

	if (hasDeps(nodes_by_fqn, "view.skittest.public.v1", 
		    "view.skittest.public.v2",
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc",
		    "privilege.cluster.marc.superuser", NULL) ||
	    hasDeps(nodes_by_fqn, "view.skittest.public.v1", 
		    "view.skittest.public.v2",
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc",
		    "grant.skittest.public.usage:public:regress",
		    "privilege.cluster.marc.superuser", NULL) ||
	    hasDeps(nodes_by_fqn, "view.skittest.public.v1", 
		    "view.skittest.public.v2",
		    "build.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc",
		    "grant.skittest.public.usage:public:regress", NULL))

	{
	    /* All is well is this case */
	}
	else {
	    fail("No complete dependency set found for %s",
		 "view.skittest.public.v1");
	}

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//dbgSexp(nodes);
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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


START_TEST(cyclic_drop)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(813);
	//showFree(572);
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = nodesFromDoc(doc);

	prepareDagForBuild((Vector **) &nodes);
	nodes_by_fqn = hashByFqn(nodes);
	
	//showVectorDeps(nodes);

	/* The following assumes that v1 will be the node that gets a
	 * cycle breaker.  This does not have to be the case, and 
	 * other sets of options should be allowed and checked for by
	 * these tests.  TODO: Add those extra tests */

	requireDeps(nodes_by_fqn, "drop.viewbase.skittest.public.v1", 
		    NULL);
	requireDeps(nodes_by_fqn, "view.skittest.public.v3", 
		    "drop.viewbase.skittest.public.v1", 
		    "view.skittest.public.v2", NULL);
	requireDeps(nodes_by_fqn, "view.skittest.public.v2", 
		    "drop.viewbase.skittest.public.v1", 
		    "view.skittest.public.v1", NULL);
	requireDeps(nodes_by_fqn, "view.skittest.public.v1", 
		    "drop.viewbase.skittest.public.v1", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//dbgSexp(nodes);
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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


START_TEST(cyclic_both)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(813);
	//showFree(572);
	eval("(setq drop t)");
	eval("(setq build t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = nodesFromDoc(doc);

	prepareDagForBuild((Vector **) &nodes);
	nodes_by_fqn = hashByFqn(nodes);
	//showVectorDeps(nodes);
	
	/* The following assumes that v1 will be the node that gets a
	 * cycle breaker.  This does not have to be the case, and 
	 * other sets of options should be allowed and checked for by
	 * these tests.  TODO: Add those extra tests */
	requireDeps(nodes_by_fqn, "rebuild.viewbase.skittest.public.v1", 
		    "schema.skittest.public", "role.cluster.marc",
		    "drop.viewbase.skittest.public.v1", NULL);
	requireDeps(nodes_by_fqn, "view.skittest.public.v3", 
		    "schema.skittest.public", "role.cluster.marc",
		    "rebuild.viewbase.skittest.public.v1", 
		    "privilege.cluster.marc.superuser",
		    "drop.view.skittest.public.v3", NULL);
	requireDeps(nodes_by_fqn, "view.skittest.public.v2", 
		    "schema.skittest.public", "view.skittest.public.v3",
		    "role.cluster.marc",
		    "privilege.cluster.marc.superuser",
		    "rebuild.viewbase.skittest.public.v1", 
		    "drop.view.skittest.public.v2", NULL);
	requireDeps(nodes_by_fqn, "view.skittest.public.v1", 
		    "schema.skittest.public", "view.skittest.public.v2",
		    "role.cluster.marc",
		    "privilege.cluster.marc.superuser",
		    "rebuild.viewbase.skittest.public.v1", 
		    "drop.view.skittest.public.v1", NULL);

	requireDeps(nodes_by_fqn, "drop.viewbase.skittest.public.v1", 
		    NULL);
	requireDeps(nodes_by_fqn, "drop.view.skittest.public.v3", 
		    "drop.viewbase.skittest.public.v1", 
		    "drop.view.skittest.public.v2", NULL);
	requireDeps(nodes_by_fqn, "drop.view.skittest.public.v2", 
		    "drop.view.skittest.public.v1", 
		    "drop.viewbase.skittest.public.v1", NULL);

	requireDeps(nodes_by_fqn, "drop.view.skittest.public.v1", 
		    "drop.viewbase.skittest.public.v1", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//dbgSexp(nodes);
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(642);
	//showFree(5641);
	eval("(setq build t)");
	doc = getDoc("test/data/gensource_fallback.xml");
	//doc = getDoc("tmp.xml");
	nodes = nodesFromDoc(doc);


	prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	nodes_by_fqn = hashByFqn(nodes);
	

	requireDeps(nodes_by_fqn, "fallback.grant.x.superuser", 
		    "role.cluster.x", NULL);
	requireDeps(nodes_by_fqn, "table.x.public.x", 
		    "schema.x.public", "role.cluster.x",
		    "tablespace.cluster.pg_default", 
		    "fallback.grant.x.superuser", NULL);
	requireDeps(nodes_by_fqn, "grant.x.public.x.trigger:x:x",
		    "table.x.public.x", "role.cluster.x",
		    "fallback.grant.x.superuser", NULL);
	requireDeps(nodes_by_fqn, "drop.fallback.grant.x.superuser", 
		    "role.cluster.x", 
		    "table.x.public.x", "grant.x.public.x.trigger:x:x",
		    "grant.x.public.x.references:x:x", 
		    "grant.x.public.x.rule:x:x", 
		    "grant.x.public.x.select:x:x", 
		    "grant.x.public.x.insert:x:x",
		    "grant.x.public.x.update:x:x",
		    "grant.x.public.x.delete:x:x", NULL);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
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



/* Conditional dependencies tests. */
START_TEST(cond)
{
    Document *doc;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    initBuiltInSymbols();
    initTemplatePath(".");
    //showFree(4247);
    //showMalloc(1390);

    eval("(setq build t)");
    eval("(setq drop t)");
    BEGIN {
	doc = getDoc("test/data/cond_test_with_deps.xml");
	nodes = nodesFromDoc(doc);

	//showVectorDeps(nodes);
	//fprintf(stderr, "-------------------------------------\n");
	prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	nodes_by_fqn = hashByFqn(nodes);

	requireDep(nodes_by_fqn, "table.regressdb.public.thing", 
		   "grant.regressdb.public.create:public:regress");
	requireNoDep(nodes_by_fqn, "table.regressdb.public.thing", 
		     "grant.regressdb.public.usage:public:regress");

	requireDep(nodes_by_fqn, 
		   "drop.grant.regressdb.public.usage:public:regress",
		   "drop.table.regressdb.public.thing");
	requireNoDep(nodes_by_fqn, "drop.table.regressdb.public.thing", 
		     "grant.regressdb.public.create:public:regress");

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
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

START_TEST(cyclic_build2)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Vector *results = NULL;
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(1911);
	//showFree(572);
	eval("(setq build t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = (Vector *) nodesFromDoc(doc);
	//dbgSexp(nodes);
	//showVectorDeps(nodes);

	results = (Vector *) resolving_tsort(nodes);
	//dbgSexp(results);

	objectFree((Object *) results, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	//dbgSexp(nodes);
	objectFree((Object *) results, FALSE);
	objectFree((Object *) nodes, TRUE);
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
deps_suite(void)
{
    Suite *s = suite_create("deps");
    TCase *tc_core = tcase_create("deps");

    ADD_TEST(tc_core, build_type_bitsets);
    ADD_TEST(tc_core, depset_deps1);
    ADD_TEST(tc_core, depset_dag1_build);
    ADD_TEST(tc_core, depset_dag1_drop);
    ADD_TEST(tc_core, depset_dag2_rebuild);
    ADD_TEST(tc_core, depset_dag2_rebuild2);
    ADD_TEST(tc_core, cyclic_build);
    ADD_TEST(tc_core, cyclic_drop);
    ADD_TEST(tc_core, cyclic_both);
    ADD_TEST(tc_core, cond);

    ADD_TEST(tc_core, fallback);
				
    ADD_TEST(tc_core, cyclic_build2);
    suite_add_tcase(s, tc_core);

    return s;
}

