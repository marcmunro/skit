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


/* Add a DagNode to a hash of DagNodes, keyed by the node's fqn attribute.
 */
static void
addNodeToHash(Hash *hash, Node *node, DagNodeBuildType build_type)
{
    DagNode *dagnode = dagnodeNew(node, build_type);
    String *key = stringDup(dagnode->fqn);
    Object *old;

    dagnode->build_type = build_type;

    if (old = hashAdd(hash, (Object *) key, (Object *) dagnode)) {
	objectFree(old, TRUE);
	RAISE(GENERAL_ERROR, 
	      newstr("doAddNode: duplicate node \"%s\"", key->value));
    }
}

static DagNodeBuildType
buildTypeFor(Node *node)
{
    Object *do_drop;
    String *diff;
    String *fqn;
    char *errmsg;
    DagNodeBuildType build_type;

    if (!streq(node->node->name, "dbobject")) {
	RAISE(TSORT_ERROR, newstr("buildTypeFor: node is not a dbobject"));
    }
	      
    diff = nodeAttribute(node->node, "diff");

    if (diff) {
	if (streq(diff->value, DIFFSAME)) {
	    build_type = EXISTS_NODE;
	}
	else if (streq(diff->value, DIFFKIDS)) {
	    build_type = EXISTS_NODE;
	}
	else if (streq(diff->value, DIFFNEW)) {
	    build_type = BUILD_NODE;
	}
	else if (streq(diff->value, DIFFGONE)) {
	    build_type = DROP_NODE;
	}
	else if (streq(diff->value, DIFFDIFF)) {
	    build_type = DIFF_NODE;
	}
	else {
	    fqn = nodeAttribute(((Node *) node)->node, "fqn");
	    errmsg = newstr(
		"buildTypeFor: unexpected diff type \"%s\" in %s", 
		diff->value, fqn->value);
	    objectFree((Object *) diff, TRUE);
	    objectFree((Object *) fqn, TRUE);
	    RAISE(TSORT_ERROR, errmsg);
	}
    }
    else {
	do_drop = dereference(symbolGetValue("drop"));
	if (dereference(symbolGetValue("build"))) {
	    if (do_drop) {
		build_type = BUILD_AND_DROP_NODE;
	    }
	    else {
		build_type = BUILD_NODE;
	    }
	}
	else if (do_drop) {
	    build_type = DROP_NODE;
	}
	else {
	    fqn = nodeAttribute(((Node *) node)->node, "fqn");
	    errmsg = newstr(
		"buildTypeFor: cannot identify build type for node %s", 
		diff->value, fqn->value);
	    objectFree((Object *) fqn, TRUE);
	    objectFree((Object *) diff, TRUE);
	    RAISE(TSORT_ERROR, errmsg);
	}
    }
    objectFree((Object *) diff, TRUE);
    return build_type;
}

/* A TraverserFn to identify dbobject nodes, adding them as Dagnodes to
 * our hash.
 */
static Object *
dagnodesToHash(Object *this, Object *hash)
{
    Node *node = (Node *) this;
    DagNodeBuildType build_type;
    if (streq(node->node->name, "dbobject")) {
	if ((build_type = buildTypeFor(node)) == BUILD_AND_DROP_NODE) {
	    addNodeToHash((Hash *) hash, node, BUILD_NODE);
	    addNodeToHash((Hash *) hash, node, DROP_NODE);
	}
	else {
	    addNodeToHash((Hash *) hash, node, build_type);
	}
    }
    return NULL;
}

/* Build a hash of dagnodes from the provided document.  The hash
 * may contain one build node and one drop node per database object,
 * depending on which build and drop options have been selected.
 */
static Hash *
dagnodesFromDoc(Document *doc)
{
    Hash *daghash = hashNew(TRUE);

    BEGIN {
	(void) xmlTraverse(doc->doc->children, &dagnodesToHash, 
			   (Object *) daghash);
    }
    EXCEPTION(ex) {
	objectFree((Object *) daghash, TRUE);
    }
    END;
    return daghash;
}

/* A HashEachFn that, if our DagNode contains pqns, adds it to our pqn hash.
 */
static Object *
addPqnEntry(Cons *node_entry, Object *param)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Hash *pqnhash = (Hash *) param;
    xmlChar *base_pqn  = xmlGetProp(node->dbobject, "pqn");
    String *pqn;
    Cons *entry;

    if (base_pqn) {
	switch (node->build_type) {
	case EXISTS_NODE:
	    pqn = stringNewByRef(newstr("exists.%s", base_pqn));
	    break;
	case BUILD_NODE:
	    pqn = stringNewByRef(newstr("build.%s", base_pqn));
	    break;
	case DROP_NODE:
	    pqn = stringNewByRef(newstr("drop.%s", base_pqn));
	    break;
	case DIFF_NODE:
	    pqn = stringNewByRef(newstr("diff.%s", base_pqn));
	    break;
	default:
	    RAISE(TSORT_ERROR,
		  newstr("Unexpected build_type for dagnode %s",
			 node->fqn->value));
	}

	if (entry = (Cons *) hashGet(pqnhash, (Object *) pqn)) {
	    dbgSexp(pqnhash);
	    RAISE(NOT_IMPLEMENTED_ERROR,
		      newstr("We have two nodes with matching pqns.  "
			  "Add the new node to the match"));
	}
	else {
	    entry = consNew((Object *) objRefNew((Object *) node), NULL);
	    hashAdd(pqnhash, (Object *) pqn, (Object *) entry);
	}
	xmlFree(base_pqn);
    }
    return (Object *) node;
}


/* Build a hash of dagnodes keyed by pqn.  Each hash entry is a list of
 * all DagNodes matching the pqn.
 */
static Hash *
makePqnHash(Hash *allnodes)
{
    Hash *pqnhash = hashNew(TRUE);
    hashEach(allnodes, &addPqnEntry, (Object *) pqnhash);
    return pqnhash;
}

static DagNode *
makeBreakerNode(DagNode *from_node, String *breaker_type)
{
    xmlNode *dbobject = from_node->dbobject;
    xmlChar *old_fqn = xmlGetProp(dbobject, "fqn");
    char *fqn_suffix = strstr(old_fqn, ".");
    char *new_fqn = newstr("%s%s", breaker_type->value, fqn_suffix);
    xmlNode *breaker_dbobject = xmlCopyNode(dbobject, 1);
    Node *breaker_node;
    DagNode *breaker;
    
    xmlSetProp(breaker_dbobject, "type", breaker_type->value);
    xmlUnsetProp(breaker_dbobject, "cycle_breaker");
    xmlSetProp(breaker_dbobject, "fqn", new_fqn);
    xmlFree(old_fqn);
    skfree(new_fqn);

    breaker_node = nodeNew(breaker_dbobject);
    breaker = dagnodeNew(breaker_node, from_node->build_type);
    breaker->parent = from_node->parent;
    objectFree((Object *) breaker_node, FALSE);
    return breaker;
}

static Depset *
copyDepset(Depset *dep)
{
    Depset *new = NULL;
    Cons *cur;
    Cons *newcons = NULL;
    new = depsetNew();
    if (dep->actual) {
	/* Simple copy of only the "actual" reference. */
	new->actual = dep->actual;
    }
    else {
	/* Copy the whole depset list */
	new->is_set = dep->is_set;
	new->is_optional = dep->is_optional;
	cur = dep->deps;
	while (cur) {
	    newcons = consNew((Object *) objRefNew(dereference(cur->car)),
			      (Object *) newcons);
	    cur = (Cons *) cur->cdr;
	}
	new->deps = newcons;
    }
    return new;
}

/* Create a copy of a DepSet list, in the same order as the original.
 */
static Cons *
copyDeps(Cons *deps)
{
    Cons *result = NULL;
    Cons *prev = NULL;
    Cons *new;
    Cons *next = deps;
    Depset *dep;
    while (next) {
	if (dep = copyDepset((Depset *) next->car)) {
	    new = consNew((Object *) dep, NULL);
	    if (prev) {
		prev->cdr = (Object *) new;
	    }
	    else {
		result = new;
	    }
	    prev = new;
	}
	next = (Cons *) next->cdr;
    }
    return result;
}

static Depset *
removeDepset(Depset *depset, DagNode *remove_dep)
{
    DagNode *node = depset->actual;
    Cons *start;
    Cons *next;

    if (node) {
	if (streq(node->fqn->value, remove_dep->fqn->value)) {
	    /* This depset matches remove_dep, so we free it and return
	     * NULL to our caller. */
	    objectFree((Object *) depset, TRUE);
	    return NULL;
	}
	return depset;
    }

    next = depset->deps;
    while (next) {
	node = (DagNode *) dereference(next->car);
	if (streq(node->fqn->value, remove_dep->fqn->value)) {
	    depset->deps = consRemove(depset->deps, next);
	    if (!depset->deps) {
		/* We have removed the last entry in deps, so we can
		 * lose the entire depset */
		objectFree((Object *) depset, TRUE);
		return NULL;
	    }
	    /* Scan the list, from the beginning, again. */
	    next = depset->deps;
	}
	else {
	    next = (Cons *) next->cdr;
	}
    }

    return depset;
}

static Cons *
removeDeps(Cons *deps, DagNode *remove_dep)
{
    Cons *start = deps;
    Cons *next = deps;
    while (next) {
	next->car = (Object *) removeDepset((Depset *) next->car, remove_dep);
	if (!next->car) {
	    /* The depset in next has been removed, so we can delete
	     * next from deps. */
	    start = consRemove(start, next);
	    next = start;  /* Scan the list, from the beginning, again. */
	}
	else {
	    next = (Cons *) next->cdr;
	}
    }
    return start;
}

/* Note: we need to add to the depset->deps field as well as the actual
 * field, in order for the objref to be freed.
 */
static void
addActualDep(DagNode *from, DagNode *to)
{
    Depset *depset = depsetNew();
    Cons *deps = consNew(NULL, NULL);
    Cons *depsets = consNew((Object *) depset, (Object *) from->deps);
    depset->actual = to;
    deps->car = (Object *) objRefNew((Object *) depset->actual);
    depset->deps = deps;
    from->deps = depsets;
}

/* We break a cycle including Y->...Z->X->Y (if breakable) as follows:
 * We create a new node Xbreak as a copy of X but without the dependency
 * to Y and with a type of breaker_type.  We then add a dependency on
 * Xbreak to X, giving us Z->Xbreak, X->Xbreak, X->Y.
 */
static DagNode *
breakCycle(DagNode *node, DagNode *dep, Hash *dagnodes)
{
    xmlNode *dbobject = node->dbobject;
    String *breaker_type = nodeAttribute(dbobject, "cycle_breaker");
    DagNode *breaker = NULL;
    String *breaker_key;

    if (breaker_type) {
	/* We may already have the break node. */
	breaker = (DagNode *) hashGet(dagnodes, (Object *) breaker_type);
	if (!breaker) {
	    BEGIN {
		breaker = makeBreakerNode(node, breaker_type);
		breaker->deps = copyDeps(node->deps);
		breaker_key = stringNewByRef(newstr("%s", breaker->fqn->value));
		(void) hashAdd(dagnodes, (Object *) breaker_key, 
			       (Object *) breaker);
	    }
	    EXCEPTION(ex);
	    FINALLY {
		objectFree((Object *) breaker_type, TRUE);
	    }
	    END;
	}
	breaker->deps = removeDeps(breaker->deps, dep);
	/* And finally, we add a dependency from node to breaker */
	addActualDep(node, breaker);
    }
    return breaker;
}

/* Forward reference for the second of the following mutually recursive 
 * functions. */ 
static DagNode *dagify_node(DagNode *node, Hash *dagnodes);

/* Identify and set the actual node to be chosen for the depset, in
 * order to make the almost DAG into a true DAG.  We normally return the
 * value of the node for which we are processing the depset (ie the node
 * we were called with), but if we perform a cycle break (using
 * breakCycle()), we return the break_node.  This allows the caller to
 * modify their dependency on node, to instead depend on break_node
 */
static DagNode *
dagify_depset(DagNode *node, Depset *depset, Hash *dagnodes)
{
    Cons *nextdep = depset->deps;
    DagNode *dep;
    DagNode *cycle_node;
    DagNode *break_node;
    char *errmsg = NULL;
    char *tmp;
    boolean optional;
    boolean more;

    while (nextdep) {
	/* Although this is a loop, we only have to successfully process
	 * a single element. */
	dep = (DagNode *) dereference(nextdep->car);
	BEGIN {
	    depset->actual = NULL; /* Reset this in case we are retrying
				    * after recursing around some cyclic
				    * dependency problems.  We don't
				    * want this value to be set
				    * improperly. */
	    break_node = dagify_node(dep, dagnodes);
	    depset->actual = break_node;
	    RETURN(node);
	}
	EXCEPTION(ex);
	WHEN(TSORT_CYCLIC_DEPENDENCY) {
	    /* Do as little as possible in the exception handler. */
	    cycle_node = (DagNode *) ex->param;
	    errmsg = newstr("%s", ex->text);
	}
	END;

	optional = depset->is_optional && (!depset->is_set);
	if (optional) {
	    /* This is an optional dependency.  That means we can choose
	     * to not follow it. */
	    skfree(errmsg);
	    return node;
	}

	/* We will reach this point only if a cyclic dependency has been
	 * detected.  We will either resolve it, or re-raise the
	 * exception so that our caller can attempt to deal with it. */
	nextdep = (Cons *) nextdep->cdr;

	if (cycle_node) {
	    /* Attempt to break the cycle with help from a cycle-breaker 
	     * node.  This is only possible if nodes of this type
	     * provide a  cycle breaking mechanism. */
	    if (break_node = breakCycle(node, dep, dagnodes)) {
		/* We have a break node.  We replace the current dep
		 * with an actual dep on the break_node. */
		depset->actual = break_node;
		skfree(errmsg);
		return break_node;
	    }
	}

        more = nextdep && TRUE;
        if (!more) {
	    /* We have failed to find a dep we can safely follow.  Let's
	     * make this the caller's problem - maybe they will have a
	     * solution. */ 
	    if (cycle_node) {
		if (node == cycle_node) {
		    /* We are at the start of the cyclic dependency.
		     * Set errmsg to describe this, and reset cycle_node
		     * for the RAISE below. */ 
		    tmp = newstr("Cyclic dependency detected: %s->%s", 
				 node->fqn->value, errmsg);
		    cycle_node = NULL;
		}
		else {
		    /* We are somewhere in the cycle of deps.  Add the current
		     * node to the error message. */ 
		    tmp = newstr("%s->%s", node->fqn->value, errmsg);
		}
	    }
	    else {
		/* We are outside of the cyclic deps.  Add this node to
		 * the error message so we can see how we got here.  */
		tmp = newstr("%s from %s", errmsg, node->fqn->value);
	    }
	    skfree(errmsg);
	    errmsg = tmp;
    
	    /* Unable to resolve cycle.  Defer the issue to the caller */
	    RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg, cycle_node);
	}
    }
    return node;
}

static DagNode *
dagify_deps(DagNode *node, Cons *deps, Hash *dagnodes)
{
    Cons *next = deps;
    DagNode *result = node;
    DagNode *break_node;
    Depset *depset;
    while (next) {
	depset = (Depset *) next->car;
	next = (Cons *) next->cdr;  /* Identify the next depset before 
				     * dagifying, as the list may be
				     * modified */
	if (!depset->actual) {
	    break_node = dagify_depset(node, depset, dagnodes);
	    if (break_node != node) {
		result = break_node;
	    }
	}
    }
    return result;
}

static DagNode *
dagify_node(DagNode *node, Hash *dagnodes)
{
    DagNode *result = node;
    DagNode *result2;

    if (node->status == VISITING) {
	/* Create a simple string to describe the cyclic dependency.
	 * This will be modified and extended further up the stack (in
	 * dagify_deps()) and finally formatted (if the exception is not
	 * handled somewhere) in dagify().
	 */
	RAISE(TSORT_CYCLIC_DEPENDENCY, newstr("%s", node->fqn->value), node);
    }

    if (node->status != VISITED_ONCE) {
	node->status = VISITING;
	result = dagify_deps(node, node->deps, dagnodes);
	node->status = VISITED_ONCE;

	if (result != node) {
	    /* A new (cycle breaking node has been added to our set of
	     * nodes.  Like all others, it must be dagified, so let's do
	     * it now. */
	    result2 = dagify_node(result, dagnodes);
	    if (result2 != result) {
		/* There is no reason at all why dagifying a cycle
		 * breaking node should cause another node to be
		 * created - so raise an error. */
		RAISE(TSORT_ERROR,
		      newstr("Failed to dagify cycle breaking node %s",
			     result->fqn->value));
	    }
	}
    }
    return result;
}

/* Our dagnodes hash currently contains optional dependencies and
 * dependency sets.  What we must do now is identify a single simple
 * dependency for a dependency set, and identify which of our optional
 * dependencies are to be used.  Once we have done this, we will have a
 * true Directed Acyclic Graph (DAG) which we can traverse using a
 * tsort algorithm.
 * Note that we can't use hashEach to explore dagnodes, as we may be
 * adding cycle breaker nodes to the hash as we go, and modifying the
 * hash as we iterate through it is a definite no-no.
 */
static void
makeIntoDag(Hash *dagnodes)
{
    //showAllDeps(dagnodes);
    Vector *vector = vectorFromHash(dagnodes);
    int i;
    for (i = 0; i < vector->elems; i++) {
	(void) dagify_node((DagNode *) vector->contents->vector[i], dagnodes);
    }
    objectFree((Object *) vector, FALSE);
    return;
}

/* Forward ref for mutual recursion below. */ 
static void tsort_node(DagNode *node, Vector *results);

static void
tsort_deps(Cons *deps, Vector *results)
{
    Cons *next = deps;
    Depset *depset;
    DagNode *depnode;
    while (next) {
	depset = (Depset *) next->car;
	if (depnode = depset->actual) {
	    tsort_node(depnode, results);
	}
	next = (Cons *) next->cdr;
    }
}

static void
tsort_node(DagNode *node, Vector *results)
{
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY,
	      newstr("Cyclic dependency detected in: %s",
		  node->fqn->value));
    case VISITED_ONCE: 
	node->status = VISITING;
	tsort_deps(node->deps, results);
	vectorPush(results, (Object *) node);
	node->status = VISITED;
	break;
    case VISITED: 
	break;
    default:
	RAISE(TSORT_ERROR,
		  newstr("Unexpected status for dagnode %s: %s",
			 node->fqn->value, node->status));
    }
}


/* A HashEachFn that traverses a DAG in dependency order to return a
 * topologically sorted list of dagnodes.
 */
static Object *
do_tsort(Cons *node_entry, Object *param)
{
    DagNode *node = (DagNode *) dereference(node_entry->cdr);
    Vector *results = (Vector *) param;

    tsort_node(node, results);
    return (Object *) node;
}



static Vector *
simple_tsort(Hash *dagnodes)
{
    int elems = hashElems(dagnodes);
    Vector *results = vectorNew(elems);
    BEGIN {
	hashEach(dagnodes, &do_tsort, (Object *) results);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, TRUE);
	RAISE();
    }
    END;
    return results;
}


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
    if ((node->status == UNBUILDABLE) && !node->deps) {
	vectorPush(vector, (Object *) node);
    }
    return (Object *) node;
}

/* Return a vector of all nodes without dependencies */
static Vector *
getBuildCandidates(Hash *allnodes)
{
    int elems = hashElems(allnodes);
    Vector *results = vectorNew(elems);
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

/* Takes a vector of buildable nodes, and marks them as buildable within
 * our DagNodeTree, updating counts as necessary */
static void
markAllBuildable(Vector *buildable)
{
    // TODO: REMOVE THIS
    DagNode *next;
    int i;

    for (i = 0; i < buildable->elems; i++) {
	next = (DagNode *) buildable->contents->vector[i];
	markAsBuildable(next);
    }
    objectFree((Object *) buildable, FALSE);
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

/* node has been selected for building.  We remove it from the
 * DagNodeTree, and by removing dependencies on it, we possibly make
 * some more nodes buildable.
 */
static Vector *
removeNodeGetNewCandidates(DagNode *node, Hash *allnodes)
{
    Vector *results = vectorNew(64);
    Vector *deps;
    DagNode *next;
    Object *ref;
#ifdef wibble
TODO: add dependents vectors to the dagnode tree so that we can do what this 
function is supposed to
    if (deps = node->dependents) {
	while (ref = vectorPop(deps)) {
	    next = (DagNode *) dereference(ref);
	    removeDependency(next, node);
	    if (!next->dependencies) {
		(void) vectorPush(results, (Object *) next);
	    }
	    objectFree(ref, FALSE);
	}
    }
#endif
    /* Finally, we remove node from our hash. */
    (void) hashDel(allnodes, (Object *) node->fqn);
    return results;
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
    Cons *this = node->deps;
    Depset *dep;
    while (this) {
	dep = (Depset *) this->car;
	if (!dep->actual) {
	    /* Why/how did this happen? */
	    RAISE(TSORT_ERROR,
		  newstr("Missing actual dep for node %s", node->fqn->value));
	}
	
	addDependent(dep->actual, node);
	this = (Cons *) this->cdr;
	
    }
    return (Object *) node;
}

static void
removeDependency(DagNode *node, DagNode *dep)
{
    Cons *deps = node->deps;
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
		node->deps = next;
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
    Vector *results;
    Vector *buildable;

    hashEach(allnodes, &addDependents, (Object *) allnodes);
    results = vectorNew(hashElems(allnodes));
    markBuildCandidates(allnodes);

    next = root;
    while (next = nextBuildable(next)) {
	(void) vectorPush(results, (Object *) next);
	markAsSelected(next);
	dbgSexp(next);
	unlinkDependents(next, allnodes);
	markBuildCandidates(allnodes);
    }
 

    dbgSexp(results);
    RAISE(NOT_IMPLEMENTED_ERROR,
	  newstr("new smart_tsort is not implemented"));

    if (hashElems(allnodes)) {
	char *nodes = objectSexp((Object *) allnodes);
	char *errmsg = newstr("gensort: unsorted nodes remain:\n\"%s\"\n",
			      nodes);
	skfree(nodes);
	RAISE(GENERAL_ERROR, errmsg);
    }

    dbgSexp(buildable);
    //showAllDeps(allnodes);
    RAISE(NOT_IMPLEMENTED_ERROR,
	  newstr("new smart_tsort is not implemented"));
    return results;
}



/* Do the sort. 
 * This is going to be an eventual replacement for gensort.  For now it
 * is run before the real gensort.
*/
Vector *
gensort2(Document *doc)
{
    Hash *dagnodes = NULL;
    Hash *pqnhash = NULL;
    Symbol *simple_sort = symbolGet("simple-sort");
    Vector *results = NULL;

    BEGIN {
	dagnodes = dagnodesFromDoc(doc);
	pqnhash = makePqnHash(dagnodes);
	recordParentage(doc, dagnodes);
	recordDependencies(doc, dagnodes, pqnhash);
	makeIntoDag(dagnodes);
	if (simple_sort) {
	    results = simple_tsort(dagnodes);
	}
	else {
	    results = smart_tsort(dagnodes);
	}
	/* If we reach this point, our results vector contains the same
	 * elements as our dagnodes hash.  We resolve this by freeing
	 * the dagnodes hash without freeing its contents. */
	objectFree((Object *) dagnodes, FALSE);
	dagnodes = NULL;
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, FALSE);
	objectFree((Object *) dagnodes, TRUE);
	objectFree((Object *) pqnhash, TRUE);
	RAISE();
    }
    END;
    /* Note that we copy the actual nodes from dagnodes into
     * our results vector, so we don't need to free the contents of
     * dagnodes below. */
    objectFree((Object *) dagnodes, TRUE);
    objectFree((Object *) pqnhash, TRUE);
    
    return results;
}

#ifdef wibble
NEXT:
- remove original gensort calls
- add navigation 
- remove deprecated depset fields
- add back the smart_tsort
#endif
