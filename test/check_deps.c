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

static Hash *
dagnodeHash(Vector *vector)
{
    int i;
    DagNode *node;
    String *key;
    Hash *hash = hashNew(TRUE);

    EACH(vector, i) {
	node = (DagNode *) ELEM(vector, i);
	assert(node->type == OBJ_DAGNODE, "Incorrect node type");
	key = stringDup(node->fqn);

	if (hashGet(hash, (Object *) key)) {
	    objectFree((Object *) key, TRUE);
	    key = stringNewByRef(newstr("%s.%s", 
					nameForBuildType(node->build_type),
					node->fqn->value));
	    hashAdd(hash, (Object *) key, (Object *) node);
	}
	else {
	    hashAdd(hash, (Object *) key, (Object *) node);
	}
    }
    return hash;
}

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
    Vector *vector = node->forward_deps;
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
	}
    }
    return FALSE;
}

static DagNodeBuildType
buildTypeFromName(char *name)
{
    char *dotpos;
    int prefix_len;

    if (dotpos = strchr(name, '.')) {
	prefix_len = dotpos - name;
	if (strncmp(name, "build", prefix_len) == 0) {
	    return BUILD_NODE;
	}
	if (strncmp(name, "drop", prefix_len) == 0) {
	    return DROP_NODE;
	}
	if (strncmp(name, "fallback", prefix_len) == 0) {
	    return FALLBACK_NODE;
	}
	if (strncmp(name, "endfallback", prefix_len) == 0) {
	    return ENDFALLBACK_NODE;
	}
	if (strncmp(name, "diffprep", prefix_len) == 0) {
	    return DIFFPREP_NODE;
	}
	if (strncmp(name, "diffcomplete", prefix_len) == 0) {
	    return DIFFCOMPLETE_NODE;
	}
    }
    return UNSPECIFIED_NODE;
}

static int
buildPrefixLen(DagNodeBuildType type)
{
    switch (type) {
    case BUILD_NODE: return 6;
    case DROP_NODE: return 5;
    case FALLBACK_NODE: return 9;
    case ENDFALLBACK_NODE: return 12;
    case DIFFPREP_NODE: return 9;
    case DIFFCOMPLETE_NODE: return 13;
    case UNSPECIFIED_NODE: return 0;
    default:
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("Unhandled build type %d in buildPrefixLen", 
		     (int) type));
    }
    return 0;
}

/* This function is all over the map for historical reasons.  It would
 * be good to clean it up but for now at least it works.
 */
static DagNode *
findDagNode(Hash *hash, char *name)
{
    DagNodeBuildType type = buildTypeFromName(name);
    String *fqn = stringNew(name);
    DagNode *node = (DagNode *) hashGet(hash, (Object *) fqn);
    if (!node) {
	/* Try removing any build type prefix from the name and see if
	 * there is a match that way. */
	objectFree((Object *) fqn, TRUE);
	fqn = stringNew(name + buildPrefixLen(type));
	node = (DagNode *) hashGet(hash, (Object *) fqn);
	if (!node) {
	    dbgSexp(node);
	    dbgSexp(hash);
	}
    }

    objectFree((Object *) fqn, TRUE);
    return node;
}

static boolean
hasDep(Hash *hash, char *from, char *to)
{
    DagNode *fnode = (DagNode *) findDagNode(hash, from);
    DagNode *tnode = (DagNode *) findDagNode(hash, to);

    if (!fnode) {
	fail("Cannot find source node %s ", from);
	return FALSE;
    }

    if (!tnode) {
	fail("Cannot find dependency %s ", to);
	return FALSE;
    }
    
    return hasDependency(fnode, tnode);
}

static void
requireDep(char *testid, Hash *hash, char *from, char *to)
{
    if (!hasDep(hash, from, to)) {
	fail("Test %s: no dep exists from %s to %s", testid, from, to);
    }
}

static void
requireNoDep(char *testid, Hash *hash, char *from, char *to)
{
    if (hasDep(hash, from, to)) {
        fail("Test %s: unwanted dep exists from %s to %s", testid, from, to);
    }
}


static void
requireDeps(char *testid, Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *fromnode;
    int dep_elems;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	requireDep(testid, hash, from, to);
	count++;
    }
    va_end(params);
    fromnode = (DagNode *) findDagNode(hash, from);

    if (!fromnode) {
	fail("Cannot find %s ", from);
    }
    else {
	dep_elems = (fromnode->forward_deps)? fromnode->forward_deps->elems: 0;
	
	if (dep_elems != count) {
	    fail("Test %s.  Not all dependencies of %s accounted for (expecting %d got %d)", 
		 testid, from, count, 
		 (fromnode->forward_deps)? fromnode->forward_deps->elems: 0);
	}
    }
}

static boolean
chkDep(Hash *hash, char *from, char *to)
{
    DagNode *fnode = (DagNode *) findDagNode(hash, from);
    DagNode *tnode = (DagNode *) findDagNode(hash, to);

    if (!fnode) {
	return FALSE;
    }

    if (!tnode) {
	return FALSE;
    }
    
    return hasDependency(fnode, tnode);
}

static boolean
hasDeps(char *testid, Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *fromnode;
    int dep_elems;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	if (!chkDep(hash, from, to)) {
	    return FALSE;
	}
	count++;
    }
    va_end(params);
    return TRUE;
    fromnode = (DagNode *) findDagNode(hash, from);

    if (!fromnode) {
	fail("Test %s: cannot find %s ", testid, from);
	return FALSE;
    }
    else {
	dep_elems = (fromnode->forward_deps)? fromnode->forward_deps->elems: 0;
	
	if (dep_elems != count) {
	    return FALSE;
	}
    }
    return TRUE;
}


static void
requireOptionalDependencies(char *testid, Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *fromnode;
    DagNode *tonode;
    String *key;
    Vector *deplist = vectorNew(10);
    int i;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	vectorPush(deplist, (Object*) stringNew(to));
	count++;
    }
    va_end(params);
    fromnode = (DagNode *) findDagNode(hash, from);
    if (!fromnode) {
	fail("Test %s: cannot find %s ", testid, from);
	return;
    }

    EACH(deplist, i) {
	key = (String *) ELEM(deplist, i);
	tonode = (DagNode *) findDagNode(hash, key->value);
	if (hasDependency(fromnode, tonode)) {
	    objectFree((Object *) deplist, TRUE);
	    return;
	}
    }

    objectFree((Object *) deplist, TRUE);
	
    fail("Test %s: no optional dependencies found in %s", 
	 testid, fromnode->fqn->value);
}

static void
requireOptionalDependents(char *testid, Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *tonode;
    DagNode *fromnode;
    String *key;
    Vector *deplist = vectorNew(10);
    int i;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	vectorPush(deplist, (Object*) stringNew(to));
	count++;
    }
    va_end(params);
    tonode = (DagNode *) findDagNode(hash, from);
    if (!tonode) {
	fail("Test %s: cannot find %s ", testid, from);
	return;
    }

    EACH(deplist, i) {
	key = (String *) ELEM(deplist, i);
	fromnode = (DagNode *) findDagNode(hash, key->value);
	if (hasDependency(fromnode, tonode)) {
	    objectFree((Object *) deplist, TRUE);
	    return;
	}
    }
    objectFree((Object *) deplist, TRUE);
	
    fail("Test %s: no optional dependents found in %s", 
	 testid, tonode->fqn->value);
}



/* Test basic build dependencies, using dependency sets */ 
START_TEST(depset_dag1_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(654);
	//showFree(724);

	eval("(setq build t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes, FALSE);

	requireDeps("D1B_1", nodes_by_fqn, 
		    "role.cluster.r1", "role.cluster.r3", NULL);
	requireDeps("DIB_2", nodes_by_fqn, 
		    "role.cluster.r2", "role.cluster.r3", NULL);

	requireOptionalDependencies("DIB_3", nodes_by_fqn, "role.cluster.r3", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r4", NULL);

	requireOptionalDependencies("D1B_4", nodes_by_fqn, "role.cluster.r4", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r5", NULL);

	requireDeps("D1B_5", nodes_by_fqn, "role.cluster.r5", NULL);



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

/* Test basic (inverted) drop dependencies, using dependency sets. */ 
START_TEST(depset_dag1_drop)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	eval("(setq drop t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireOptionalDependents("DD1D_1", nodes_by_fqn, "role.cluster.r3", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r4", NULL);

	requireOptionalDependents("DD1D_2", nodes_by_fqn, "role.cluster.r4", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r5", NULL);

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


START_TEST(depset_dag1_both)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(901);
	//showFree(415);

	eval("(setq drop t)");
	eval("(setq build t)");

	doc = getDoc("test/data/gensource_depset.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DD1B_1", nodes_by_fqn, "role.cluster.r1", 
		    "drop.role.cluster.r1", "role.cluster.r3", NULL);
	requireDeps("DD1B_2", nodes_by_fqn, "role.cluster.r2", 
		    "drop.role.cluster.r2", "role.cluster.r3", NULL);

	requireDep("DD1B_3", nodes_by_fqn, 
		   "role.cluster.r3", "drop.role.cluster.r3");
	requireOptionalDependents("DD1B_4", nodes_by_fqn, "role.cluster.r3", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r4", NULL);

	requireDep("DD1B_5", nodes_by_fqn, 
		   "role.cluster.r4", "drop.role.cluster.r4");
	requireOptionalDependencies("DD1B_6", nodes_by_fqn, "role.cluster.r4", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r5", NULL);

	requireDeps("DD1B_7", nodes_by_fqn, "role.cluster.r5", 
		    "drop.role.cluster.r5", NULL);

	/* No guaranteed deps for drop r1 */
	/* No guaranteed deps for drop r2 */

	requireDep("DD1B_8", nodes_by_fqn, "drop.role.cluster.r3", 
		   "drop.role.cluster.r1");
	requireDep("DD1B_9", nodes_by_fqn, "drop.role.cluster.r3", 
		   "drop.role.cluster.r2");
	requireOptionalDependents("DD1B_A", nodes_by_fqn, 
				  "drop.role.cluster.r3", 
				  "drop.role.cluster.r1", 
				  "drop.role.cluster.r2", 
				  "drop.role.cluster.r4", NULL);

	/* No guaranteed deps for drop r4 */
	requireOptionalDependents("DD1B_B", nodes_by_fqn, 
				  "drop.role.cluster.r4", 
				  "drop.role.cluster.r1", 
				  "drop.role.cluster.r2", 
				  "drop.role.cluster.r5", NULL);

	/* No guaranteed deps for drop r5 */

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

START_TEST(depset_dia_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;
    eval("(setq dbver (version '8.4'))");

    BEGIN {
	initTemplatePath(".");
	//showMalloc(548);
	//showFree(415);

	eval("(setq build t)");

	doc = getDoc("test/data/gensource_fromdia.xml");
	//dbgSexp(doc);
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes, FALSE);
	requireDeps("DDB_1", nodes_by_fqn, "role.cluster.x", "cluster", NULL);
	requireDeps("DDB_2", nodes_by_fqn, "table.cluster.ownedbyx", 
		    "cluster", "role.cluster.x", 
		    "privilege.cluster.x.superuser", NULL);  // 5, 7

	requireDeps("DDB_3", nodes_by_fqn, "privilege.cluster.x.superuser", 
		    "role.cluster.x", NULL); // 3

	requireDeps("DDB_4", nodes_by_fqn, "endprivilege.cluster.x.superuser", 
		    "privilege.cluster.x.superuser",  
                    "table.cluster.ownedbyx", 
		    "role.cluster.x", 
		    NULL); // 11, 9, 1

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

START_TEST(depset_dia_drop)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	eval("(setq dbver (version '8.4'))");
	//showMalloc(901);
	//showFree(415);

	eval("(setq drop t)");

	doc = getDoc("test/data/gensource_fromdia.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes, FALSE);
	requireDeps("DDD_1", nodes_by_fqn, "role.cluster.x", 
		    "table.cluster.ownedbyx", 
		    "privilege.cluster.x.superuser",
		    "endprivilege.cluster.x.superuser", NULL);  // 6, 4, 2

	requireDeps("DDD_2", nodes_by_fqn, "table.cluster.ownedbyx", 
		    "privilege.cluster.x.superuser", NULL);  // 8

	requireDeps("DDD_3", nodes_by_fqn, "privilege.cluster.x.superuser", 
		    NULL); // 

	requireDeps("DDD_4", nodes_by_fqn, "endprivilege.cluster.x.superuser", 
		    "privilege.cluster.x.superuser",  
                    "table.cluster.ownedbyx", 
		    NULL); // 10, 12

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

START_TEST(depset_dia_both)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	eval("(setq dbver (version '8.4'))");
	//showMalloc(901);
	//showFree(415);

	eval("(setq drop t)");
	eval("(setq build t)");

	doc = getDoc("test/data/gensource_fromdia.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes, FALSE);
	requireDeps("DD2_1", nodes_by_fqn, "role.cluster.x", 
		    "cluster", "drop.role.cluster.x", NULL);  // 15

	requireDeps("DD2_2", nodes_by_fqn, "table.cluster.ownedbyx", 
		    "cluster", "role.cluster.x", "drop.table.cluster.ownedbyx", 
		    "privilege.cluster.x.superuser", NULL);  // 5, 13, 7

	requireDeps("DD2_3", nodes_by_fqn, "privilege.cluster.x.superuser", 
		    "role.cluster.x", 
		    "enddsprivilege.cluster.x.superuser", 
		    NULL); // 3, 14

	requireDeps("DD2_4", nodes_by_fqn, "endprivilege.cluster.x.superuser", 
		    "privilege.cluster.x.superuser",  
                    "table.cluster.ownedbyx", 
		    "role.cluster.x", 
		    NULL); // 11, 9, 1

	requireDeps("DD2_5", nodes_by_fqn, "drop.role.cluster.x", 
		    "drop.table.cluster.ownedbyx", 
		    "enddsprivilege.cluster.x.superuser", 
		    "dsprivilege.cluster.x.superuser", 
		    NULL); // 6, 2, 4

	requireDeps("DD2_6", nodes_by_fqn, "drop.table.cluster.ownedbyx", 
		    "dsprivilege.cluster.x.superuser", 
		    NULL); // 8

	requireDeps("DD2_7", nodes_by_fqn, 
		    "enddsprivilege.cluster.x.superuser", 
		    "drop.table.cluster.ownedbyx", 
		    "dsprivilege.cluster.x.superuser", 
		    NULL); // 10, 12

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

START_TEST(depset_diff)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	eval("(setq drop t)");
	eval("(setq build t)");
	/* This xmlfile is created from the diff regression test:
	 * ./skit --diff regress/scratch/regressdb_dump3a.xml \
	 *      regress/scratch/regressdb_dump3b.xml \
	 *        >test/data/diffstream1.xml
	 * Run "make prep" to refresh this file.
	 */
	doc = getDoc("test/data/diffstream1.xml");
	nodes = dagFromDoc(doc);

	//deps for tbs2 should be different in prep and complete stages
	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);

	requireDep("DD1", nodes_by_fqn, "diffcomplete.tablespace.cluster.tbs2", 
		   "diffcomplete.role.cluster.keep");
	requireNoDep("DD2", nodes_by_fqn, 
		     "diffcomplete.tablespace.cluster.tbs2", 
		   "diffcomplete.role.cluster.regress");
	requireDep("DD3", nodes_by_fqn, "diffprep.role.cluster.regress",
	           "diffprep.tablespace.cluster.tbs2");
	requireNoDep("DD4", nodes_by_fqn, "diffprep.role.cluster.keep",
	           "diffprep.tablespace.cluster.tbs2");
	
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


START_TEST(cyclic_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(1584);
	//showFree(1504);
	eval("(setq build t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes, FALSE);
	
	if (hasDeps("CB_0", nodes_by_fqn, "view.skittest.public.v3", 
		    "viewbase.skittest.public.v1",
		    "schema.skittest.public", "role.cluster.marc",
		    "privilege.cluster.marc.superuser", NULL)) 
	{
	    // V3 --> VIEWBASE 1
	    requireDeps("CB_1", nodes_by_fqn, "view.skittest.public.v1", 
			"build.view.skittest.public.v2",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", NULL);
	    requireDeps("CB_2", nodes_by_fqn, "view.skittest.public.v2", 
			"build.view.skittest.public.v3",
			"schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps("CB_3", nodes_by_fqn, 
			"build.viewbase.skittest.public.v1", 
			"schema.skittest.public", "role.cluster.marc", NULL);
	}
	else if (hasDeps("CB_4", nodes_by_fqn, "view.skittest.public.v2", 
			 "build.viewbase.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL)) 
	{
	    // V2 --> VIEWBASE 3
	    requireDeps("CB_5", nodes_by_fqn, "view.skittest.public.v3", 
			"build.view.skittest.public.v1",
			"schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps("CB_6", nodes_by_fqn, "view.skittest.public.v1", 
			"build.view.skittest.public.v2",
			"schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps("CB_7", nodes_by_fqn, 
			"build.viewbase.skittest.public.v3", 
			"schema.skittest.public", "role.cluster.marc", NULL);
	}
	else if (hasDeps("CB_8", nodes_by_fqn, "view.skittest.public.v2", 
			 "build.viewbase.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL))
	{
	    // V1 --> VIEWBASE 2
	    requireDeps("CB_9", nodes_by_fqn, "view.skittest.public.v2", 
			"build.view.skittest.public.v3",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", NULL);
	    requireDeps("CB_A", nodes_by_fqn, "view.skittest.public.v3", 
			"build.view.skittest.public.v1",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", NULL);
	    requireDeps("CB_B", nodes_by_fqn, 
			"build.viewbase.skittest.public.v2", 
			"schema.skittest.public", "role.cluster.marc", NULL);
	}
	else {
	    fail("No cycle breaker found");
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
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(813);
	//showFree(572);
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);


	/* Note that viewbase and view positions in the sorted output
	 * will have been inverted when compared to the build direction.
	 * This is due to the action of swapBackwardBreakers.  The
	 * comments (eg V3 <-- VIEWBASE 1) show the original pre-swap
	 * dependencies (for historical reasons and to make it easier
	 * to compare with the tests for cyclic_build). */

	if (hasDeps("CD_1", nodes_by_fqn, "drop.view.skittest.public.v1",
		     "drop.view.skittest.public.v3", NULL))
	{
	    // V3 <-- VIEWBASE 1
	    requireDeps("CD_2", nodes_by_fqn, "drop.view.skittest.public.v3",
			"drop.view.skittest.public.v2", NULL);
	    requireDeps("CD_3", nodes_by_fqn, "drop.view.skittest.public.v2",
			"drop.viewbase.skittest.public.v1", NULL);

	    requireDeps("CD_4", nodes_by_fqn, "drop.schema.skittest.public", 
			"drop.view.skittest.public.v1",
			"drop.view.skittest.public.v2",
			"drop.view.skittest.public.v3",
			"drop.viewbase.skittest.public.v1", 
			"drop.grant.skittest.public.usage:public:regress", 
			NULL);
	}
	else if (hasDeps("CD_5", nodes_by_fqn, "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL))
	{
	    fprintf(stderr, "BBBBBBBBBBBBB\n");
	    // V1 <-- VIEWBASE 2
	    requireDeps("CD_6", nodes_by_fqn, "drop.view.skittest.public.v1",
			"drop.view.skittest.public.v3", NULL);
	    requireDeps("CD_7", nodes_by_fqn, "drop.view.skittest.public.v3",
			"drop.viewbase.skittest.public.v2", NULL);

	    requireDeps("CD_8", nodes_by_fqn, "drop.schema.skittest.public", 
			"drop.view.skittest.public.v1",
			"drop.view.skittest.public.v2",
			"drop.view.skittest.public.v3",
			"drop.viewbase.skittest.public.v2", 
			"drop.grant.skittest.public.usage:public:regress", 
			NULL);
	}
	else if (hasDeps("CD_9", nodes_by_fqn, "drop.view.skittest.public.v3",
			 "drop.view.skittest.public.v2", NULL))
	{
	    fprintf(stderr, "CCCCCCCCCCCCCCCCCCCC\n");
	    // V2 <-- VIEWBASE 3
	    requireDeps("CD_A", nodes_by_fqn, "drop.view.skittest.public.v2",
			"drop.view.skittest.public.v1", NULL);
	    requireDeps("CD_B", nodes_by_fqn, "drop.view.skittest.public.v1",
			"drop.viewbase.skittest.public.v3", NULL);

	    requireDeps("CD_C", nodes_by_fqn, "drop.schema.skittest.public", 
			"drop.view.skittest.public.v1",
			"drop.view.skittest.public.v2",
			"drop.view.skittest.public.v3",
			"drop.viewbase.skittest.public.v3", 
			"drop.grant.skittest.public.usage:public:regress", 
			NULL);
	}
	else {
	    fail("No cycle breaker found");
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


START_TEST(cyclic_both)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(813);
	//showFree(572);
	eval("(setq drop t)");
	eval("(setq build t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	if (hasDeps("C2_1", nodes_by_fqn, "view.skittest.public.v3", 
		    "viewbase.skittest.public.v1",
		    "schema.skittest.public", "role.cluster.marc",
		    "privilege.cluster.marc.superuser", NULL)) 
	{
	    // V3 --> VIEWBASE 1
	    requireDeps("C2_2", nodes_by_fqn, "view.skittest.public.v1", 
			"view.skittest.public.v2",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", 
			"drop.viewbase.skittest.public.v1", NULL);
	    requireDeps("C2_3", nodes_by_fqn, "view.skittest.public.v2", 
			"view.skittest.public.v3",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", 
			"drop.view.skittest.public.v2", NULL);
	    requireDeps("C2_4", nodes_by_fqn, 
			"build.viewbase.skittest.public.v1", 
			"schema.skittest.public", "role.cluster.marc", 
			"drop.view.skittest.public.v1", NULL);
	}
	else if (hasDeps("C2_5", nodes_by_fqn, "view.skittest.public.v2", 
			 "viewbase.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL)) 
	{
	    fail("FAIL 2"); // Fix the following test if this fail happens
	    // V2 --> VIEWBASE 3
	    requireDeps("C2_6", nodes_by_fqn, "view.skittest.public.v3", 
			"view.skittest.public.v1",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", 
			"drop.view.skittest.public.v3", NULL);
	    requireDeps("C2_7", nodes_by_fqn, "view.skittest.public.v1", 
			"view.skittest.public.v2",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", 
			"drop.view.skittest.public.v1", NULL);
	    requireDeps("C2_8", nodes_by_fqn, 
			"build.viewbase.skittest.public.v3", 
			"schema.skittest.public", "role.cluster.marc", 
			"drop.viewbase.skittest.public.v3", NULL);
	}
	else if (hasDeps("C2_9", nodes_by_fqn, "view.skittest.public.v2", 
			 "viewbase.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL))
	{
	    fail("FAIL 3");// Fix the following test if this fail happens
	    // V1 --> VIEWBASE 2
	    requireDeps("C2_A", nodes_by_fqn, "view.skittest.public.v2", 
			"view.skittest.public.v3",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", 
			"drop.view.skittest.public.v2", NULL);
	    requireDeps("C2_B", nodes_by_fqn, "view.skittest.public.v3", 
			"view.skittest.public.v1",
			"schema.skittest.public", "role.cluster.marc",
			"privilege.cluster.marc.superuser", 
			"drop.view.skittest.public.v3", NULL);
	    requireDeps("C2_C", nodes_by_fqn, 
			"build.viewbase.skittest.public.v2", 
			"schema.skittest.public", "role.cluster.marc", 
			"drop.viewbase.skittest.public.v2", NULL);
	}
	else {
	    fail("No cycle breaker found in build side");
	}

	if (hasDeps("C2_D", nodes_by_fqn, "drop.view.skittest.public.v2",
		    "drop.viewbase.skittest.public.v1", NULL))
	{
	    // V2 --> VIEWBASE 1
	    requireDeps("C2_E", nodes_by_fqn, "drop.view.skittest.public.v3",
			"drop.view.skittest.public.v2", NULL);
	    requireDeps("C2_F", nodes_by_fqn, "drop.view.skittest.public.v1",
			"drop.view.skittest.public.v3", NULL);

	    requireDeps("C2_G", nodes_by_fqn, "drop.schema.skittest.public", 
			"drop.view.skittest.public.v1",
			"drop.view.skittest.public.v2",
			"drop.view.skittest.public.v3",
			"drop.viewbase.skittest.public.v1", 
			"drop.grant.skittest.public.usage:public:regress", 
			NULL);
	}
	else if (hasDeps("C2_H", nodes_by_fqn, 
			 "drop.viewbase.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL))
	{
	    fail("FAIL 2A");// Fix the following test if this fail happens
	}
	else if (hasDeps("C2_J", nodes_by_fqn, 
			 "drop.viewbase.skittest.public.v3",
			 "drop.view.skittest.public.v2", NULL))
	{
	    fail("FAIL 3A");// Fix the following test if this fail happens
	}
	else {
	    // Fix the else if clauses above if this fail is reached.
	    fail("No cycle breaker found in drop side");
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


START_TEST(fallback)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(642);
	//showFree(5641);
	eval("(setq build t)");
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource_fallback.xml");
	//doc = getDoc("tmp.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes, FALSE);

	requireDeps("F_1", nodes_by_fqn, "fallback.grant.x.superuser", 
		    "drop.fallback.grant.x.superuser",
		    "role.cluster.x", NULL);
	requireDeps("F_2", nodes_by_fqn, "table.x.public.x", 
		    "role.cluster.x","schema.x.public", 
		    "tablespace.cluster.pg_default", 
		    "fallback.grant.x.superuser", 
		    "drop.table.x.public.x", NULL);
	requireDeps("F_3", nodes_by_fqn, "grant.x.public.x.trigger:x:x",
		    "table.x.public.x", "role.cluster.x",
		    "fallback.grant.x.superuser", 
		    "drop.grant.x.public.x.trigger:x:x", NULL);

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

    initTemplatePath(".");
    //showFree(4247);
    //showMalloc(1390);

    eval("(setq build t)");
    eval("(setq drop t)");
    BEGIN {
	doc = getDoc("test/data/cond_test_with_deps.xml");
	nodes = dagFromDoc(doc);

	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);

	requireDep("C_1", nodes_by_fqn, "table.regressdb.public.thing", 
		   "grant.regressdb.public.create:public:regress");
	requireNoDep("C_2", nodes_by_fqn, "table.regressdb.public.thing", 
		     "grant.regressdb.public.usage:public:regress");

	requireDep("C_3", nodes_by_fqn, 
		   "drop.grant.regressdb.public.usage:public:regress",
		   "drop.table.regressdb.public.thing");
	requireNoDep("C_4", nodes_by_fqn, 
		     "drop.grant.regressdb.public.create:public:regress",
		     "drop.table.regressdb.public.thing");

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

#ifdef wibble
static Document *
depdiffs(char *path1, char *path2)
{
    Document *indoc;
    String *diffrules = stringNew("diffrules.xml");
    xmlNode *diffs_root;
    xmlDocPtr docnode; 
    Document *result;

    indoc = getDoc(path1);
    docStackPush(indoc);
    indoc = getDoc(path2);
    docStackPush(indoc);
    diffs_root = doDiff(diffrules, FALSE);
    objectFree((Object *) diffrules, TRUE);

    docnode = xmlNewDoc((xmlChar *) "1.0");
    xmlDocSetRootElement(docnode, diffs_root);
    result = documentNew(docnode, NULL);
    return result;
}
#endif

#ifdef wibble
static void
check_testcase_1(Hash *nodes_by_fqn)
{
    requireDeps("D1_TC1_1", nodes_by_fqn, 
		"diffcomplete.sequence.test_data.n1.s1", 
		"diffcomplete.schema.test_data.n1",
		"diffprep.sequence.test_data.n1.s1", NULL);
    requireDeps("D1_TC1_2", nodes_by_fqn, 
		"drop.sequence.test_data.n1.s1a", 
		"drop.grant.sequence.test_data.n1.s1a.usage", 
		"drop.grant.sequence.test_data.n1.s1a.update", 
		"drop.grant.sequence.test_data.n1.s1a.select", 
		NULL);
    requireDeps("D1_TC1_3", nodes_by_fqn, 
		"build.sequence.test_data.n1.s1b", 
		"diffcomplete.schema.test_data.n1", NULL);
}

static void
check_testcase_2(Hash *nodes_by_fqn)
{
    requireDeps("D1_TC1_2", nodes_by_fqn, 
		"diffcomplete.sequence.test_data.n1.s1", 
		"diffcomplete.schema.test_data.n1",
		"diffprep.sequence.test_data.n1.s1", NULL);
}

/* Dependency diff tests. */
START_TEST(depdiffs_1)
{
    Document *volatile diffs = NULL;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    initTemplatePath(".");
    diffs = depdiffs("test/data/depdiffs_1a.xml",
		     "test/data/depdiffs_1b.xml");

    //dbgSexp(diffs);

    BEGIN {
	nodes = dagFromDoc(diffs);
	nodes_by_fqn = dagnodeHash(nodes);
	fprintf(stderr, "\n============FINAL==============\n");
	showVectorDeps(nodes, FALSE);

	/* See comments in test/data/depdiffs_1a.sql for a description
	   of each test case. */
	check_testcase_1(nodes_by_fqn);
	check_testcase_2(nodes_by_fqn);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
    }
    FINALLY {
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) diffs, TRUE);
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST
#endif

Suite *
deps_suite(void)
{
    Suite *s = suite_create("deps");
    TCase *tc_core = tcase_create("deps");

    ADD_TEST(tc_core, depset_dag1_build);
    ADD_TEST(tc_core, depset_dag1_drop);
    ADD_TEST(tc_core, depset_dag1_both);
    ADD_TEST(tc_core, depset_dia_build);
    ADD_TEST(tc_core, depset_dia_drop);
    ADD_TEST(tc_core, depset_dia_both);
    ADD_TEST(tc_core, depset_diff);
    ADD_TEST(tc_core, cyclic_build);
    ADD_TEST(tc_core, cyclic_drop);
    ADD_TEST(tc_core, cyclic_both);
    ADD_TEST(tc_core, cond);
    //ADD_TEST(tc_core, depdiffs_1);

    ADD_TEST(tc_core, fallback);
				
    suite_add_tcase(s, tc_core);

    return s;
}
