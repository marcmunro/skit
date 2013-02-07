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
    DogNode *node;
    String *key;
    Object *old;
    Hash *hash = hashNew(TRUE);

    EACH(vector, i) {
	node = (DogNode *) ELEM(vector, i);
	assert(node->type == OBJ_DOGNODE, "Incorrect node type");
	key = stringDup(node->fqn);

	if (hashGet(hash, (Object *) key)) {
	    objectFree((Object *) key, TRUE);
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

/*
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
*/

 /*
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
 */

  /*
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
  */
#ifdef wibble
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
#endif

#ifdef wibble
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
    else {
	objectFree((Object *) key, FALSE);
	
	dep_elems = (fromnode->dependencies)? fromnode->dependencies->elems: 0;
	
	if (dep_elems != count) {
	    fail("Not all dependencies accounted for in %s (expecting %d got %d)", 
		 fromnode->fqn->value, count, fromnode->dependencies->elems);
	}
    }
}
#endif

static boolean
hasDependency2(DogNode *node, DogNode *dep)
{
    Vector *vector = node->forward_deps;
    Object *depobj;
    int i;
    if (vector) {
	for (i = 0; i < vector->elems; i++) {
	    depobj = vector->contents->vector[i];
	    if (depobj->type == OBJ_DOGNODE) {
		if (dep == (DogNode *) depobj) {
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
    char *prefix;
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
    case UNSPECIFIED_NODE: return 0;
    default:
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("Unhandled build type %d in buildPrefixLen", 
		     (int) type));
    }
}

static DogNode *
findDagNode(Hash *hash, char *name)
{
    DagNodeBuildType type = buildTypeFromName(name);
    String *fqn = stringNew(name);
    DogNode *node = (DogNode *) hashGet(hash, (Object *) fqn);

    if (!node) {
	/* Try removing any build type prefix from the name and see if
	 * there is a match that way. */
	objectFree((Object *) fqn, TRUE);
	fqn = stringNew(name + buildPrefixLen(type));
	node = (DogNode *) hashGet(hash, (Object *) fqn);
    }

    if (node) {
	if (type != UNSPECIFIED_NODE) {
	    if (node->build_type != type) {
		node = node->mirror_node;
	    }
	    if (node) {
		objectFree((Object *) fqn, TRUE);
		fqn = NULL;
		assert(node->build_type == type,
		       "Cannot find node for %s", name); 
	    }
	}
    }
    objectFree((Object *) fqn, TRUE);
    return node;
}

static boolean
hasDep2(Hash *hash, char *from, char *to)
{
    DogNode *fnode = (DogNode *) findDagNode(hash, from);
    DogNode *tnode = (DogNode *) findDagNode(hash, to);

    if (!fnode) {
	fail("Cannot find source node %s ", from);
    }

    if (!tnode) {
	fail("Cannot find dependency %s ", to);
    }
    
    return hasDependency2(fnode, tnode);
}

static void
requireDep2(Hash *hash, char *from, char *to)
{
    if (!hasDep2(hash, from, to)) {
	fail("No dep exists from %s to %s", from, to);
    }
}

static void
requireDeps2(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DogNode *fromnode;
    int dep_elems;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	requireDep2(hash, from, to);
	count++;
    }
    va_end(params);
    fromnode = (DogNode *) findDagNode(hash, from);

    if (!fromnode) {
	fail("Cannot find %s ", from);
    }
    else {
	dep_elems = (fromnode->forward_deps)? fromnode->forward_deps->elems: 0;
	
	if (dep_elems != count) {
	    fail("Not all dependencies accounted for in %s (expecting %d got %d)", 
		 fromnode->fqn->value, count, fromnode->forward_deps->elems);
	}
    }
}

static boolean
chkDep2(Hash *hash, char *from, char *to)
{
    DogNode *fnode = (DogNode *) findDagNode(hash, from);
    DogNode *tnode = (DogNode *) findDagNode(hash, to);

    if (!fnode) {
	return FALSE;
    }

    if (!tnode) {
	return FALSE;
    }
    
    return hasDependency2(fnode, tnode);
}

static boolean
hasDeps2(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DogNode *fromnode;
    int dep_elems;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	if (!chkDep2(hash, from, to)) {
	    return FALSE;
	}
	count++;
    }
    va_end(params);
    return TRUE;
    fromnode = (DogNode *) findDagNode(hash, from);

    if (!fromnode) {
	fail("Cannot find %s ", from);
    }
    else {
	dep_elems = (fromnode->forward_deps)? fromnode->forward_deps->elems: 0;
	
	if (dep_elems != count) {
	    return FALSE;
	}
    }
    return TRUE;
}

#ifdef wibble
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
#endif

#ifdef wibble
static void
requireOptionalDependencies(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *fromnode;
    DagNode *tonode;
    String *key;
    int dep_elems = 0;
    Vector *deplist = vectorNew(10);
    int i;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	vectorPush(deplist, (Object*) stringNew(to));
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
    EACH(deplist, i) {
	key = (String *) ELEM(deplist, i);
	tonode = (DagNode *) hashGet(hash, (Object *) key);
	if (hasDependency(fromnode, tonode)) {
	    objectFree((Object *) deplist, TRUE);
	    return;
	}
    }

    objectFree((Object *) deplist, TRUE);
	
    fail("No optional dependencies found in %s", fromnode->fqn->value);
}
#endif

static void
requireOptionalDependencies2(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DogNode *fromnode;
    DogNode *tonode;
    String *key;
    int dep_elems = 0;
    Vector *deplist = vectorNew(10);
    int i;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	vectorPush(deplist, (Object*) stringNew(to));
	count++;
    }
    va_end(params);
    fromnode = (DogNode *) findDagNode(hash, from);
    if (!fromnode) {
	fail("Cannot find %s ", from);
    }

    dep_elems = (fromnode->forward_deps)? fromnode->forward_deps->elems: 0;
    EACH(deplist, i) {
	key = (String *) ELEM(deplist, i);
	tonode = (DogNode *) findDagNode(hash, key->value);
	if (hasDependency2(fromnode, tonode)) {
	    objectFree((Object *) deplist, TRUE);
	    return;
	}
    }

    objectFree((Object *) deplist, TRUE);
	
    fail("No optional dependencies found in %s", fromnode->fqn->value);
}

#ifdef wibble
static void
requireOptionalDependents(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DagNode *tonode;
    DagNode *fromnode;
    String *key;
    int dep_elems = 0;
    Vector *deplist = vectorNew(10);
    int i;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	vectorPush(deplist, (Object*) stringNew(to));
	count++;
    }
    va_end(params);
    key = stringNewByRef(from);
    tonode = (DagNode *) hashGet(hash, (Object *) key);
    if (!tonode) {
	fail("Cannot find %s ", key->value);
    }

    objectFree((Object *) key, FALSE);
    dep_elems = (tonode->dependencies)? tonode->dependencies->elems: 0;
    EACH(deplist, i) {
	key = (String *) ELEM(deplist, i);
	fromnode = (DagNode *) hashGet(hash, (Object *) key);
	if (hasDependency(fromnode, tonode)) {
	    objectFree((Object *) deplist, TRUE);
	    return;
	}
    }
    objectFree((Object *) deplist, TRUE);
	
    fail("No optional dependents found in %s", tonode->fqn->value);
}
#endif

static void
requireOptionalDependents2(Hash *hash, char *from, ...)
{
    va_list params;
    char *to;
    int count = 0;
    DogNode *tonode;
    DogNode *fromnode;
    String *key;
    int dep_elems = 0;
    Vector *deplist = vectorNew(10);
    int i;

    va_start(params, from);
    while (to = va_arg(params, char *)) {
	vectorPush(deplist, (Object*) stringNew(to));
	count++;
    }
    va_end(params);
    tonode = (DogNode *) findDagNode(hash, from);
    if (!tonode) {
	fail("Cannot find %s ", key->value);
    }

    dep_elems = (tonode->forward_deps)? tonode->forward_deps->elems: 0;
    EACH(deplist, i) {
	key = (String *) ELEM(deplist, i);
	fromnode = (DogNode *) findDagNode(hash, key->value);
	if (hasDependency2(fromnode, tonode)) {
	    objectFree((Object *) deplist, TRUE);
	    return;
	}
    }
    objectFree((Object *) deplist, TRUE);
	
    fail("No optional dependents found in %s", tonode->fqn->value);
}



#ifdef wibble
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
#endif

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


/* Test basic build dependencies, using dependency sets */ 
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
	//showMalloc(654);
	//showFree(724);

	eval("(setq build t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = dagFromDoc(doc);
	//dbgSexp(nodes);

	//nodes = nodesFromDoc(doc);
	//prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);

	requireDeps2(nodes_by_fqn, "role.cluster.r1", "role.cluster.r3", NULL);
	requireDeps2(nodes_by_fqn, "role.cluster.r2", "role.cluster.r3", NULL);

	requireOptionalDependencies2(nodes_by_fqn, "role.cluster.r3", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r4", NULL);

	requireOptionalDependencies2(nodes_by_fqn, "role.cluster.r4", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r5", NULL);

	requireDeps2(nodes_by_fqn, "role.cluster.r5", NULL);



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
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(23);
	//showFree(724);

	eval("(setq drop t)");
	doc = getDoc("test/data/gensource_depset.xml");
	nodes = dagFromDoc(doc);
	//nodes = nodesFromDoc(doc);
	//prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);

	requireOptionalDependents2(nodes_by_fqn, "role.cluster.r3", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r4", NULL);

	requireOptionalDependents2(nodes_by_fqn, "role.cluster.r4", 
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
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(901);
	//showFree(415);

	eval("(setq drop t)");
	eval("(setq build t)");

	doc = getDoc("test/data/gensource_depset.xml");
	nodes = dagFromDoc(doc);
	//nodes = nodesFromDoc(doc);
	//prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	nodes_by_fqn = dagnodeHash(nodes);

	requireDeps2(nodes_by_fqn, "role.cluster.r1", 
		    "drop.role.cluster.r1", "role.cluster.r3", NULL);
	requireDeps2(nodes_by_fqn, "role.cluster.r2", 
		    "drop.role.cluster.r2", "role.cluster.r3", NULL);

	requireDep2(nodes_by_fqn, "role.cluster.r3", "drop.role.cluster.r3");
	requireOptionalDependents2(nodes_by_fqn, "role.cluster.r3", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r4", NULL);

	requireDep2(nodes_by_fqn, "role.cluster.r4", "drop.role.cluster.r4");
	requireOptionalDependencies2(nodes_by_fqn, "role.cluster.r4", 
			    "role.cluster.r1", "role.cluster.r2", 
			    "role.cluster.r5", NULL);

	requireDeps2(nodes_by_fqn, "role.cluster.r5", 
		    "drop.role.cluster.r5", NULL);

	/* No guaranteed deps for drop r1 */
	/* No guaranteed deps for drop r2 */

	requireDep2(nodes_by_fqn, "drop.role.cluster.r3", 
		   "drop.role.cluster.r1");
	requireDep2(nodes_by_fqn, "drop.role.cluster.r3", 
		   "drop.role.cluster.r2");
	requireOptionalDependents2(nodes_by_fqn, "drop.role.cluster.r3", 
			    "drop.role.cluster.r1", "drop.role.cluster.r2", 
			    "drop.role.cluster.r4", NULL);

	/* No guaranteed deps for drop r4 */
	requireOptionalDependents2(nodes_by_fqn, "drop.role.cluster.r4", 
			    "drop.role.cluster.r1", "drop.role.cluster.r2", 
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

#ifdef wibble
START_TEST(depset_diff)
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
	eval("(setq build t)");
	doc = getDoc("test/data/diffstream1.xml");
	nodes = nodesFromDoc(doc);

	prepareDagForBuild((Vector **) &nodes);
	//showVectorDeps(nodes);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) doc, TRUE);
    }
    END;

    FREEMEMWITHCHECK;
    if (failed) {
	fail("gensort fails with exception");
    }
}
END_TEST
#endif


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
	//showMalloc(1395);
	//showFree(1504);
	eval("(setq build t)");
	doc = getDoc("test/data/gensource2.xml");
	//nodes = nodesFromDoc(doc);
	nodes = dagFromDoc(doc);
	//prepareDagForBuild((Vector **) &nodes);
	//nodes_by_fqn = hashByFqn(nodes);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);
	
	if (hasDeps2(nodes_by_fqn, "view.skittest.public.v3", 
		     "build.viewbase.skittest.public.v1",
		     "schema.skittest.public", "role.cluster.marc",
		     "privilege.cluster.marc.superuser", NULL)) 
	{
	    // V3 --> VIEWBASE 1
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v1", 
			 "build.view.skittest.public.v2",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v2", 
			 "build.view.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps2(nodes_by_fqn, "build.viewbase.skittest.public.v1", 
			 "schema.skittest.public", "role.cluster.marc", NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "view.skittest.public.v2", 
		     "build.viewbase.skittest.public.v3",
		     "schema.skittest.public", "role.cluster.marc",
			  "privilege.cluster.marc.superuser", NULL)) 
	{
	    // V2 --> VIEWBASE 3
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v3", 
			 "build.view.skittest.public.v1",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v1", 
			 "build.view.skittest.public.v2",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps2(nodes_by_fqn, "build.viewbase.skittest.public.v3", 
			 "schema.skittest.public", "role.cluster.marc", NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "view.skittest.public.v2", 
		     "build.viewbase.skittest.public.v3",
		     "schema.skittest.public", "role.cluster.marc",
		     "privilege.cluster.marc.superuser", NULL))
	{
	    // V1 --> VIEWBASE 2
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v2", 
			 "build.view.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v3", 
			 "build.view.skittest.public.v1",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", NULL);
	    requireDeps2(nodes_by_fqn, "build.viewbase.skittest.public.v2", 
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
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(813);
	//showFree(572);
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource2.xml");
	nodes = dagFromDoc(doc);
	//nodes = nodesFromDoc(doc);

	//prepareDagForBuild((Vector **) &nodes);
	//nodes_by_fqn = hashByFqn(nodes);
	
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	if (hasDeps2(nodes_by_fqn, "drop.viewbase.skittest.public.v1",
		     "drop.view.skittest.public.v3", NULL))
	{
	    // V3 <-- VIEWBASE 1
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v3",
			 "drop.view.skittest.public.v2", NULL);
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL);

	    requireDeps2(nodes_by_fqn, "drop.schema.skittest.public", 
			 "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v3",
			 "drop.viewbase.skittest.public.v1", 
			 "drop.grant.skittest.public.usage:public:regress", 
			 NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "drop.viewbase.skittest.public.v2",
			  "drop.view.skittest.public.v1", NULL))
	{
	    // V1 <-- VIEWBASE 2
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v3", NULL);
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v3",
			 "drop.view.skittest.public.v2", NULL);

	    requireDeps2(nodes_by_fqn, "drop.schema.skittest.public", 
			 "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v3",
			 "drop.viewbase.skittest.public.v2", 
			 "drop.grant.skittest.public.usage:public:regress", 
			 NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "drop.viewbase.skittest.public.v3",
			  "drop.view.skittest.public.v2", NULL))
	{
	    // V2 <-- VIEWBASE 3
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL);
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v3", NULL);

	    requireDeps2(nodes_by_fqn, "drop.schema.skittest.public", 
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
	//nodes = nodesFromDoc(doc);
	nodes = dagFromDoc(doc);

	//prepareDagForBuild((Vector **) &nodes);
	//nodes_by_fqn = hashByFqn(nodes);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);
	if (hasDeps2(nodes_by_fqn, "view.skittest.public.v3", 
		     "build.viewbase.skittest.public.v1",
		     "schema.skittest.public", "role.cluster.marc",
		     "privilege.cluster.marc.superuser", NULL)) 
	{
	    // V3 --> VIEWBASE 1
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v1", 
			 "build.view.skittest.public.v2",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", 
			 "drop.view.skittest.public.v1", NULL);
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v2", 
			 "build.view.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", 
			 "drop.view.skittest.public.v2", NULL);
	    requireDeps2(nodes_by_fqn, "build.viewbase.skittest.public.v1", 
			 "schema.skittest.public", "role.cluster.marc", 
			 "drop.viewbase.skittest.public.v1", NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "view.skittest.public.v2", 
		     "build.viewbase.skittest.public.v3",
		     "schema.skittest.public", "role.cluster.marc",
			  "privilege.cluster.marc.superuser", NULL)) 
	{
	    fail("FAIL 2");
	    // V2 --> VIEWBASE 3
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v3", 
			 "build.view.skittest.public.v1",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", 
			 "drop.view.skittest.public.v3", NULL);
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v1", 
			 "build.view.skittest.public.v2",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", 
			 "drop.view.skittest.public.v1", NULL);
	    requireDeps2(nodes_by_fqn, "build.viewbase.skittest.public.v3", 
			 "schema.skittest.public", "role.cluster.marc", 
			 "drop.viewbase.skittest.public.v3", NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "view.skittest.public.v2", 
		     "build.viewbase.skittest.public.v3",
		     "schema.skittest.public", "role.cluster.marc",
		     "privilege.cluster.marc.superuser", NULL))
	{
	    fail("FAIL 3");
	    // V1 --> VIEWBASE 2
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v2", 
			 "build.view.skittest.public.v3",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", 
			 "drop.view.skittest.public.v2", NULL);
	    requireDeps2(nodes_by_fqn, "view.skittest.public.v3", 
			 "build.view.skittest.public.v1",
			 "schema.skittest.public", "role.cluster.marc",
			 "privilege.cluster.marc.superuser", 
			 "drop.view.skittest.public.v3", NULL);
	    requireDeps2(nodes_by_fqn, "build.viewbase.skittest.public.v2", 
			 "schema.skittest.public", "role.cluster.marc", 
			 "drop.viewbase.skittest.public.v2", NULL);
	}
	else {
	    fail("No cycle breaker found in build side");
	}

	if (hasDeps2(nodes_by_fqn, "drop.viewbase.skittest.public.v1",
		     "drop.view.skittest.public.v3", NULL))
	{
	    // V3 <-- VIEWBASE 1
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v3",
			 "drop.view.skittest.public.v2", NULL);
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL);

	    requireDeps2(nodes_by_fqn, "drop.schema.skittest.public", 
			 "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v3",
			 "drop.viewbase.skittest.public.v1", 
			 "drop.grant.skittest.public.usage:public:regress", 
			 NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "drop.viewbase.skittest.public.v2",
			  "drop.view.skittest.public.v1", NULL))
	{
	    // V1 <-- VIEWBASE 2
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v3", NULL);
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v3",
			 "drop.view.skittest.public.v2", NULL);

	    requireDeps2(nodes_by_fqn, "drop.schema.skittest.public", 
			 "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v3",
			 "drop.viewbase.skittest.public.v2", 
			 "drop.grant.skittest.public.usage:public:regress", 
			 NULL);
	}
	else if (hasDeps2(nodes_by_fqn, "drop.viewbase.skittest.public.v3",
			  "drop.view.skittest.public.v2", NULL))
	{
	    // V2 <-- VIEWBASE 3
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v1", NULL);
	    requireDeps2(nodes_by_fqn, "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v3", NULL);

	    requireDeps2(nodes_by_fqn, "drop.schema.skittest.public", 
			 "drop.view.skittest.public.v1",
			 "drop.view.skittest.public.v2",
			 "drop.view.skittest.public.v3",
			 "drop.viewbase.skittest.public.v3", 
			 "drop.grant.skittest.public.usage:public:regress", 
			 NULL);
	}
	else {
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
    char *xnode_name;
    Hash *volatile nodes_by_fqn = NULL;

    BEGIN {
	initBuiltInSymbols();
	initTemplatePath(".");
	//showMalloc(642);
	//showFree(5641);
	eval("(setq build t)");
	eval("(setq drop t)");
	doc = getDoc("test/data/gensource_fallback.xml");
	//doc = getDoc("tmp.xml");
	nodes = dagFromDoc(doc);
	//nodes = nodesFromDoc(doc);


	//prepareDagForBuild((Vector **) &nodes);
	//nodes_by_fqn = hashByFqn(nodes);
	nodes_by_fqn = dagnodeHash(nodes);
	//showVectorDeps(nodes);

	requireDeps2(nodes_by_fqn, "fallback.grant.x.superuser", 
		    "role.cluster.x", NULL);
	requireDeps2(nodes_by_fqn, "table.x.public.x", 
		    "role.cluster.x","schema.x.public", 
		    "tablespace.cluster.pg_default", 
		    "fallback.grant.x.superuser", 
	             "drop.table.x.public.x", NULL);
	requireDeps2(nodes_by_fqn, "grant.x.public.x.trigger:x:x",
		    "table.x.public.x", "role.cluster.x",
		    "fallback.grant.x.superuser", 
		     "drop.grant.x.public.x.trigger:x:x", NULL);
	requireDeps2(nodes_by_fqn, "endfallback.fallback.grant.x.superuser", 
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


#ifdef wibble
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
#endif

#ifdef wibble
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
#endif

Suite *
deps_suite(void)
{
    Suite *s = suite_create("deps");
    TCase *tc_core = tcase_create("deps");

    ADD_TEST(tc_core, build_type_bitsets);
    ADD_TEST(tc_core, depset_dag1_build);
    ADD_TEST(tc_core, depset_dag1_drop);
    ADD_TEST(tc_core, depset_dag1_both);
    //ADD_TEST(tc_core, depset_diff);
    ADD_TEST(tc_core, cyclic_build);
    ADD_TEST(tc_core, cyclic_drop);
    ADD_TEST(tc_core, cyclic_both);
    //ADD_TEST(tc_core, cond);

    ADD_TEST(tc_core, fallback);
				
    //ADD_TEST(tc_core, cyclic_build2);
    suite_add_tcase(s, tc_core);

    return s;
}
