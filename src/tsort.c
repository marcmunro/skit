/**
 * @file   tsort.c
 * \code
 *     Copyright (c) 2010, 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for performing topological sorts.
 */

#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"




#ifdef wibble
// Old version of smart tsort follows

/* hashEach function to append nodes to a vector, creating a vector from
 * a hash of nodes. */
static Object *
appendToVec(Cons *node_entry, Object *results)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Vector *vector = (Vector *) results;
    String *parent_name;

    assert(node->type == OBJ_DAGNODE, "Node is not a dagnode");
    vectorPush(vector, (Object *) node);
    return (Object *) node;
}

/* Create a vector from a hash of nodes. */
static Vector *
nodeList(Hash *allnodes)
{
    int elems;
    Vector *list;
    elems = hashElems(allnodes);
    list = vectorNew(elems);
    hashEach(allnodes, &appendToVec, (Object *) list);

    return list;
}


/* Comparison function for qsort, for sorting by fqn.
 */
static int 
fqnCmp(const void *item1, const void *item2)
{
    DagNode *node1 = *((DagNode **) item1);
    DagNode *node2 = *((DagNode  **) item2);
    assert(node1 && node1->type == OBJ_DAGNODE,
	   newstr("fqnCmp: node1 is not a dagnode (%d)", node1->type));
    assert(node2 && node2->type == OBJ_DAGNODE,
	   newstr("fqnCmp: node2 is not a dagnode (%d)", node2->type));
    return strcmp(node1->fqn->value, node2->fqn->value);
}


/* Maintain an ordered, cyclic list of DagNode siblings */
static void
linkToSibling(DagNode *first, DagNode *node)
{
    if (first->next) {
	node->prev = first->prev;
	first->prev->next = node;
    }
    else {
	first->next = node;
	node->prev = first;
    }
    node->next = first;
    first->prev = node;
}

/* Create linkages between parent and child in a DagNode tree. */
static void
linkToParent(DagNode *node)
{
    DagNode *parent = node->parent;
    DagNode *first_sib = parent->kids;
    if (first_sib) {
	linkToSibling(first_sib, node);
    }
    else {
	parent->kids = node;
    }
}

/* Create a sorted tree, reflecting the hierarchy of DagNodes, and
 * intitialise the status and buildable_kids counts.  At each
 * level of the tree, the nodes are sorted in fqn order.
 */
static DagNode *
initDagNodeTree(Hash *allnodes)
{
    Vector *nodelist = nodeList(allnodes);
    DagNode *root = NULL;
    DagNode *node;
    int i;

    /* Make a vector of the contents of allnodes, sorted by fqn. */
    qsort((void *) nodelist->contents->vector,
	  nodelist->elems, sizeof(Object *), fqnCmp);

    /* Now initialise and add each node into it's rightful place in the
     * tree */
    for (i = 0; i < nodelist->elems; i++) {
	node = (DagNode *) nodelist->contents->vector[i];
	node->status = UNBUILDABLE;
	node->buildable_kids = 0;
	if (node->parent) {
	    linkToParent(node);
	}
	else {
	    if (root) {
		linkToSibling(root, node);
	    }
	    else {
		root = node;
	    }
	}
    }
    objectFree((Object *) nodelist, FALSE);

    return root;
}

static Object *
addCandidateToBuild(Cons *node_entry, Object *results)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Vector *vector = (Vector *) results;
    String *parent_name;

    assert(node->type == OBJ_DAGNODE, "Node is not a dagnode");
    if ((node->status == UNBUILDABLE) && !node->dependencies) {
	vectorPush(vector, (Object *) node);
    }
    return (Object *) node;
}

/* Return a vector of all nodes without dependencies */
static Vector *
getBuildCandidates(Hash *allnodes)
{
    int elems = hashElems(allnodes);
    Vector *volatile results = vectorNew(elems);
    BEGIN {
	hashEach(allnodes, &addCandidateToBuild, (Object *) results);
    }
    EXCEPTION(ex) {
	objectFree((Object *) results, FALSE);
    }
    END
    return results;
}


/* Mark this node as buildable, and update the counts of buildable_kids
 * in all ancestors. */
static void
markAsBuildable(DagNode *node)
{
    DagNode *up = node->parent;
    node->status = BUILDABLE;
    while (up) {
	up->buildable_kids++;
	up = up->parent;
    }
}

static void
markBuildCandidates(Hash *allnodes)
{
    Vector *candidates = getBuildCandidates(allnodes);
    DagNode *next;
    int i;

    for (i = 0; i < candidates->elems; i++) {
	next = (DagNode *) candidates->contents->vector[i];
	markAsBuildable(next);
	(void) hashDel(allnodes, (Object *) next->fqn);
    }
    objectFree((Object *) candidates, FALSE);
}

/* Remove node as a build candidate (after it has been selected for
 * building), taking care of its ancestors' counts of buildable_kids */
static void
markAsSelected(DagNode *node)
{
    DagNode *up = node->parent;
    node->status = SELECTED_FOR_BUILD;
    while (up) {
	up->buildable_kids--;
	up = up->parent;
    }
}

/* Find the next buildable node in the DagNodeTree, from a given starting
 * point.   If we are unable to find a node at or below our starting
 * point, we move up to our parent and look from there.  This means
 * that we may be checking some nodes more than once - that is
 * considered too bad.  */
static DagNode *
nextBuildable(DagNode *node)
{
    DagNode *sibling;
    if (!node) {
	return NULL;
    }
    if (node->status == BUILDABLE) {
	return node;
    }
    if (node->buildable_kids) {
	return nextBuildable(node->kids);
    }
    if (sibling = node->next) {
	while (sibling != node) {
	    if (sibling->status == BUILDABLE) {
		return sibling;
	    }
	    if (sibling->buildable_kids) {
		return nextBuildable(sibling->kids);
	    }
	    sibling = sibling->next;
	}
    }
    return nextBuildable(node->parent);
}

static void
addDependent(DagNode *node, DagNode *dependent)
{
    if (!node->dependents) {
	node->dependents = vectorNew(10);
    }
    setPush(node->dependents, (Object *) dependent);
}

/* HashEach function to add dependents entries as the inverse of
 * dependencies. */
static Object *
addDependents(Cons *node_entry, Object *param)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Hash *allnodes = (Hash *) param;
    Cons *this = node->dependencies;
    Depset *dep;
    while (this) {
	dep = (Depset *) this->car;
	if (dep->actual) {
	    addDependent(dep->actual, node);
	}
	else if (!dep->is_optional) {
	    /* Why/how did this happen? */
	    showDeps(node);
	    RAISE(TSORT_ERROR,
		  newstr("Missing actual dep for node %s", node->fqn->value));
	}
	
	this = (Cons *) this->cdr;
	
    }
    return (Object *) node;
}

static void
removeDependency(DagNode *node, DagNode *dep)
{
    Cons *deps = node->dependencies;
    Cons *prev = NULL;
    Cons *next;
    Depset *depset;
    while (deps) {
	depset = (Depset *) deps->car;
	next = (Cons *) deps->cdr;
	if (depset->actual == dep) {
	    /* Found the depset entry for dep.  Now we remove it from
	     * node->deps */
	    if (prev) {
		/* Unlink from prev */
		prev->cdr = (Object *) next;
	    }
	    else {
		/* Unlink from node->deps */
		node->dependencies = next;
	    }
	    objectFree((Object *) depset, TRUE);
	    objectFree((Object *) deps, FALSE);
	}
	else {
	    prev = deps;
	}
	deps = next;
    }
}

static void
unlinkDependents(DagNode *node, Hash *allnodes)
{
    Vector *dependents = node->dependents;
    DagNode *depnode;
    if (dependents) {
	while (depnode = (DagNode *) vectorPop(dependents)) {
	    removeDependency(depnode, node);
	}
    }
}
static Vector *
smart_tsort(Hash *allnodes)
{
    DagNode *root = initDagNodeTree(allnodes);
    DagNode *next;
    Vector *volatile results;

    hashEach(allnodes, &addDependents, (Object *) allnodes);
    results = vectorNew(hashElems(allnodes));
    BEGIN {
	markBuildCandidates(allnodes);

	next = root;
	while (next = nextBuildable(next)) {
	    (void) vectorPush(results, (Object *) next);
	    markAsSelected(next);
	    unlinkDependents(next, allnodes);
	    markBuildCandidates(allnodes);
	}
	if (hashElems(allnodes)) {
	    char *nodes = objectSexp((Object *) allnodes);
	    char *errmsg = newstr("gensort: unsorted nodes remain:\n\"%s\"\n",
				  nodes);
	    skfree(nodes);
	    showAllDeps(allnodes);
	    RAISE(GENERAL_ERROR, errmsg);
	}
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	RAISE();
    }
    END;

    //showAllDeps(allnodes);
    return results;
}

#endif

#ifdef wibble
static void
tsort_node(Vector *nodes, DagNode *node, Vector *results);

static void
tsort_deps(Vector *nodes, DagNode *node, Vector *results)
{
    Vector *deps;
    int i;
    Dependency *dep;
    DagNode *depnode;

    if (deps = node->dependencies) {
	for (i = 0; i < deps->elems; i++) {
	    dep = (Dependency *) deps->contents->vector[i];
	    depnode = dep->dependency;
	    tsort_node(nodes, depnode, results);
	}
    }
}


static void
tsort_node(Vector *nodes, DagNode *node, Vector *results)
{
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("Cyclic dep found in %s", node->fqn->value), node);
    case UNVISITED: 
    case RESOLVED: 
	BEGIN {
	    node->status = VISITING;
	    tsort_deps(nodes, node, results);
	    vectorPush(results, (Object *) node);
	}
	EXCEPTION(ex) {
	    node->status = UNVISITED;
	    RAISE();
	}
	END;
	node->status = VISITED;
	break;
    case VISITED: 
	break;
    default:
	RAISE(TSORT_ERROR,
		  newstr("Unexpected status for dagnode %s: %d",
			 node->fqn->value, node->status));
    }
}
#endif

/* Create an initial sort of our dagnodes.  This identifies which
 * optional dependencies and fallbacks should be followed, and which
 * cycle-breakers should be employed.  This has two purposes: it turns
 * our list of dagnodes into a true dag, and it gives us an ordering
 * from which to split multi-action nodes (splitting multi-action nodes
 * should be done in dependency order, ie dependent objects before the
 * objects on which they depend).
 */
/*
Vector *
simple_tsort(Vector *nodes)
{
    Vector *volatile results = 
	vectorNew(nodes->elems + 10); // Allow for expansion
    DagNode *node;
    int i;
    BEGIN {
	for (i = 0; i < nodes->elems; i++) {
	    node = (DagNode *) nodes->contents->vector[i];
	    tsort_node(nodes, node, results);
	}
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, FALSE);
	RAISE();
    }
    END;
    return results;
}
*/

/* Do the sort. 
*/
#ifdef wibble
Vector *
gensort(Document *doc)
{
    Vector *volatile nodes = NULL;
    Vector *results = NULL;
    Symbol *simple_sort = symbolGet("simple-sort");

    BEGIN {
	nodes = nodesFromDoc(doc);
	prepareDagForBuild((Vector **) &nodes);
	results = simple_tsort(nodes);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes, TRUE);
	RAISE();
    }
    END;
    objectFree((Object *) nodes, FALSE);
    return results;
}
#endif

static void
tsort2_node(Vector *nodes, DogNode *node, Vector *results);

static void
tsort2_deps(Vector *nodes, DogNode *node, Vector *results)
{
    Vector *deps;
    int i;
    DogNode *dep;

    if (deps = node->forward_deps) {
	EACH(deps, i) {
	    dep = (DogNode *) ELEM(deps, i);
	    tsort2_node(nodes, dep, results);
	}
    }
}


static void
tsort2_node(Vector *nodes, DogNode *node, Vector *results)
{
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("Cyclic dep found in %s", node->fqn->value), node);
    case UNVISITED: 
    case RESOLVED: 
	BEGIN {
	    node->status = VISITING;
	    tsort2_deps(nodes, node, results);
	    vectorPush(results, (Object *) node);
	}
	EXCEPTION(ex) {
	    node->status = UNVISITED;
	    RAISE();
	}
	END;
	node->status = VISITED;
	break;
    case VISITED: 
	break;
    default:
	RAISE(TSORT_ERROR,
		  newstr("Unexpected status for dagnode %s: %d",
			 node->fqn->value, node->status));
    }
}

Vector *
simple_tsort2(Vector *nodes)
{
    Vector *volatile results = 
	vectorNew(nodes->elems + 10); // Allow for expansion
    DogNode *node;
    int i;
    BEGIN {
	EACH(nodes, i) {
	    node = (DogNode *) ELEM(nodes, i);
	    tsort2_node(nodes, node, results);
	}
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, FALSE);
	RAISE();
    }
    END;
    return results;
}

/* Do the sort. 
*/
Vector *
gensort2(Document *doc)
{
    Vector *volatile nodes = NULL;
    Vector *results = NULL;

    BEGIN {
	nodes = dagFromDoc(doc);
	results = simple_tsort2(nodes);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes, TRUE);
	RAISE();
    }
    END;
    objectFree((Object *) nodes, FALSE);
    return results;
}

