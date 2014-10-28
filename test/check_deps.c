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
    Vector *vector = node->deps;
    Object *depobj;
    int i;
    if (vector) {
	for (i = 0; i < vector->elems; i++) {
	    depobj = vector->contents->vector[i];
	    if (depobj && depobj->type == OBJ_DAGNODE) {
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
	prefix_len = dotpos - name + 1;
	if (strncmp(name, "build.", prefix_len) == 0) {
	    return BUILD_NODE;
	}
	if (strncmp(name, "drop.", prefix_len) == 0) {
	    return DROP_NODE;
	}
	if (strncmp(name, "fallback.", prefix_len) == 0) {
	    return FALLBACK_NODE;
	}
	if (strncmp(name, "endfallback.", prefix_len) == 0) {
	    return ENDFALLBACK_NODE;
	}
	if (strncmp(name, "diff.", prefix_len) == 0) {
	    return DIFF_NODE;
	}
	if (strncmp(name, "diffprep.", prefix_len) == 0) {
	    return DIFFPREP_NODE;
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
    case DIFF_NODE: return 5;
    case DIFFPREP_NODE: return 9;
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
    }

    objectFree((Object *) fqn, TRUE);
    return node;
}

static boolean
hasDep(Hash *hash, char *from, char *to, char *id)
{
    DagNode *fnode = (DagNode *) findDagNode(hash, from);
    DagNode *tnode = (DagNode *) findDagNode(hash, to);

    if (!fnode) {
	fail("Cannot find (in %s) source node %s ", id, from);
	return FALSE;
    }

    if (!tnode) {
	fail("Cannot find (in %s) dependency %s ", id, to);
	return FALSE;
    }
    
    return hasDependency(fnode, tnode);
}

static void
requireDep(char *testid, Hash *hash, char *from, char *to)
{
    if (!hasDep(hash, from, to, testid)) {
	fail("Test %s: no dep exists from %s to %s", testid, from, to);
    }
}

static void
requireNoDep(char *testid, Hash *hash, char *from, char *to)
{
    if (hasDep(hash, from, to, testid)) {
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
	dep_elems = (fromnode->deps)? fromnode->deps->elems: 0;
	
	if (dep_elems != count) {
	    fail("Test %s.  Not all dependencies of %s accounted for "
		 "(expecting %d got %d)", testid, from, count, 
		 (fromnode->deps)? fromnode->deps->elems: 0);
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
	dep_elems = (fromnode->deps)? fromnode->deps->elems: 0;
	
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



/* Just check for memory leaks. */
START_TEST(deps_basic)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(2697);
	//trackMalloc(1589);
	//showFree(1627);

	eval("(setq build t)");
	doc = getDoc("test/data/diffstream1.xml");
	//doc = getDoc("test/data/deps_simple.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	//showVectorDeps(nodes);


	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
	fprintf(stderr, "EXCEPTION %d, %s\n", ex->signal, ex->text);
	fprintf(stderr, "%s\n", ex->backtrace);
	failed = TRUE;
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("deps_basic fails with exception");
    }
}
END_TEST


START_TEST(deps_simple_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(839);
	//showFree(724);

	eval("(setq build t)");
	doc = getDoc("test/data/deps_simple.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DSB_1", nodes_by_fqn, 
		    "dbincluster.regressdb", "cluster", NULL);
	requireDeps("DSB_2", nodes_by_fqn, 
		    "database.regressdb", "dbincluster.regressdb", NULL);
	requireDeps("DSB_3", nodes_by_fqn, 
		    "schema.regressdb.public", "database.regressdb", NULL);
	requireDeps("DSB_4", nodes_by_fqn, 
		    "language.regressdb.plpgsql", "database.regressdb", NULL);
	requireDeps("DSB_5", nodes_by_fqn, 
		    "type.regressdb.public.seg",
		    "schema.regressdb.public", 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSB_6", nodes_by_fqn, 
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "type.regressdb.public.seg",
		    "schema.regressdb.public", NULL);
	requireDeps("DSB_7", nodes_by_fqn, 
		    "function.regressdb.public.seg2int(public.seg)",
		    "type.regressdb.public.seg",
		    "schema.regressdb.public", 
		    "language.regressdb.plpgsql", NULL);
	requireDeps("DSB_8", nodes_by_fqn, 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public", NULL);
	requireDeps("DSB_9", nodes_by_fqn, 
		    "function.regressdb.public.seg_out(public.seg)",
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public", NULL);

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
	fail("deps_simple_build fails with exception");
    }
}
END_TEST


START_TEST(deps_simple_drop)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(839);
	//showFree(724);

	eval("(setq drop t)");
	doc = getDoc("test/data/deps_simple.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DSD_1", nodes_by_fqn, 
		    "cluster", "dbincluster.regressdb", NULL);
	requireDeps("DSD_2", nodes_by_fqn, 
		    "dbincluster.regressdb", "database.regressdb", NULL);
	requireDeps("DSD_3", nodes_by_fqn, 
		    "database.regressdb", 
		    "schema.regressdb.public", 
		    "language.regressdb.plpgsql", NULL);
	requireDeps("DSD_4", nodes_by_fqn, 
		    "schema.regressdb.public", 
		    "type.regressdb.public.seg",
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "function.regressdb.public.seg2int(public.seg)",
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSD_5", nodes_by_fqn, 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "type.regressdb.public.seg",
		    "function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSD_6", nodes_by_fqn, 
		    "function.regressdb.public.seg_out(public.seg)", 
		    "type.regressdb.public.seg", NULL);
	requireDeps("DSD_7", nodes_by_fqn, 
		    "type.regressdb.public.seg",
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "function.regressdb.public.seg2int(public.seg)", NULL);
	requireDeps("DSD_8", nodes_by_fqn, 
		    "language.regressdb.plpgsql",
		    "function.regressdb.public.seg2int(public.seg)", NULL);

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
	fail("deps_simple_drop fails with exception");
    }
}
END_TEST


START_TEST(deps_simple_rebuild)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(839);
	//showFree(724);

	eval("(setq build t)");
	eval("(setq drop t)");
	doc = getDoc("test/data/deps_simple.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DSR_1", nodes_by_fqn, 
		    "dbincluster.regressdb", "cluster", 
		    "drop.dbincluster.regressdb", NULL);
	requireDeps("DSR_2", nodes_by_fqn, 
		    "database.regressdb", "dbincluster.regressdb", 
		    "drop.database.regressdb", NULL);
	requireDeps("DSR_3", nodes_by_fqn, 
		    "schema.regressdb.public", "database.regressdb", 
		    "drop.schema.regressdb.public", NULL);
	requireDeps("DSR_4", nodes_by_fqn, 
		    "language.regressdb.plpgsql", "database.regressdb", 
		    "drop.language.regressdb.plpgsql", NULL);
	requireDeps("DSR_5", nodes_by_fqn, 
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "function.regressdb.public.seg_out(public.seg)", 
		    "drop.type.regressdb.public.seg", NULL);
	requireDeps("DSR_6", nodes_by_fqn, 
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "drop.function.regressdb.public.seg_cmp"
		    "(public.seg,public.seg)",  NULL);
	requireDeps("DSR_7", nodes_by_fqn, 
		    "function.regressdb.public.seg2int(public.seg)",
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "drop.function.regressdb.public.seg2int(public.seg)",
		    "language.regressdb.plpgsql", NULL);
	requireDeps("DSR_8", nodes_by_fqn, 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "drop.function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public", NULL);
	requireDeps("DSR_9", nodes_by_fqn, 
		    "function.regressdb.public.seg_out(public.seg)",
		    "drop.function.regressdb.public.seg_out(public.seg)",
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public", NULL);
	requireDeps("DSR_10", nodes_by_fqn, 
		    "drop.cluster", "drop.dbincluster.regressdb", NULL);
	requireDeps("DSR_11", nodes_by_fqn, 
		    "drop.dbincluster.regressdb", 
		    "drop.database.regressdb", NULL);
	requireDeps("DSR_12", nodes_by_fqn, 
		    "drop.database.regressdb", "drop.schema.regressdb.public", 
		    "drop.language.regressdb.plpgsql", NULL);
	requireDeps("DSR_13", nodes_by_fqn, 
		    "drop.schema.regressdb.public", 
		    "drop.type.regressdb.public.seg",
		    "drop.function.regressdb.public.seg_cmp"
		    "(public.seg,public.seg)",
		    "drop.function.regressdb.public.seg2int(public.seg)",
		    "drop.function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "drop.function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSR_14", nodes_by_fqn, 
		    "drop.function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "drop.type.regressdb.public.seg",
		    "drop.function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSR_15", nodes_by_fqn, 
		    "drop.function.regressdb.public.seg_out(public.seg)", 
		    "drop.type.regressdb.public.seg", NULL);
	requireDeps("DSR_16", nodes_by_fqn, 
		    "drop.type.regressdb.public.seg",
		    "drop.function.regressdb.public.seg_cmp"
		    "(public.seg,public.seg)",
		    "drop.function.regressdb.public.seg2int(public.seg)", NULL);
	requireDeps("DSR_17", nodes_by_fqn, 
		    "drop.language.regressdb.plpgsql",
		    "drop.function.regressdb.public.seg2int(public.seg)", NULL);

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
	fail("deps_simple_rebuild fails with exception");
    }
}
END_TEST


/* Like deps_simple_build but the source file contains dependency sets.
 */
START_TEST(depset_simple_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(1167);
	//showFree(724);

	eval("(setq build t)");
	doc = getDoc("test/data/depset_simple.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DSSB_1", nodes_by_fqn, 
		    "dbincluster.regressdb", "cluster", 
		    "role.regress", NULL);
	requireDeps("DSSB_2", nodes_by_fqn, 
		    "database.regressdb", "dbincluster.regressdb", NULL);
	requireDeps("DSSB_3", nodes_by_fqn, 
		    "schema.regressdb.public", "database.regressdb", 
		    "role.regress", NULL);
	requireDeps("DSSB_4", nodes_by_fqn, 
		    "language.regressdb.plpgsql", "database.regressdb", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSB_5", nodes_by_fqn, 
		    "type.regressdb.public.seg",
		    "schema.regressdb.public", 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "function.regressdb.public.seg_out(public.seg)", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSB_6", nodes_by_fqn, 
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSB_7", nodes_by_fqn, 
		    "function.regressdb.public.seg2int(public.seg)",
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "language.regressdb.plpgsql", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSB_8", nodes_by_fqn, 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSB_9", nodes_by_fqn, 
		    "function.regressdb.public.seg_out(public.seg)",
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSB_10", nodes_by_fqn, 
		    "role.regress", "cluster", NULL);
	requireDeps("DSSB_11", nodes_by_fqn, 
		    "role.bark", "database.regressdb", NULL);
	requireDeps("DSSB_12", nodes_by_fqn, 
		    "privilege.role.bark.superuser", "role.bark", NULL);

	// TODO: Add tests for the roles: regress

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
	fail("deps_simple_build fails with exception");
    }
}
END_TEST


START_TEST(depset_simple_drop)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(839);
	//showFree(724);

	eval("(setq drop t)");
	doc = getDoc("test/data/depset_simple.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DSSD_1", nodes_by_fqn, 
		    "cluster", "dbincluster.regressdb", "role.regress", NULL);
	requireDeps("DSSD_2", nodes_by_fqn, 
		    "dbincluster.regressdb", "database.regressdb", NULL);
	requireDeps("DSSD_3", nodes_by_fqn, 
		    "database.regressdb", 
		    "schema.regressdb.public", "role.bark",
		    "language.regressdb.plpgsql", NULL);
	requireDeps("DSSD_4", nodes_by_fqn, 
		    "schema.regressdb.public", 
		    "type.regressdb.public.seg",
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "function.regressdb.public.seg2int(public.seg)",
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSSD_5", nodes_by_fqn, 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "type.regressdb.public.seg",
		    "function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSSD_6", nodes_by_fqn, 
		    "function.regressdb.public.seg_out(public.seg)", 
		    "type.regressdb.public.seg", NULL);
	requireDeps("DSSD_7", nodes_by_fqn, 
		    "type.regressdb.public.seg",
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "function.regressdb.public.seg2int(public.seg)", NULL);
	requireDeps("DSSD_8", nodes_by_fqn, 
		    "language.regressdb.plpgsql",
		    "function.regressdb.public.seg2int(public.seg)", NULL);
	requireDeps("DSSB_10", nodes_by_fqn, 
		    "role.bark", 
		    "privilege.role.bark.superuser", 
		    "privilege.role.bark.inherit",
		    "privilege.role.bark.createrole",
		    "privilege.role.bark.createdb",
		    "language.regressdb.plpgsql",
		    "type.regressdb.public.seg",
		    "function.regressdb.public.seg2int(public.seg)",
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "function.regressdb.public.seg_out(public.seg)", NULL);

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
	fail("deps_simple_drop fails with exception");
    }
}
END_TEST


START_TEST(depset_simple_rebuild)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(839);
	//showFree(724);

	eval("(setq build t)");
	eval("(setq drop t)");
	doc = getDoc("test/data/depset_simple.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DSSR_1", nodes_by_fqn, 
		    "dbincluster.regressdb", "cluster", 
		    "drop.dbincluster.regressdb", 
		    "role.regress", NULL);
	requireDeps("DSSR_2", nodes_by_fqn, 
		    "database.regressdb", "dbincluster.regressdb", 
		    "drop.database.regressdb", NULL);
	requireDeps("DSSR_3", nodes_by_fqn, 
		    "schema.regressdb.public", "database.regressdb", 
		    "drop.schema.regressdb.public", 
		    "role.regress", NULL);
	requireDeps("DSR_4", nodes_by_fqn, 
		    "language.regressdb.plpgsql", "database.regressdb", 
		    "drop.language.regressdb.plpgsql", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSR_5", nodes_by_fqn, 
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "function.regressdb.public.seg_out(public.seg)", 
		    "drop.type.regressdb.public.seg", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSR_6", nodes_by_fqn, 
		    "function.regressdb.public.seg_cmp(public.seg,public.seg)",
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "drop.function.regressdb.public.seg_cmp"
		    "(public.seg,public.seg)",
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSR_7", nodes_by_fqn, 
		    "function.regressdb.public.seg2int(public.seg)",
		    "type.regressdb.public.seg", "schema.regressdb.public", 
		    "drop.function.regressdb.public.seg2int(public.seg)",
		    "language.regressdb.plpgsql",
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSR_8", nodes_by_fqn, 
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "drop.function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public",
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSR_9", nodes_by_fqn, 
		    "function.regressdb.public.seg_out(public.seg)",
		    "drop.function.regressdb.public.seg_out(public.seg)",
		    "function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "schema.regressdb.public", 
		    "role.bark", "privilege.role.bark.superuser", NULL);
	requireDeps("DSSR_10", nodes_by_fqn, 
		    "drop.cluster", "drop.dbincluster.regressdb", 
		    "drop.role.regress", NULL);
	requireDeps("DSSR_11", nodes_by_fqn, 
		    "drop.dbincluster.regressdb", 
		    "drop.database.regressdb", NULL);
	requireDeps("DSSR_12", nodes_by_fqn, 
		    "drop.database.regressdb", "drop.schema.regressdb.public", 
		    "drop.language.regressdb.plpgsql", "drop.role.bark", 
		    NULL);
	requireDeps("DSSR_13", nodes_by_fqn, 
		    "drop.schema.regressdb.public", 
		    "drop.type.regressdb.public.seg",
		    "drop.function.regressdb.public.seg_cmp"
		    "(public.seg,public.seg)",
		    "drop.function.regressdb.public.seg2int(public.seg)",
		    "drop.function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "drop.function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSSR_14", nodes_by_fqn, 
		    "drop.function.regressdb.public.seg_in(pg_catalog.cstring)",
		    "drop.type.regressdb.public.seg",
		    "drop.function.regressdb.public.seg_out(public.seg)", NULL);
	requireDeps("DSSR_15", nodes_by_fqn, 
		    "drop.function.regressdb.public.seg_out(public.seg)", 
		    "drop.type.regressdb.public.seg", NULL);
	requireDeps("DSSR_16", nodes_by_fqn, 
		    "drop.type.regressdb.public.seg",
		    "drop.function.regressdb.public.seg_cmp"
		    "(public.seg,public.seg)",
		    "drop.function.regressdb.public.seg2int(public.seg)", NULL);
	requireDeps("DSSR_17", nodes_by_fqn, 
		    "drop.language.regressdb.plpgsql",
		    "drop.function.regressdb.public.seg2int(public.seg)", NULL);

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
	fail("deps_simple_rebuild fails with exception");
    }
}
END_TEST


/* Test basic build dependencies, using dependency sets */ 
START_TEST(depset_dag1_build)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	//showMalloc(727);
	//showFree(724);

	eval("(setq build t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	nodes_by_fqn = dagnodeHash(nodes);

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
	//showMalloc(727);
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
	//showMalloc(678);
	//showFree(509);

	eval("(setq build t)");

	doc = getDoc("test/data/gensource_fromdia.xml");
	//dbgSexp(doc);
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("DDB_1", nodes_by_fqn, "role.x", "cluster", NULL);
	requireDeps("DDB_2", nodes_by_fqn, "table.cluster.ownedbyx", 
		    "cluster", "role.x", 
		    "privilege.role.x.superuser.1", NULL);  // 5, 7

	requireDeps("DDB_3", nodes_by_fqn, 
		    "endfallback.privilege.role.x.superuser.1", 
                    "table.cluster.ownedbyx", 
		    "cluster", "role.x", 
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
	//dbgSexp(nodes_by_fqn);
	//fprintf(stderr, "\n---------------------\n");
	//showVectorDeps(nodes);	
	//fprintf(stderr, "---------------------\n\n");

	requireDeps("DDD_1", nodes_by_fqn, "role.x", 
		    "table.cluster.ownedbyx", 
		    "privilege.role.x.superuser.1",
		    "endfallback.privilege.role.x.superuser.1",
		    NULL);  // 6, 4, 2

	requireDeps("DDD_2", nodes_by_fqn, "table.cluster.ownedbyx", 
		    "privilege.role.x.superuser.1", NULL);  // 8

	requireDeps("DDD_3", nodes_by_fqn, "privilege.role.x.superuser.1", 
		    NULL); // 

	requireDeps("DDD_4", nodes_by_fqn, 
		    "endfallback.privilege.role.x.superuser.1", 
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
	//showMalloc(1611);
	//showFree(694);
	eval("(setq drop t)");
	eval("(setq build t)");

	doc = getDoc("test/data/gensource_fromdia.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//dbgSexp(nodes_by_fqn);
	//showVectorDeps(nodes);
 
	/* Comments below refer to numbered dependencies in 
	 * file: dependency_resolution.dia
	 * Note that deps 11 and 12 have been deemed superfluous.
	 */
	requireDeps("DD2_1", nodes_by_fqn, "cluster", "drop.cluster", NULL);
	requireDeps("DD2_2", nodes_by_fqn,         // 15 
		    "role.x", "cluster", "drop.role.x", NULL);
	requireDeps("DD2_3", nodes_by_fqn,         // 13, 5, 7
		    "table.cluster.ownedbyx", 
		    "drop.table.cluster.ownedbyx", 
		    "cluster", "role.x", 
		    "privilege.role.x.superuser.1", NULL);
	requireDeps("DD2_4", nodes_by_fqn,
		    "drop.cluster", "drop.table.cluster.ownedbyx", 
		    "drop.role.x", "privilege.role.x.superuser.2",
		    "endfallback.privilege.role.x.superuser.2", NULL);
	requireDeps("DD2_5", nodes_by_fqn,         // 2, 4, 6
		    "drop.role.x", "drop.table.cluster.ownedbyx", 
		    "privilege.role.x.superuser.2",
		    "endfallback.privilege.role.x.superuser.2", NULL);
	requireDeps("DD2_6", nodes_by_fqn,         // 8
		    "drop.table.cluster.ownedbyx", 
		    "privilege.role.x.superuser.2", NULL);
	requireDeps("DD2_7", nodes_by_fqn,         // 3, 14
		    "privilege.role.x.superuser.1",
		    "role.x", "endfallback.privilege.role.x.superuser.2", 
		    "cluster", NULL);
	requireDeps("DD2_8", nodes_by_fqn,         // 1, 9
		    "endfallback.privilege.role.x.superuser.1",
		    "role.x", "table.cluster.ownedbyx", "cluster", NULL);
	requireDeps("DD2_9", nodes_by_fqn,         // 
		    "privilege.role.x.superuser.2", NULL);
	requireDeps("DD2_10", nodes_by_fqn,         // 10
		    "endfallback.privilege.role.x.superuser.2", 
		    "drop.table.cluster.ownedbyx", NULL);

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


START_TEST(fallback)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	eval("(setq dbver (version '8.4'))");
	//showMalloc(642);
	//showFree(5641);
	eval("(setq build t)");
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource_fallback.xml");
	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps("F_1", nodes_by_fqn, "privilege.role.x.superuser.1", 
		    "endfallback.privilege.role.x.superuser.2",
		    "role.x", "database.x", NULL);
	requireDeps("F_2", nodes_by_fqn, "table.x.public.x", 
		    "drop.table.x.public.x", "schema.x.public", 
		    "tablespace.pg_default", "column.x.public.x.x", 
		    "role.x", "privilege.role.x.superuser.1", NULL);
	requireDeps("F_3", nodes_by_fqn, "grant.table.x.public.x.trigger",
		    "drop.grant.table.x.public.x.trigger", 
		    "table.x.public.x", "role.x",
		    "grant.schema.x.public.usage", NULL);

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
    eval("(setq dbver (version '8.4'))");
    //showFree(4247);
    //showMalloc(1390);

    eval("(setq build t)");
    eval("(setq drop t)");
    BEGIN {
	doc = getDoc("test/data/cond_test_with_deps.xml");
	nodes = dagFromDoc(doc);

	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);

	/* Require create priv on the schema to create the table but not
	 * to drop it. */
	requireDep("C_1", nodes_by_fqn, "table.regressdb.public.thing", 
		   "grant.schema.regressdb.public.create:public");
	requireNoDep("C_2", nodes_by_fqn, 
		     "drop.grant.schema.regressdb.public.create:public",
		     "drop.table.regressdb.public.thing");

	/* Require superuser to drop the table but not to create it. */
	requireDep("C_3", nodes_by_fqn, 
		   "endfallback.privilege.role.wibble.superuser.1",
		   "drop.table.regressdb.public.thing");

	requireNoDep("C_4", nodes_by_fqn, 
		     "table.regressdb.public.thing", 
		     "privilege.role.wibble.superuser.1");

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
	//dbgSexp(doc);

	nodes = dagFromDoc(doc);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);
	
	if (hasDeps("CB_0", nodes_by_fqn, "view.skittest.public.v3", 
		    "viewbase.skittest.public.v1", NULL)) 
	{
	    // V3 --> VIEWBASE 1
	    fprintf(stderr, "cyclic_build: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
	    requireDeps("CB_1", nodes_by_fqn, "view.skittest.public.v1", 
			"view.skittest.public.v2",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("CB_2", nodes_by_fqn, "view.skittest.public.v2", 
			"view.skittest.public.v3",
			"schema.skittest.public", "role.marc",
			 "privilege.role.marc.superuser", NULL);
	    requireDeps("CB_3", nodes_by_fqn, 
			"viewbase.skittest.public.v1", 
			"schema.skittest.public", "role.marc", 
			"privilege.role.marc.superuser", NULL);
	}
	else if (hasDeps("CB_4", nodes_by_fqn, "view.skittest.public.v2", 
			 "viewbase.skittest.public.v3", NULL)) 
	{
	    // V2 --> VIEWBASE 3
	    requireDeps("CB_5", nodes_by_fqn, "view.skittest.public.v3", 
			"view.skittest.public.v1",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("CB_6", nodes_by_fqn, "view.skittest.public.v1", 
			"view.skittest.public.v2",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("CB_7", nodes_by_fqn, 
			"viewbase.skittest.public.v3", 
			"schema.skittest.public", "role.marc", 
			"privilege.role.marc.superuser", NULL);
	}
	else if (hasDeps("CB_8", nodes_by_fqn, "view.skittest.public.v1", 
			 "viewbase.skittest.public.v2", NULL))
	{
	    // V1 --> VIEWBASE 2
	    fprintf(stderr, "cyclic_build: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
	    requireDeps("CB_9", nodes_by_fqn, "view.skittest.public.v2", 
			"view.skittest.public.v3",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("CB_A", nodes_by_fqn, "view.skittest.public.v3", 
			"view.skittest.public.v1",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("CB_B", nodes_by_fqn, 
			"viewbase.skittest.public.v2", 
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
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

	if (hasDeps("CD_1", nodes_by_fqn, "drop.view.skittest.public.v1",
		     "drop.viewbase.skittest.public.v3", NULL))
	{
	    requireDeps("CD_2", nodes_by_fqn, 
			"drop.view.skittest.public.v1",
			"drop.viewbase.skittest.public.v3", NULL);
	    requireDeps("CD_3", nodes_by_fqn, "drop.view.skittest.public.v2",
			"drop.view.skittest.public.v1", NULL);
	    requireDeps("CD_4", nodes_by_fqn, "drop.view.skittest.public.v3",
			"drop.view.skittest.public.v2", NULL);

	    requireDeps("CD_5", nodes_by_fqn, "drop.schema.skittest.public", 
			"drop.view.skittest.public.v1",
			"drop.view.skittest.public.v2",
			"drop.view.skittest.public.v3",
			"drop.viewbase.skittest.public.v3", 
			"drop.grant.schema.skittest.public.usage:public:regress", 
			NULL);
	}
	else if (hasDeps("CD_6", nodes_by_fqn, 
			 "drop.viewbase.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL))
	{
	    fprintf(stderr, "cyclic_drop: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
	}
	else if (hasDeps("CD_B", nodes_by_fqn, 
			 "drop.viewbase.skittest.public.v1",
			 "drop.view.skittest.public.v3", NULL))
	{
	    fprintf(stderr, "cyclic_drop: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
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


	if (hasDeps("C2_0", nodes_by_fqn, "view.skittest.public.v3", 
		    "viewbase.skittest.public.v1", NULL)) 
	{
	    // V3 --> VIEWBASE 1
	    fprintf(stderr, "cyclic_build: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
	    requireDeps("C2_1", nodes_by_fqn, "view.skittest.public.v1", 
			"view.skittest.public.v2",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("C2_2", nodes_by_fqn, "view.skittest.public.v2", 
			"view.skittest.public.v3",
			"schema.skittest.public", "role.marc",
			 "privilege.role.marc.superuser", NULL);
	    requireDeps("C2_3", nodes_by_fqn, 
			"viewbase.skittest.public.v1", 
			"schema.skittest.public", "role.marc", 
			"privilege.role.marc.superuser", NULL);
	}
	else if (hasDeps("C2_5", nodes_by_fqn, "view.skittest.public.v2", 
			 "viewbase.skittest.public.v3", NULL)) 
	{
	    // V2 --> VIEWBASE 3
	    requireDeps("C2_6", nodes_by_fqn, "view.skittest.public.v3", 
			"view.skittest.public.v1",
			"schema.skittest.public", 
			"drop.view.skittest.public.v3", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("C2_7", nodes_by_fqn, "view.skittest.public.v1", 
			"view.skittest.public.v2",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", 
			"drop.view.skittest.public.v1", NULL);
	    requireDeps("C2_8", nodes_by_fqn, 
			"viewbase.skittest.public.v3", 
			"schema.skittest.public", "role.marc", 
			"privilege.role.marc.superuser", 
			"drop.viewbase.skittest.public.v3", NULL);
	    requireDeps("C2_9", nodes_by_fqn, 
			"view.skittest.public.v2", 
			"drop.view.skittest.public.v2",
			"schema.skittest.public", "role.marc", 
			"privilege.role.marc.superuser", 
			"viewbase.skittest.public.v3", NULL);
	}
	else if (hasDeps("C2_A", nodes_by_fqn, "view.skittest.public.v1", 
			 "viewbase.skittest.public.v2", NULL))
	{
	    // V1 --> VIEWBASE 2
	    fprintf(stderr, "cyclic_build: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
	    requireDeps("C2_B", nodes_by_fqn, "view.skittest.public.v2", 
			"view.skittest.public.v3",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("C2_C", nodes_by_fqn, "view.skittest.public.v3", 
			"view.skittest.public.v1",
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	    requireDeps("C2_D", nodes_by_fqn, 
			"viewbase.skittest.public.v2", 
			"schema.skittest.public", "role.marc",
			"privilege.role.marc.superuser", NULL);
	}
	else {
	    fail("No build-side cycle breaker found");
	}

	if (hasDeps("C2_G", nodes_by_fqn, "drop.view.skittest.public.v1",
		     "drop.viewbase.skittest.public.v3", NULL))
	{
	    requireDeps("C2_H", nodes_by_fqn, 
			"drop.view.skittest.public.v1",
			"drop.viewbase.skittest.public.v3", NULL);
	    requireDeps("C2_J", nodes_by_fqn, "drop.view.skittest.public.v2",
			"drop.view.skittest.public.v1", NULL);
	    requireDeps("C2_K", nodes_by_fqn, "drop.view.skittest.public.v3",
			"drop.view.skittest.public.v2", NULL);

	    requireDeps("C2_L", nodes_by_fqn, "drop.schema.skittest.public", 
			"drop.view.skittest.public.v1",
			"drop.view.skittest.public.v2",
			"drop.view.skittest.public.v3",
			"drop.viewbase.skittest.public.v3", 
			"drop.grant.schema.skittest.public.usage:public:regress", 
			NULL);
	}
	else if (hasDeps("C2_M", nodes_by_fqn, 
			 "drop.viewbase.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL))
	{
	    fprintf(stderr, "cyclic_drop: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
	}
	else if (hasDeps("C2_S", nodes_by_fqn, 
			 "drop.viewbase.skittest.public.v1",
			 "drop.view.skittest.public.v3", NULL))
	{
	    fprintf(stderr, "cyclic_drop: "
		    "THIS TESTSET HAS NOT BEEN VERIFIED\n");
	}
	else {
	    fail("No drop-side cycle breaker found");
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


START_TEST(cyclic_exception)
{
    Document *volatile doc = NULL;
    boolean failed = FALSE;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initTemplatePath(".");
	eval("(setq dbver (version '8.4'))");
	//showMalloc(727);
	//showFree(724);

	eval("(setq build t)");
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource3.xml");
	nodes = dagFromDoc(doc);

	//nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
	fail("Cycle exception expected but did not happen");
    }
    EXCEPTION(ex);
    WHEN(TSORT_CYCLIC_DEPENDENCY) {
	objectFree((Object *) nodes_by_fqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
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
	fail("dependency handler fails with exception");
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
	//showMalloc(50444);
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

	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);

	/* This is hardly an exhaustive set of deps, but it should
	   suffice for now. */
	requireDep("DD1", nodes_by_fqn, "diff.role.regress", 
		   "diffprep.role.regress");
	requireDep("DD2", nodes_by_fqn, 
		     "diff.tablespace.tbs2", 
		     "diffprep.tablespace.tbs2");
	requireNoDep("DD3", nodes_by_fqn, "diffprep.role.regress",
	           "diffprep.tablespace.tbs2");
	requireNoDep("DD4", nodes_by_fqn, "diffprep.role.keep",
	           "diffprep.tablespace.tbs2");
	
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


static Document *
creatediffs(char *path1, char *path2)
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


/* See comments in test/data/depdiffs_1a.sql */
static void
dd1_testcase_1(Hash *nodes_by_fqn)
{
    requireDeps("DD_TC1_1", nodes_by_fqn, 
		"diff.sequence.regressdb.n1.s1", 
		"diffprep.sequence.regressdb.n1.s1", 
		"diff.schema.regressdb.n1", NULL);
    requireDeps("DD_TC1_2", nodes_by_fqn, 
		"drop.sequence.regressdb.n1.s1a", 
		"drop.grant.sequence.regressdb.n1.s1a.usage:r1", 
		"drop.grant.sequence.regressdb.n1.s1a.update:r1", 
		"drop.grant.sequence.regressdb.n1.s1a.select:r1", 
		NULL);
    requireDeps("DD_TC1_3", nodes_by_fqn, 
		"build.sequence.regressdb.n1.s1b",
		"diff.schema.regressdb.n1", NULL);
}

/* See comments in test/data/depdiffs_1a.sql */
static void
dd1_testcase_2(Hash *nodes_by_fqn)
{
    requireDeps("DD_TC1_1", nodes_by_fqn, 
		"diff.sequence.regressdb.n1.s1", 
		"diffprep.sequence.regressdb.n1.s1", 
		"diff.schema.regressdb.n1", NULL);
}

/* Dependency diff tests. */
START_TEST(depdiffs_1)
{
    Document *volatile diffs = NULL;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    initTemplatePath(".");
    diffs = creatediffs("test/data/depdiffs_1a.xml",
			"test/data/depdiffs_1b.xml");

    //dbgSexp(diffs);

    BEGIN {
	nodes = dagFromDoc(diffs);
	nodes_by_fqn = dagnodeHash(nodes);
	//fprintf(stderr, "\n============FINAL==============\n");
	//showVectorDeps(nodes);

	/* See comments in test/data/depdiffs_1a.sql for a description
	   of each test case. */
	dd1_testcase_1(nodes_by_fqn);
	dd1_testcase_2(nodes_by_fqn);
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



START_TEST(general_diffs)
{
    Document *volatile diffs = NULL;
    Vector *volatile nodes = NULL;
    Hash *volatile nodes_by_fqn = NULL;

    initTemplatePath(".");
    diffs = creatediffs("test/data/gendiffs_1a.xml",
			"test/data/gendiffs_1b.xml");

    //dbgSexp(diffs);

    BEGIN {
	showMalloc(12452);
	nodes = dagFromDoc(diffs);
	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);
	requireDep("GD_1a", nodes_by_fqn, 
		   "build.function.regressdb.mysum(pg_catalog.int4)",
		   "drop.function.regressdb.mysum(pg_catalog.int4)");
	requireDep("GD_1b", nodes_by_fqn, 
		   "build.function.regressdb.mysum(pg_catalog.int4)",
		    "build.function.regressdb.public"
		   ".addnint4(pg_catalog.int4,pg_catalog.int4)");
	requireDep("GD_2", nodes_by_fqn, 
		   "drop.function.regressdb.public"
		   ".addint4(pg_catalog.int4,pg_catalog.int4)",
		   "drop.function.regressdb.mysum(pg_catalog.int4)");

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



Suite *
deps_suite(void)
{
    Suite *s = suite_create("deps");
    TCase *tc_core = tcase_create("deps");

    ADD_TEST(tc_core, deps_basic);
    ADD_TEST(tc_core, deps_simple_build);
    ADD_TEST(tc_core, deps_simple_drop);
    ADD_TEST(tc_core, deps_simple_rebuild);
    ADD_TEST(tc_core, depset_simple_build);
    ADD_TEST(tc_core, depset_simple_drop);
    ADD_TEST(tc_core, depset_simple_rebuild);
    ADD_TEST(tc_core, depset_dag1_build);
    ADD_TEST(tc_core, depset_dag1_drop);
    ADD_TEST(tc_core, depset_dag1_both);
    ADD_TEST(tc_core, depset_dia_build);
    ADD_TEST(tc_core, depset_dia_drop);
    ADD_TEST(tc_core, depset_dia_both);
    ADD_TEST(tc_core, fallback);
    ADD_TEST(tc_core, cond);
    ADD_TEST(tc_core, cyclic_build);
    ADD_TEST(tc_core, cyclic_drop);
    ADD_TEST(tc_core, cyclic_both);
    ADD_TEST(tc_core, cyclic_exception);
    ADD_TEST(tc_core, depset_diff);
    ADD_TEST(tc_core, depdiffs_1);
    ADD_TEST(tc_core, general_diffs);
    // For debugging regression tests
    // ADD_TEST(tc_core, rt3);  /* Diff from regression_test_3 */

    /* Add tests for:
     *   degrade_if_missing
     *   multiple cycles through one node (ie breaker re-use)
     *   exists node dependencies being eliminated
     *   rebuild promotion
     *   diff node deps optimisations
     *   fallback node optimisations:
     *     if multiple fallbacks exist eliminate the lesser priv
     *     if fallbacks on both sides of the dag, eliminate superfluous
     *       endfallback->fallback in the middle.
     *   any new functionality added from TODO
     */
     
    suite_add_tcase(s, tc_core);

    return s;
}
