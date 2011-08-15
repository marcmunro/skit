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
#include <stdarg.h>
#include "skit_lib.h"
#include "exceptions.h"

static boolean do_build = FALSE;
static boolean do_drop = FALSE;

#ifdef nowt
static String *
getPrefixedAttribute(xmlNodePtr node, 
		     char *prefix,
		     const xmlChar *name)
{
    String *result;
    xmlChar *value  = xmlGetProp(node, name);
    if (value) {
	result = stringNewByRef(newstr("%s.%s", prefix, (char *) value));
	xmlFree(value);
	return result;
    }
    return NULL;
}

static Cons *
consCopy(Cons *in)
{
    Cons *result = NULL;
    Cons *next;
    Cons *prev;
    Object *node;

    while (in) {
	node = dereference(in->car);
	next = consNew((Object *) objRefNew(node), NULL);
	if (result) {
	    prev->cdr = (Object *) next;
	}
	else {
	    result = next;
	}
	prev = next;
	in = (Cons *) in->cdr;
    }
    return result;
}

static void
freeConsNode(Cons *node)
{
    objectFree(node->car, FALSE);
    objectFree((Object *) node, FALSE);
}

static void
tsortdebug(char *x)
{
    fprintf(stderr, "DEBGUG %s\n", x);
}

static void
showNodeDeps(DagNode *node)
{
    int i;
    printSexp(stderr, "NODE: ", (Object *) node);
    if (node->dependencies) {
	for (i = 0; i < node->dependencies->elems; i++) {
	    printSexp(stderr, "   --> ", 
		      node->dependencies->contents->vector[i]);
	}
    }
}

static void
showAllNodeDeps(DagNode *node)
{
    int i;
 
    showNodeDeps(node);
    if (node->dependents) {
	for (i = 0; i < node->dependents->elems; i++) {
	    printSexp(stderr, "   <-- ", node->dependents->contents->vector[i]);
	}
    }
}

static Object *
showDeps(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    showAllNodeDeps(node);
    return (Object *) node;
}

static void
showAllDeps(Hash *nodes)
{
    hashEach(nodes, &showDeps, (Object *) nodes);
}

static Cons *
visitNode(DagNode *node, Hash *allnodes)
{
    int i;
    Object *deps;
    DagNode *dep;
    Cons *result = NULL;
    switch (node->status) {
    case VISITED:
        return NULL;
    case VISITING:
        return consNew((Object *) objRefNew((Object *) node), NULL);
    }
    node->status = VISITING;
    if (node->dependencies) {
        for (i = node->dependencies->elems - 1; i >= 0; i--) {
            deps = node->dependencies->contents->vector[i];
	    dep = dependencyFromSet(node, deps, allnodes, FALSE, &result);
	    if (!dep) {
		/* We have an unhandled cyclic dependency.  Attempt a
		 * retry, allowing it to be handled. */
		objectFree((Object *) result, TRUE);
		dep = dependencyFromSet(node, deps, allnodes, TRUE, &result);
		if (!dep) {
		    if (result) {
			result = consNew((Object *) objRefNew((Object *) node),
					 (Object *) result);
			node->status = UNVISITED;
			return result;
		    }
		    if (!result) {
			// Part of ongoing work to refactor for optional deps
			//RAISE(NOT_IMPLEMENTED_ERROR, 
			//      newstr("Check this code path(1)"));
		    }
		}
	    }

	    /* Replace depset with the single dependency to which we
	     * have successfully traversed, or remove the depset if
	     * it was an unused optional dep. */ 
	    objectFree(node->dependencies->contents->vector[i], TRUE);
	    if (dep) {
		deps = (Object *) objRefNew((Object *) dep);
		node->dependencies->contents->vector[i] = (Object *) deps;
	    }
	    else {
		vectorRemove(node->dependencies, i);
	    }
        }
    }
    node->status = VISITED;
    return NULL;
}

static void
report_cycle(DagNode *node)
{
    DagNode *this = node->cur_dep;
    char *str;
    char *str2;
    str = objectSexp((Object *) node);
    fprintf(stderr, "CYCLE: %s", str);
    while (this) {
	str2 = objectSexp((Object *) this);
	fprintf(stderr, " <-- %s", str2);
	skfree(str2);
	this = this->cur_dep;
	if (this == node) {
	    break;
	}
    }
    fprintf(stderr, " <-- %s\n", str);
    skfree(str);
}

#ifdef WASSIS
Object *
update_node_deps(DagNode *node, int i, DepTracker *tracker)
{
    if (tracker->deps) {
	node->dependencies->contents->vector[i] = tracker->deps;
    }
    else {
	vectorRemove(node->dependencies, i);
    }
}

void
attempt_cycle_break(DagNode *node, int i, DagNode *dep, DepTracker *tracker)
{
    Cons *prev;
    Cons *next;
    // TODO: proper implementation
    switch (tracker->type) {
    case OBJ_INT4:
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("Have not yet implemented "
		     "attempt_cycle_break() for optionals"));
	// ONCE THIS IS DONE, ALL CYCLES FROM DEPSETS SHOULD BE
	// RESOLVED.  CHECK THE DAG AFTERWARDS AND ALSO CHECK THE depset
	// HASH
    case OBJ_CONS:
	/* Eliminate the current entry from the list. */
	prev = NULL;
	next = (Cons *) tracker->deps;
	while ((Cons *) next->cdr != tracker->next) {
	    prev = next;
	    next = (Cons *) next->cdr;
	}
	if (prev) {
	    /* Remove the dep from the middle of the list */
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("Have not yet implemented "
			 "removal from the middle"));
	}
	else {
	    /* Unlink the cons cell. */
	    next->cdr = NULL;
	    objectFree((Object *) next, TRUE);
	    tracker->deps = (Object *) tracker->next;
	    if (tracker->next) {
		tracker->next = (Cons *) tracker->next->cdr;
	    }
	    update_node_deps(node, i, tracker);
	}
    }
}
#endif
#endif

static void
removeDependency(DagNode *from, DagNode *dependency)
{
    int i;
    DagNode *dep;

    if (from->dependencies) {
	/* Loop backwards as vectorRemove will reposition all elements
 	 * following the removed item.  This is both faster and safer. */
	for (i = from->dependencies->elems - 1; i >= 0; i--) {
	    dep = (DagNode *) from->dependencies->contents->vector[i];
	    if (dereference((Object *) dep) == (Object *) dependency) {
		objectFree((Object *) dep, TRUE);
		(void) vectorRemove(from->dependencies, i);
	    }
	}
	if (!from->dependencies->elems) {
	    objectFree((Object *) from->dependencies, TRUE);
	    from->dependencies = NULL;
	}
    }
}


static Vector *
removeNodeGetNewCandidates(DagNode *node, Hash *allnodes)
{
    Vector *results = vectorNew(64);
    Vector *deps;
    DagNode *next;
    Object *ref;

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

    /* Finally, we remove node from our hash. */
    (void) hashDel(allnodes, (Object *) node->fqn);
    return results;
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

static DagNode *
nextBuildable(DagNode *node)
{
    DagNode *sibling;
    /* Find the next buildable node in the tree.  */
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
markAllBuildable(Vector *buildable)
{
    DagNode *next;
    int i;

    for (i = 0; i < buildable->elems; i++) {
	next = (DagNode *) buildable->contents->vector[i];
	markAsBuildable(next);
    }
    objectFree((Object *) buildable, FALSE);
}

int 
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

    /* First we sort the vector by fqn. */
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

static void
addDependent(DagNode *node, DagNode *dep)
{
    Object *new;
    assert(node->type == OBJ_DAGNODE,
	"addDependent: Cannot handle non-dagnode nodes");
    assert(dep->type == OBJ_DAGNODE,
	"addDependent: Cannot handle non-dagnode dependent");
    
    if (!(dep->dependents)) {
	dep->dependents = vectorNew(10);
    }
    if (!setPush(dep->dependents, 
		 new = (Object *) objRefNew((Object *) node))) {
	/* If the object was already in place, free up the objReference */
	objectFree(new, FALSE);
    }
}



static Object *
addDependentsForNode(Cons *node_entry, Object *param)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Hash *allnodes = (Hash *) param;
    Vector *dependencies = node->dependencies;
    int i;
    Object *deps;

    if (dependencies) {
	for (i = 0; i < dependencies->elems; i++) {
	    deps = dependencies->contents->vector[i];
	    addDependent(node, (DagNode *) dereference(deps));
	}
    }
    return (Object *) node;
}


static void
addAllDependents(Hash *allnodes)
{
    hashEach(allnodes, &addDependentsForNode, (Object *) allnodes);
}

static Object *
addCandidateToBuild(Cons *node_entry, Object *results)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Vector *vector = (Vector *) results;
    String *parent_name;

    assert(node->type == OBJ_DAGNODE, "Node is not a dagnode");
    if (!node->dependencies) {
	vectorPush(vector, (Object *) node);
    }
    return (Object *) node;
}

/* Return a vector of all nodes without dependencies */
static Vector *
get_build_candidates(Hash *nodelist)
{
    int elems = hashElems(nodelist);
    Vector *results = vectorNew(elems);
    BEGIN {
	hashEach(nodelist, &addCandidateToBuild, (Object *) results);
    }
    EXCEPTION(ex) {
	objectFree((Object *) results, FALSE);
    }
    END
    return results;
}


/* This is Marc's smart tsort algorithm.  This algorithm attempts to 
 * sort not just by dependencies, but so that we do as little
 * tree-traversal as possible during the build.  Note that this is
 * way slower than the standard tsort algorithm but produces output
 * which is more "naturally ordered" (more like a person would create,
 * making the output more readable).
 * The algorithm is this:
 * smart_tsort(hash: all_nodes)
 *   tree := create sorted tree from all_nodes
 *           -- This tree is sorted by fqn and structured by node
 *           --   parentage.  Each node in the tree has fields:
 *           --   status: (unbuildable, buildable, selected_for_build),
 *           --   integer: buildable_kids
 *           -- initialised to unbuildable and 0 respectively
 *  candidates := unordered list of all buildable nodes from all_nodes
 *  for each candidate in candidates loop
 *    mark the node as buildable in tree
 *    increment buildable_kids in all ancestors in the tree
 *  end loop
 *  buildlist := new empty list
 *  previous position in tree := root of tree
 *  loop
 *    candidate := first buildable node found by minimal traversal of
 *                   tree from previous position
 *    append candidate to buildlist
 *    mark node as built in tree
 *    decrement buildable_kids in all ancestors in the tree
 *    for each dependent item loop
 *      unlink candidate from dependent
 *      if dependent has no more dependencies then
 *        mark dependent as buildable in tree
 *        increment buildable_kids in all ancestors in the tree
 *      end if
 *    end loop
 *  until no more buildable nodes
 * 
 * API notes: allnodes should be empty when we are done!
 */
static Vector *
smart_tsort(Hash *allnodes)
{
    DagNode *root = initDagNodeTree(allnodes);
    DagNode *next;
    int i;
    Vector *results = vectorNew(hashElems(allnodes));
    Vector *buildable = get_build_candidates(allnodes);

    //showAllDeps(allnodes);
    addAllDependents(allnodes);
    markAllBuildable(buildable);  /* Frees the buildable vector */

    next = nextBuildable(root);
    while (next) {
	(void) vectorPush(results, (Object *) next);
	markAsSelected(next);
	buildable = removeNodeGetNewCandidates(next, allnodes);
	markAllBuildable(buildable);
	next = nextBuildable(next);
    }
 
    if (hashElems(allnodes)) {
	char *nodes = objectSexp((Object *) allnodes);
	char *errmsg = newstr("gensort: unsorted nodes remain:\n\"%s\"\n",
			      nodes);
	skfree(nodes);
	RAISE(GENERAL_ERROR, errmsg);
    }
    return results;
}

/* Add a DagNode to a hash of DagNodes, keyed by the node's fqn attribute.
 */
static void
doAddNode(Hash *hash, Node *node, DagNodeBuildType build_type)
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

/* A TraverserFn to identify dbobject nodes, adding them as Dagnodes to
 * our hash.
 */
static Object *
dagnodesToHash(Object *this, Object *hash)
{
    Node *node = (Node *) this;
    String *diff;
    String *fqn;
    char *errmsg;
    DagNodeBuildType build_type;
    boolean both = FALSE;
    if (streq(node->node->name, "dbobject")) {
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
		    "addDagNodeToHash: cannot handle diff type %s in %s", 
		    diff->value, fqn->value);
		objectFree((Object *) diff, TRUE);
		objectFree((Object *) fqn, TRUE);
		RAISE(GENERAL_ERROR, errmsg);
	    }
	    objectFree((Object *) diff, TRUE);
	}
	else {
	    if (do_build) {
		build_type = BUILD_NODE;
		both = do_drop;
	    }
	    else if (do_drop) {
		build_type = DROP_NODE;
	    }
	}
	
	doAddNode((Hash *) hash, node, build_type);
	if (both) {
	    doAddNode((Hash *) hash, node, DROP_NODE);
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

    do_build = dereference(symbolGetValue("build")) && TRUE;
    do_drop = dereference(symbolGetValue("drop")) && TRUE;

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

/* Predicate identifying whether a node is of a specific type.
 */
static boolean
xmlnodeMatch(xmlNode *node, char *name)
{
    return node && (node->type == XML_ELEMENT_NODE) && streq(node->name, name);
}


/* Find the nearest (xml document) ancestor node of the given type. */
static xmlNode *
findAncestor(xmlNode *start, char *name)
{
    xmlNode *result = start;
    while (result = (xmlNode *) result->parent) {
	if (xmlnodeMatch(result, name)) {
	    return result;
	}
    }
    return NULL;
}

/* Concatenate each vararg in turn with fqn, and return the first node
 * from allnodes that has that key.  Eg:
 * findNodeFromOptions(hash, "cluster", "build", "exists", "diff", NULL)
 * will look first for "build.cluster", then "exists.cluster", etc.
 */
static Object *
findNodeFromOptions(Hash *allnodes, String *fqn, ...)
{
    va_list args;
    char *prefix;
    String *key;
    Object *result = NULL;

    va_start(args, fqn);
    while (prefix = va_arg(args, char *)) {
	key = stringNewByRef(newstr("%s.%s", prefix, fqn->value));
	result = hashGet(allnodes, (Object *) key);
	objectFree((Object *) key, TRUE);
	if (result) {
	    break;
	}
    }
    va_end(args);
    return result;
}


static Cons *
consCopy(Cons *in)
{
    Cons *result = NULL;
    Cons *next;
    Cons *prev;
    Object *node;

    while (in) {
	node = dereference(in->car);
	next = consNew((Object *) objRefNew(node), NULL);
	if (result) {
	    prev->cdr = (Object *) next;
	}
	else {
	    result = next;
	}
	prev = next;
	in = (Cons *) in->cdr;
    }
    dbgSexp(result);
    RAISE(NOT_IMPLEMENTED_ERROR,
	  newstr("Fix the function comment and remove this."));
    return result;
}

/* Identify a matching node for fqn, based on the build_type.  This is
 * not a trivial as, if we are building from a diff, we may be creating
 * a child of a new object, or one that has changed, or one that is
 * unchanged, so the parent node may have the prefix, "build",
 * "exists", or "diff". */
static Object *
nodeForBuildType(Hash *hash, String *fqn, DagNodeBuildType build_type)
{
    Object *result;
    switch (build_type) {
    case BUILD_NODE:
    case DIFF_NODE:
    case EXISTS_NODE:
	result = findNodeFromOptions(hash, fqn, 
				     "build", "exists", "diff", NULL);
	break;
    case DROP_NODE:
	result = findNodeFromOptions(hash, fqn, 
				     "drop", "exists", "diff", NULL);
	break;
    default:
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("nodeForBuildType for build type %d is not implemented",
		     build_type));
    }
    if (result) {
	if (result->type == OBJ_CONS) {
	    if (((Cons *) result)->cdr) {
		RAISE(NOT_IMPLEMENTED_ERROR,
		      newstr("I SUSPECT WE DON'T GET HERE!  TODO: REMOVE"));
		return (Object *) consCopy((Cons *) result);
	    }
	    else {
		/* Although the result is a list, it has only one
		 * member, so return it as a single DagNode */
		result = (Object *) dereference(((Cons *) result)->car);
	    }
	}
    }
    return result;
}

/* A HashEachFn, to record the parent for a dagnode.  This is required
 * primarily for figuring out navigation to and from nodes during a
 * build.  Note that this parentage is quite distinct from any
 * dependencies on parentage - the dependencies will be added later.
 */
static Object *
addParentForNode(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    xmlNode *parent = findAncestor(node->dbobject, "dbobject");
    String *parent_fqn = NULL;
    if (parent) {
	BEGIN {
	    parent_fqn  = nodeAttribute(parent, "fqn");
	    node->parent = (DagNode *) nodeForBuildType((Hash *) dagnodes, 
							parent_fqn, 
							node->build_type);
	}
	EXCEPTION(ex);
	FINALLY {
	    objectFree((Object *) parent_fqn, TRUE);
	}
	END;
    }
    return (Object *) node;
}

/* Identify the type of a dependency.  Shouldn't they all be the same
 * now? 
 */
static depType
dependencyType(Object *deps)
{
    char *sexp;
    char *msg;
    if (deps->type == OBJ_DAGNODE) {
	/* Yep, these are still possible: 20110813 */
	return DEP_SINGLE;
    }
    else if (deps->type == OBJ_CONS) {
	if (((Cons *) deps)->car->type == OBJ_INT4) {
	    /* Yep, these are still possible: 20110813 */
	    return DEP_OPTIONAL;
	}
	else {
	    /* Yep, these are still possible: 20110813 */
	    return DEP_LIST;
	}
    }
    sexp = objectSexp(deps);
    msg = newstr("Unexpected dependency type: %s", sexp);
    skfree(sexp);
    RAISE(TSORT_ERROR, msg);
}

/* Check whether a dep is already present in a dependency list. */
static boolean
depExists(DagNode *node, Object *dep)
{
    Object *actual = dereference(dep);
    Vector *vec = node->dependencies;
    int i;
    if (vec) {
	for (i = 0; i < vec->elems; i++) {
	    if (dereference(vec->contents->vector[i]) == actual) {
		return TRUE;
	    }
	}
    }
    return FALSE;
}

/* Add a dependency to node's dependency list, iff that dep is not
 * already present.
 */
static void
addDependencies(DagNode *node, Object *deps)
{
    Cons *cons;
    Cons *tmp;
    Cons *prev = NULL;
    Object *this;
    Int4 *depno;
    depType dep_type = dependencyType(deps);

    assert(node->type == OBJ_DAGNODE,
	"addDependency: node must be a dagnode");

    switch (dep_type) {
    case DEP_LIST:
	/* If any item in the dependency set already exists as a
	 * dependency, we can ignore the dependency entirely. */
	// Yep, we get here 20110813
	cons = (Cons *) deps;
	while (cons) {
	    this = cons->car;
	    if (depExists(node, this)) {
		objectFree(deps, TRUE);
		return;
	    }
	    cons = (Cons *) cons->cdr;
	}

	this = deps;
	break;
    case DEP_OPTIONAL:
	/* Yep, we still get here: 20110813 */
	this = deps;
	break;
    case DEP_SINGLE:
	if (depExists(node, deps)) {
	    /* This dependency is already recorded, so nothing else to do. */
	    return;
	}
	this = (Object *) objRefNew(deps);
    }

    if (!(node->dependencies)) {
	node->dependencies = vectorNew(10);
    }

    vectorPush(node->dependencies, this);
}

/* Add a drop node dependency for a build node: 
 * Build nodes are dependent on their matching drop nodes, if
 * any exist.  This is so that drops happen before builds. */
static void
addDropNodeDependency(DagNode *node, Hash *allnodes)
{
    char *base_fqn = strchr(node->fqn->value, '.');
    char *depname = newstr("drop%s", base_fqn);
    String *depkey = stringNewByRef(depname);
    DagNode *drop_node = (DagNode *) hashGet(allnodes, (Object *) depkey);
    
    if (drop_node) {
	addDependencies(node, (Object *) drop_node);
    }
    
    objectFree((Object *) depkey, TRUE);
}

/* Find the next (xml document) sibling of the given node type. */
static xmlNode *
findNextSibling(xmlNode *start, char *name)
{
    xmlNode *result = start;
    if (result) {
	while (result = (xmlNode *) result->next) {
	    if (xmlnodeMatch(result, name)) {
		return result;
	    }
	}
    }
    return NULL;
}

/* Find the first (xml document) child of the given node type. */
static xmlNode *
findFirstChild(xmlNode *parent, char *name)
{
    if (xmlnodeMatch(parent->children, name)) {
	return parent->children;
    }
    return findNextSibling(parent->children, name);
}

static Object *
depsFromNode(xmlNode *dep_defn, DagNodeBuildType build_type, Cons *hashes)
{
    String *fqn = nodeAttribute(dep_defn, "fqn");
    String *pqn;
    Object *depnode = NULL;
    Hash *allnodes = (Hash *) hashes->car;
    Hash *pqnhash = (Hash *) hashes->cdr;

    if (fqn) {
	depnode = nodeForBuildType(allnodes, fqn, build_type);
	objectFree((Object *) fqn, TRUE);
	if (depnode) {
	    return depnode;
	}
    }
    else {
	pqn = nodeAttribute(dep_defn, "pqn");
	depnode = (Object *) nodeForBuildType(pqnhash, pqn, build_type);
	objectFree((Object *) pqn, TRUE);
    }

    return depnode;
}

/* Predicate identifying whether a dpendency is classed as "old".  An
 * old dependency is one for a diff node, that no longer applies because
 * of the nature of the differences.  For example if a table no longer
 * has a column of type wibble_t, there will be an "old" dependency on
 * wibble_t.
 */
static boolean
isOldDep(xmlNode *dep_node)
{
    xmlChar *old = xmlGetProp(dep_node, "old");
    if (old) {
	xmlFree(old);
	return TRUE;
    }
    return FALSE;
}

static Hash *
optionalDepsHash()
{
    static Symbol *optional_deps = NULL;
    Hash *hash;
    if (!optional_deps) {
	optional_deps = symbolNew("optional-deps");
	hash = hashNew(TRUE);
	symSet(optional_deps, (Object *) hash);
	return hash;
    }
    return (Hash *) symGet(optional_deps);
}


static int
defineOptionalDep()
{
    static int depset_id = 0;
    Hash *hash = optionalDepsHash();
    Int4 *key;

    depset_id++;
    key = int4New(depset_id);
    (void) hashAdd(hash, (Object *) key, NULL);
    return depset_id;
}

/* These are the rules for diff dependencies:     
 * - for standard dependencies:
 *   - if there is an exists node, all is well and there is no
 *     dependency
 *   - if there is a build node we depend on that
 *   - if there is a diff node, we depend on the diff
 *   - otherwise we have an error
 * - handle parents the same as standard dependencies
 * - when there are old dependencies:
 *   - if the old dependency's node is a drop, it depends on this diff
 *     (ie the drop may not happen until the diff has been performed)
 *   - if the old dependency's node is a diff, the same applies
 *   - if the old dep is an exists node, it can be ignored
 *   - otherwise there is an error.
 * 
 */
// Inverted deps where there is a list of options is a little
// tricky!  Need to think about this.
// I think the solution is to make each inverted dep, itself
// optional.  An optional dep would be allowed to not be satisfied
// when traversing the dag (and would be the last dep checked for a
// node).  To mark a dep optional I think having a list with an
// initial nil would work.

static void
addDirectedDependencies(DagNode *node, Object *deps, boolean is_old_dep)
{
    boolean inverted = is_old_dep || (node->build_type == DROP_NODE);
    boolean free_deps = FALSE;
    DagNode *this;
    Cons *next;
    Object *this_dep;
    int depset_id;

    if (inverted) {
	if (deps->type == OBJ_CONS) {
	    this = (DagNode *) dereference(((Cons *) deps)->car);
	    next = (Cons *) ((Cons *) deps)->cdr;
	    depset_id = defineOptionalDep();
	    free_deps = TRUE;
	}
	else {
	    this = (DagNode *) deps;
	    next = NULL;
	    this_dep = (Object *) node;
	}
	while (this) {
	    assert(this->type == OBJ_DAGNODE, "Node is not a dagnode");
	    switch (this->build_type) {
	    case EXISTS_NODE: break;
	    case BUILD_NODE:
		RAISE(TSORT_ERROR,
		      newstr("Old dependency on a build node.  "
			     "Something is very wrong"));
	    case DIFF_NODE:
	    case DROP_NODE:
		if (deps->type == OBJ_CONS) {
		    this_dep = (Object *) 
			consNew((Object *) int4New(depset_id), 
				(Object *) objRefNew((Object *) node));
		}
		addDependencies(this, this_dep);
		break;
	    default:
		RAISE(NOT_IMPLEMENTED_ERROR, 
		      newstr("addDirectedDependency does not handle this: %d",
			     this->build_type));
	    }
	    if (next) {
		this = (DagNode *) dereference(next->car);
		next  = (Cons *) next->cdr;
	    }
	    else {
		break;
	    }
	}
	if (free_deps) {
	    objectFree(deps, TRUE);
	}
    }
    else {
	switch (node->build_type) {
	case EXISTS_NODE: break;
	case BUILD_NODE:
	case DIFF_NODE:
	case DROP_NODE:
	    addDependencies(node, deps);
	    break;
	default:
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("addDirectedDependency does not handle this(2): %d",
			 node->build_type));
	}
    }
}

/* Add a simple, single dependency to the node's list of dependencies.
 */
static void
addSimpleDepToNode(DagNode *dagnode, xmlNode *dep_node, Cons *hashes)
{
    Object *deps;

    if (deps = depsFromNode(dep_node, dagnode->build_type, 
			    (Cons *) hashes)) {
	addDirectedDependencies(dagnode, deps, isOldDep(dep_node));
    }
    else {
	char *node_str = nodestr(dep_node);
	char *errstr = newstr("No dep found for: %s", node_str);
	skfree(node_str);
	RAISE(TSORT_ERROR, errstr);
    }
}

/* Report that no deps within a dependency set were found. */
static void
reportDepsetError(const char *msg, xmlNode *depset_node)
{
    xmlNode *dep_node;
    char *node_str = NULL;
    char *pqn;
    char *errstr;
    for (dep_node = findFirstChild(depset_node, "dependency");
	 dep_node; dep_node = findNextSibling(dep_node, "dependency")) 
    {
	dbgNode(dep_node);
	pqn = xmlGetProp(dep_node, "pqn");
	fprintf(stderr, "PQN: %s\n", pqn);
	dbgNode(depset_node->parent);
	dbgNode(depset_node->parent->parent);
	if (node_str) {
	    errstr = newstr("%s, %s", node_str, pqn);
	    skfree(node_str);
	    node_str = errstr;
	}
	else {
	    node_str = newstr(pqn);
	}
	xmlFree(pqn);
    }
    
    errstr = newstr("%s for: %s", msg, node_str);
    skfree(node_str);
    RAISE(TSORT_ERROR, errstr);
}

/* Make a cons-cell containing node
 */
static Cons *
consNode(DagNode *node)
{
    ObjReference *ref = objRefNew(dereference((Object *) node));
    Cons *result = consNew((Object *) ref, NULL);
    return result;
}

/* Add a depenency set to a node.  A dependency set is simply a
 * collection of dependencies.
 */
static void
addDepSetToNode(DagNode *dagnode, xmlNode *depset_node, Cons *hashes)
{
    xmlNode *dep_node;
    Object *deps = NULL;
    Object *next;

    for (dep_node = findFirstChild(depset_node, "dependency");
	 dep_node; dep_node = findNextSibling(dep_node, "dependency")) 
    {
	if (isOldDep(dep_node)) {
	    reportDepsetError("Old dep handling not implemented", depset_node);
	}
	if (next = depsFromNode(dep_node, dagnode->build_type,
				(Cons *) hashes))
	{
	    if (deps) {
		if (deps->type != OBJ_CONS) {
		    deps = (Object *) consNode((DagNode *) deps);
		}
		if (next->type != OBJ_CONS) {
		    next = (Object *) consNode((DagNode *) next);
		}
		deps = (Object *) consConcat((Cons *) deps, (Cons *) next);
	    }
	    else {
		deps = next;
	    }
	}
    }
    if (deps) {
	addDirectedDependencies(dagnode, deps, FALSE);
    }
    else {
	reportDepsetError("No deps found", depset_node);
    }
}

/* A HashEachFn, that identifies and adds dependencies to our DagNode.
 */
static Object *
addDepsForNode(Cons *node_entry, Object *hashes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    xmlNode *deps_node;
    xmlNode *depset_node;
    xmlNode *dep_node;

    switch (node->build_type) {
    case EXISTS_NODE: 
	return (Object *) node;
    case DROP_NODE: 
	if (node->parent) {
	    addDependencies(node->parent, (Object *) node);
	}
	break;
    case BUILD_NODE: 
	addDropNodeDependency(node, (Hash *) ((Cons *) hashes)->car);
	/* No break: this is deliberate */
    case DIFF_NODE:
	/* Note this code executes for build and diff nodes */
	if (node->parent) {
	    addDependencies(node, (Object *) node->parent);
	}
    }

    deps_node = findFirstChild(node->dbobject, "dependencies");
    if (deps_node) {
	for (dep_node = findFirstChild(deps_node, "dependency");
	     dep_node; dep_node = findNextSibling(dep_node, "dependency")) 
	{
	    addSimpleDepToNode(node, dep_node, (Cons *) hashes);
	}

	for (depset_node = findFirstChild(deps_node, "dependency-set");
	     depset_node; 
	     depset_node = findNextSibling(dep_node, "dependency-set")) 
	{
	    addDepSetToNode(node, depset_node, (Cons *) hashes);
	}
    }

    return (Object *) node;
}


/* Identify all dependencies for our DagNodes, creating and filling in
 * the dependencies dagnode->dependencies and dagnode->dependents
 * vectors as we go.
 */
static void
identifyDependencies(Document *doc, Hash *dagnodes, Hash *pqnlist)
{
    Cons *hashes = consNew((Object *) dagnodes, (Object *) pqnlist);
    BEGIN {
	hashEach(dagnodes, &addParentForNode, (Object *) dagnodes);
	hashEach(dagnodes, &addDepsForNode, (Object *) hashes);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) hashes, FALSE);
    }
    END;
}


static void
solidify_node_options(DagNode *node)
{
    Cons *list;
    Cons *entry;
    int idx;

    //TODO: record when optional deps have been satisfied.
    if (list = node->chosen_options) {
	while (list) {
	    entry = (Cons *) list->car;
	    idx = ((Int4 *) entry->car)->value;
	    list = (Cons *) list->cdr;
	    if (entry->cdr) {
		Cons *depset = (Cons *) node->dependencies->contents->
		    vector[idx];
		node->dependencies->contents->vector[idx] = entry->cdr;
		entry->cdr = NULL;
		objectFree((Object *) depset, TRUE);
	    }
	    else {
		Cons *option = (Cons *) node->dependencies->contents->
		    vector[idx];
		node->dependencies->contents->vector[idx] = option->cdr;
		option->cdr = NULL;
		objectFree((Object *) option, TRUE);
	    }
	}
    }
}

static Object *
solidifyOptions(Cons *entry, Object *ignore)
{
    DagNode *node = (DagNode *) entry->cdr;
    solidify_node_options(node);
    return (Object *) node;
}


// TODO: Figure out if this can be optimised away.
static Object *
resetStatus(Cons *entry, Object *allnodes)
{
    DagNode *node = (DagNode *) entry->cdr;
    node->status = UNVISITED;

    return (Object *) node;
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

static void
copyDeps(DagNode *target, DagNode *src, DagNode *do_not_copy)
{
    DagNode *ignore = src->cur_dep;
    Cons *src_cons;
    Object *src_deps;
    Object *target_deps;
    Cons *target_cons;
    DagNode *src_dagnode;
    int i;

    if (src->dependencies) {
	for (i = 0; i < src->dependencies->elems; i++) {
	    src_deps = src->dependencies->contents->vector[i];
	    if (src_deps->type == OBJ_CONS) {
		src_cons = (Cons *) src_deps;
		target_cons = NULL;
		while (src_cons) {
		    src_dagnode = (DagNode *) dereference(src_cons->car);
		    if (src_dagnode != do_not_copy) {
			target_cons = consNew((Object *) objRefNew(
						  (Object *) src_dagnode), 
					      (Object *) target_cons); 
		    }
		    src_cons = (Cons *) src_cons->cdr;
		}
		target_deps = (Object *) target_cons;
	    }
	    else {
		src_dagnode = (DagNode *) dereference(src_deps);
		if (src_dagnode != do_not_copy) {
		    target_deps = (Object *) src_dagnode;
		}
		else {
		    target_deps = NULL;
		}
	    }
	    
	    if (target_deps) {
		addDependencies(target, target_deps);
	    }
	}
    }
}

static DagNode *
attemptCycleBreak(
    DagNode *cur_node,
    DagNode *cycle_node)
{
    xmlNode *dbobject = cycle_node->dbobject;
    String *breaker_type = nodeAttribute(dbobject, "cycle_breaker");
    DagNode *breaker = NULL;

    if (breaker_type) {
	/* We break the cycle of X->Y->X as follows:
	 * (cur_node is Y, cycle_node is X)
	 * create a new dbobject X_break as a copy of X, but without 
	 * the dependency in cycle_node->cur_dep and with a type of
	 * breaker_type.  We add a dependency on X_break to X, set Y's
	 * cur_dep to X_break, visit X_break using visitNode() and then
	 * return X_break as the result. 
	 */

	breaker = makeBreakerNode(cycle_node, breaker_type);
	copyDeps(breaker, cycle_node, cur_node);
	xmlSetProp(cycle_node->dbobject, "cycle_breaker_fqn", 
		   breaker->fqn->value);
	objectFree((Object *) breaker_type, TRUE);
    }
    return breaker;
}


static void
optionalDepSetSatisfied(Int4 *dep)
{
    Hash *hash = optionalDepsHash();
    Int4 *key = int4New(dep->value);
    Symbol *t = symbolGet("t");
    ObjReference *ref = objRefNew((Object *) t);
    fprintf(stderr, "Satisfying %d\n", dep->value);
    (void) hashAdd(hash, (Object *) key, (Object *) ref);
}



/* This function checks that the set of dependencies in allnodes, makes
 * A DAG.  Specifically, it resolves optional dependencies picking a
 * specific dependency from each set of candidates, and attempts to
 * resolve cyclic dependencies by checking each dbobject in the cycle
 * for the ability to break a cyle.
 * Here is the basic algorithm:
 * for each node n do
 *   visit(n)
 * function visit(node n)
 *  if node has been visited
 *    return null
 *  if node is being visited
 *    -- We have a cyclic dependency - indicate this to the caller
 *    return n
 *  mark n as being visited
 *  for ds = each set of dependencies in n
 *    node d = first dep in ds
 *    while d and not found
 *      n2 = visit(d)
 *      if n2
 *        d = next dep in ds
 *      else
 *        found = true
 *    if found
 *      replace ds with d
 *    else
 *      -- We still have a cyclic dependency.  Let the caller try to 
 *      -- resolve it.
 *      mark n as unvisited
 *      return n2
 *  mark n as visited
 *  return null
 * TODO: make the pseudocode reflect the code.
 */
static Cons *visitNode(DagNode *node, Hash *allnodes);

static DagNode *
dependencyFromSet(
    DagNode *node,
    Object *deps, 
    Hash *allnodes,
    boolean break_allowed,
    Cons **p_cycle)
{
    Int4 *depset_id = NULL;
    DagNode *dep = NULL;
    Cons *cycle;
    String *newfqn;

    *p_cycle = NULL;
    while (deps) {
	depset_id = NULL;
	if (deps->type == OBJ_CONS) {
	    dep = (DagNode *) dereference(((Cons *) deps)->car);
	    deps = ((Cons *) deps)->cdr;
	    if ((!dep) || (dep->type == OBJ_INT4)) {
		depset_id = (Int4 *) dep;
		dep = (DagNode *) dereference(deps);
		deps = NULL;
	    }
	}
	else {
	    dep = (DagNode *) dereference(deps);
	    deps = NULL;
	}

	node->cur_dep = dep;
	if (cycle = visitNode(dep, allnodes)) {
	    /* We have a cyclic dependency. */
	    if (break_allowed) {
		if (dep = attemptCycleBreak(node, dep)) {
		    newfqn = stringNew(dep->fqn->value);
		    hashAdd(allnodes, (Object *) newfqn, (Object *) dep);
		    objectFree((Object *) cycle, TRUE);
		    return dep;
		}

		if (depset_id) {
		    /* We have retried the dependency and been unable to 
		     * satisfy it, but as it is an optional dependency
		     * we can ignore it.  Only one dependency from each
		     * set *must* be satisfied, and we will check for
		     * that later. */
		    objectFree((Object *) cycle, TRUE);
		    cycle = NULL;
		}
		*p_cycle = cycle;
		return NULL;
	    }
	    if (deps) {
		/* We have another way to satisfy this dependency, so
		 * let's try it. */ 
		objectFree((Object *) cycle, TRUE);
	    }
	    else {
		*p_cycle = cycle;
		return NULL;
	    }
	}
	else {
	    if (depset_id) {
		optionalDepSetSatisfied(depset_id);
	    }
	    return dep;
	}
    }
    return dep;
}

static Cons *
visitNode(DagNode *node, Hash *allnodes)
{
    int i;
    Object *deps;
    DagNode *dep;
    Cons *result = NULL;
    switch (node->status) {
    case VISITED:
        return NULL;
    case VISITING:
        return consNew((Object *) objRefNew((Object *) node), NULL);
    }
    node->status = VISITING;
    if (node->dependencies) {
        for (i = node->dependencies->elems - 1; i >= 0; i--) {
            deps = node->dependencies->contents->vector[i];
	    dep = dependencyFromSet(node, deps, allnodes, FALSE, &result);
	    if (!dep) {
		/* We have an unhandled cyclic dependency.  Attempt a
		 * retry, allowing it to be handled. */
		objectFree((Object *) result, TRUE);
		dep = dependencyFromSet(node, deps, allnodes, TRUE, &result);
		if (!dep) {
		    if (result) {
			result = consNew((Object *) objRefNew((Object *) node),
					 (Object *) result);
			node->status = UNVISITED;
			return result;
		    }
		    if (!result) {
			// Part of ongoing work to refactor for optional deps
			//RAISE(NOT_IMPLEMENTED_ERROR, 
			//      newstr("Check this code path(1)"));
		    }
		}
	    }

	    /* Replace depset with the single dependency to which we
	     * have successfully traversed, or remove the depset if
	     * it was an unused optional dep. */ 
	    objectFree(node->dependencies->contents->vector[i], TRUE);
	    if (dep) {
		deps = (Object *) objRefNew((Object *) dep);
		node->dependencies->contents->vector[i] = (Object *) deps;
	    }
	    else {
		vectorRemove(node->dependencies, i);
	    }
        }
    }
    node->status = VISITED;
    return NULL;
}

static Object *
visitNodeInHash(Cons *entry, Object *allnodes)
{
    DagNode *node = (DagNode *) entry->cdr;
    Cons *cons;
    char *deps;
    char *errmsg;

    if (cons= visitNode(node, (Hash *) allnodes)) {
	deps = objectSexp((Object *) cons);
	objectFree((Object *) cons, TRUE);
	errmsg = newstr("Cyclic dependency in %s", deps);
	skfree(deps);
	RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg);
    }

    return (Object *) node;
}

typedef struct DepTracker {
    ObjType  type;
    Object  *deps;
    Cons    *next;
    boolean  break_attempted;
} DepTracker;


static DagNode *
first_dep(Object *deps, DepTracker *tracker)
{
    DagNode *dep;
    Cons *list;

    tracker->deps = deps;
    tracker->break_attempted = FALSE;
    if (deps->type == OBJ_CONS) {
	list = (Cons *) deps;
	if (list->car->type == OBJ_INT4) {
	    tracker->type = OBJ_INT4;
	    dep = (DagNode *) dereference(list->cdr);
	    list = NULL;
	}
	else {
	    tracker->type = OBJ_CONS;
	    dep = (DagNode *) dereference(list->car);
	    list = (Cons *) list->cdr;
	}
    }
    else {
	tracker->type = OBJ_DAGNODE;
	dep = (DagNode *) dereference(deps);
	list = NULL;
    }
    tracker->next = list;
    return dep;
}

/* The current dep being evaluated (as described in tracker) was
 * successfully visited, so if this dep is in some way optional, record
 * this so that we can later convert our almost DAG into a true DAG.
 */
static void
record_success(DagNode *node, DepTracker *tracker)
{
    Cons *record;
    Int4 *idx;
    if (tracker->type == OBJ_DAGNODE) {
	/* Nothing to do here. */
	return;
    }
    idx = int4New(node->cur_dep_idx);
    record = consNew((Object *) idx, NULL);
    if (tracker->type == OBJ_CONS) {
	record->cdr = (Object *) objRefNew((Object *) node->cur_dep);
    }
    node->chosen_options = consNew((Object *) record, 
				   (Object *) node->chosen_options);
}

static DagNode *
next_dep(DepTracker *tracker)
{
    DagNode *dep;
    tracker->break_attempted = FALSE;
    if (tracker->next) {
	dep = (DagNode *) dereference(tracker->next->car);
	tracker->next = (Cons *) tracker->next->cdr;
    }
    else {
	dep = NULL;
    }
    return dep;
}

/* Remove nodes that were apparantly sucessfully visited from the
 * visited vector, resetting them to unvisited.  This is called in the
 * event of a cyclic dependency exception beig detected, so that we may
 * attempt some remedial actions. */
static void
unvisit_nodes(Vector *visited, int elems)
{
    if (visited->elems > elems) {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("Have not yet implemented unvisit_nodes"));
    }
}

static boolean
attempt_recovery(
    DepTracker *tracker, 
    int how_hard_to_try, 
    DagNode *node,
    Hash *allnodes,
    DagNode **p_dep)
{
    if (tracker->type == OBJ_CONS) {
	/* We are in a dependency set, so try the next entry in the set. */
	if (*p_dep =  next_dep(tracker)) {
	    return TRUE;
	}
    }
    if ((how_hard_to_try > 1) && (tracker->type == OBJ_INT4)) {
	/* That was an optional dependency, try ignoring it. */
	*p_dep = NULL;
	return TRUE;
    }
    if (how_hard_to_try > 2) {
	fprintf(stderr, "SHOUD BE TRYING TO RESOLVE CYCLIC DEPS - XXXXXXXXXXXXXX\n");
	//dbgSexp(node);
	//dbgSexp(*p_dep);
    }
    return FALSE;
}



/* Breaking cyclic dependencies:
 * We use recursion to traverse the dag.  When we reach a node that has
 * no unvisited dependencies, we mark that node as visited and add it to
 * our result vector.  If we reach a node that we are already visiting,
 * we have a cyclic dependency. 
 * These can be resolved in 3 ways:
 *   1) If we have a dependency-set, we can try a different element
 *      from the set.
 *      When we return successfully from traversing the node, we must
 *      record which of the optional dependencies was used.
 *   2) If this is an optional dependency, we can ignore it.  
 *      When we return successfully from traversing the node,
 *      we must record which optional dependencies have been ignored.
 *   3) If this node has a "cycle_breaker" attribute, we can use it.
 *      This should only be done when all other mechanisms have been
 *      unsuccessfully explored.
 *   4) Ignore it
 *      This will only be an option if the allow_cycles parameter has
 *      been defined (TODO: create this parameter and make it usable).
 * When a cyclic dependency is detected, we immediately raise a 
 * TSORT_CYCLIC_DEPENDENCY exception.  This will be trapped by each
 * preceding level of recursion and will either be handled or re-raised.
 * When we are unable to handle the cyclic dependency, we reset any
 * nodes added to our result vector during our visit to the current
 * node.
 * The how_hard_to_try parameter is normally 1.   When it is 1, we will
 * only attempt method 1 above.  When it is 2, we will try methods 1 and
 * 2, and when 3, we will try all 3.
 * When recursing, the value of how_hard_to_try that is passed to
 * recursed function will be 1.  If a cyclic dependency exception is
 * raised, we may retry with an incremented value.  We can never
 * increment beyond the value we were called with.
 * If we are at the cycle node (the node at which he cycle was detected,
 * we can retry with an incremented how_hard_to_try parameter.
 * This mechanism is meant to ensure that we try harder to use method 1,
 * and only if that fails use method 2.  Similarly for method 3.
 * This is an inefficient and exhaustive method for resolving cycles but
 * simpler methods have not so far worked.
 * Possible future optimisations:
 * 1 Sort dependency lists so that dependency sets appear last, then
 *   optional dependencies.
 * 2 Check for the presence of optional dependencies and cycle_breakers
 *   along the cyclic path (which we can determine using node->cur_dep)
 *   before incrementing the how_hard_to_try parameter.
 */
static void
eliminate_cycles(DagNode *node, Hash *allnodes, 
		 Vector *visited, int how_hard_to_try)
{
    int i;
    int cur_elems = visited->elems;
    Object *deps;
    DagNode *dep;
    DepTracker tracker;
    int how_hard_to_retry = 1;

    switch (node->status) {
    case VISITED:
        return;
    case VISITING:
	/* We have detected a cycle */
	//report_cycle(node);
	RAISE(TSORT_CYCLIC_DEPENDENCY, newstr(""), node);
    }
    node->status = VISITING;
    if (node->dependencies) {
        for (i = node->dependencies->elems - 1; i >= 0; i--) {
            deps = node->dependencies->contents->vector[i];
	    dep = first_dep(deps, &tracker);
	    node->cur_dep_idx = i;
	    while (dep) {
		node->cur_dep = dep;
		BEGIN {
		    eliminate_cycles(dep, allnodes, visited, how_hard_to_retry);
		    record_success(node, &tracker);
		    dep = NULL;
		}
		EXCEPTION(ex);
		WHEN(TSORT_CYCLIC_DEPENDENCY) {
		    //fprintf(stderr, "Exception trapped ");
		    //printSexp(stderr, "visiting: ", (Object *) node);
		    //dbgSexp(dep);
		    unvisit_nodes(visited, cur_elems);

		    if (attempt_recovery(&tracker, how_hard_to_try, 
					 node, allnodes, &dep)) {
		    }
		    else if (how_hard_to_try > how_hard_to_retry) {
			/* We are allowed to retry a little harder */
			how_hard_to_retry++;
		    }
		    else if (((DagNode *) ex->param == node) &&
			     (how_hard_to_retry < 3)){
			/* We are allowed to increment how_hard_to_retry
			 * as this is the  cycle_node. */
			how_hard_to_retry++;
		    }
		    else {
			node->status = UNVISITED;
			RAISE();
		    }
		}
		WHEN_OTHERS {
		    node->status = UNVISITED;
		    RAISE();
		}
		END;
	    }
        }
    }
    node->status = VISITED;
    return;
}

static void
eliminateCycles(Hash *allnodes)
{
    Vector *nodelist = vectorFromHash(allnodes);
    Vector *visited = vectorNew(20);
    int i;
    BEGIN {
	visited->elems = 0;
	for (i = 0; i < nodelist->elems; i++) {
	    eliminate_cycles((DagNode *) nodelist->contents->vector[i], 
			     allnodes, visited, 1);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) visited, FALSE);
	objectFree((Object *) nodelist, FALSE);
    }
    END;
}


static Object *
checkOptionalDep(Cons *node_entry, Object *param)
{
    Object *satisfied = node_entry->cdr;
    return satisfied;
    if (!satisfied) {
	/* If this error starts looking likely we should somehow record
	 * details of the dependency set (which dagnode, which elements
	 * in the set). */
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("Unsatisfied optional dependency set"));
    }
    RAISE(NOT_IMPLEMENTED_ERROR,
	  newstr("Comment this."));
    return satisfied;
}


static void
check_optional_deps()
{
    Hash *hash = optionalDepsHash();
    hashEach(hash, &checkOptionalDep, NULL);
}



/* Converts the almost DAG into a DAG.  It resolves cyclic dependencies,
 * and replaces lists of dependencies with single dependencies */
static void
check_dag(Hash *allnodes)
{
    BEGIN {
	eliminateCycles(allnodes);

	//showAllDeps(allnodes);
	hashEach(allnodes, &solidifyOptions, NULL);
    }
    EXCEPTION(ex);
    WHEN(TSORT_CYCLIC_DEPENDENCY) {
	fprintf(stderr, "UNFIXED CYCLIC DEPS EXIST...\n");
    }
    END;
    hashEach(allnodes, &resetStatus, (Object *) allnodes);
    hashEach(allnodes, &visitNodeInHash, (Object *) allnodes);
    check_optional_deps();
}


static Cons *
tsortVisitNode(DagNode *node, Vector *results)
{
    Cons *result;
    Cons *depset;
    DagNode *dep;
    int i;

    switch (node->status) {
    case VISITED:
	return NULL;
    case VISITING:
	return consNew((Object *) objRefNew((Object *) node), NULL);
    }
    node->status = VISITING;
    if (node->dependencies) {
	for (i = 0; i < node->dependencies->elems; i++) {
	    depset = (Cons *) node->dependencies->contents->vector[i];
	    if (depset) {
		dep = (DagNode *) dereference(depset->car);
		if (result = tsortVisitNode(dep, results)) {
		    return consNew((Object *) objRefNew((Object *) node), 
				   (Object *) result);
		}
	    }
	}
    }
    node->status = VISITED;
    vectorPush(results, (Object *) node);
    return NULL;
}

static Object *
tsortVisitHashNode(Cons *entry, Object *results)
{
    DagNode *node = (DagNode *) entry->cdr;
    Cons *result;
    char *deps;
    char *errmsg;
    if (result = tsortVisitNode(node, (Vector *) results)) {
	deps = objectSexp((Object *) result);
	errmsg = newstr("Unresolved cyclic dependency: %s", deps);
	skfree(deps);
	objectFree((Object *) result, TRUE);
	RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg);
    }

    return NULL; /* Remove node from the hash as it will have been
		  * added to the results Vector. */
}


static Object *
tsortSetUnvisited(Cons *entry, Object *ignore)
{
    DagNode *node = (DagNode *) entry->cdr;
    node->status = UNVISITED;

    return (Object *) node;
}




static Vector *
simple_tsort(Hash *allnodes)
{
    int elems = hashElems(allnodes);
    Vector *results = vectorNew(elems);
    BEGIN {
	hashEach(allnodes, &tsortSetUnvisited, NULL);
	hashEach(allnodes, &tsortVisitHashNode, (Object *) results);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, FALSE);
	RAISE();

    }
    END;
    return results;
}






/* Record something XXXX???, based upon the ignore-contexts symbol/ */
static boolean handling_context = FALSE;

/* Do the sort. */
Vector *
gensort(Document *doc)
{
    Hash *dagnodes = NULL;
    Hash *pqnhash = NULL;
    Hash *pqnhash2 = NULL;
    Vector *sorted = NULL;
    Symbol *ignore_contexts = symbolGet("ignore-contexts");
    Symbol *simple_sort = symbolGet("simple-sort");
    
    handling_context = (ignore_contexts == NULL);

    BEGIN {
	//dbgSexp(doc);
	dagnodes = dagnodesFromDoc(doc);
	pqnhash = makePqnHash(dagnodes);
	//dbgSexp(dagnodes);
	//dbgSexp(pqnhash);
	identifyDependencies(doc, dagnodes, pqnhash);
	//showAllDeps(dagnodes);
	check_dag(dagnodes);
	//fprintf(stderr, "\n\nXX\n\n");
	//showAllDeps(dagnodes);
	if (simple_sort) {
	    sorted = simple_tsort(dagnodes);
	}
	else {
	    sorted = smart_tsort(dagnodes);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) dagnodes, TRUE);
	objectFree((Object *) pqnhash, TRUE);
    }
    END;
    return sorted;
}

static int
generationCount(DagNode *node)
{
    int count = 0;
    while (node) {
	count++;
	node = node->parent;
    }
    return count;
}

static boolean
nodeEq(DagNode *node1, DagNode *node2)
{
    if (node1 && node2) {
	return node1->dbobject == node2->dbobject;
    }
    if (node1 || node2) {
	return FALSE;
    }
    return TRUE;
}

/* Identify whether it is necessary to navigate to/from node */
static boolean
requiresNavigation(xmlNode *node)
{
    String *visit = nodeAttribute(node, "visit");
    if (visit) {
	objectFree((Object *) visit, TRUE);
	return TRUE;
    }
    return FALSE;
}

static DagNode *
getCommonRoot(DagNode *current, DagNode *target)
{
    int cur_depth = generationCount(current);
    int target_depth = generationCount(target);

    while (cur_depth > target_depth) {
	current = current->parent;
	cur_depth = generationCount(current);
    }
    while (target_depth > cur_depth) {
	target = target->parent;
	target_depth = generationCount(target);
    }
    while (!nodeEq(current, target)) {
	current = current->parent;
	target = target->parent;
    }
    return current;
}

/* Depart the current node, returning a navigation DagNode if
 * appropriate
 */
static DagNode *
departNode(DagNode *current)
{
    DagNode *navigation = NULL;
    Node node = {OBJ_XMLNODE, NULL};
    if (requiresNavigation(current->dbobject)) {
	node.node = current->dbobject;
	navigation = dagnodeNew(&node, DEPART_NODE);
    }
    return navigation;
}

/* Arrive at the target node, returning a navigation DagNode if
 * appropriate
 */
static DagNode *
arriveNode(DagNode *target)
{
    DagNode *navigation = NULL;
    Node node = {OBJ_XMLNODE, NULL};
    if (requiresNavigation(target->dbobject)) {
	node.node = target->dbobject;
	navigation = dagnodeNew(&node, ARRIVE_NODE);
    }
    return navigation;
}

/* Return the node in to's ancestry that is the direct descendant of
 * from */
static DagNode *
nextNodeFrom(DagNode *from, DagNode *to)
{
    DagNode *cur = to;
    DagNode *prev = NULL;
    while (!nodeEq(from, cur)) {
	prev = cur;
	cur = cur->parent;
    }
    return prev;
}

static Cons *
getContexts(DagNode *node)
{
    xmlNode *context_node;
    Cons *cell;
    Cons *contexts = NULL;
    String *name;
    String *value;
    String *dflt;

    if (node) {
	for (context_node = findFirstChild(node->dbobject, "context");
	     context_node;
	     context_node = findNextSibling(context_node, "context")) {
	    name = nodeAttribute(context_node, "name");
	    value = nodeAttribute(context_node, "value");
	    dflt = nodeAttribute(context_node, "default");
	    cell = consNew((Object *) name, 
			   (Object *) consNew((Object *) value, 
					      (Object *) dflt));
	    contexts = consNew((Object *) cell, (Object *) contexts);
	}
    }
    return contexts;
}

static xmlNode *
dbobjectNode(char *type, char *name)
{
    xmlNode *xmlnode = xmlNewNode(NULL, BAD_CAST "dbobject");
    char *fqn = newstr("context.%s.%s", type, name);
    xmlNewProp(xmlnode, BAD_CAST "type", BAD_CAST "context");
    xmlNewProp(xmlnode, BAD_CAST "subtype", BAD_CAST type);
    xmlNewProp(xmlnode, BAD_CAST "name", BAD_CAST name);
    xmlNewProp(xmlnode, BAD_CAST "qname", BAD_CAST name);
    xmlNewProp(xmlnode, BAD_CAST "fqn", BAD_CAST fqn);
    skfree(fqn);
    return xmlnode;
}

static DagNode *
arriveContextNode(String *name, String *value)
{
    Node dbobject = {OBJ_XMLNODE, dbobjectNode(name->value, value->value)};
    return dagnodeNew(&dbobject, ARRIVE_NODE);
}

static DagNode *
departContextNode(String *name, String *value)
{
    Node dbobject = {OBJ_XMLNODE, dbobjectNode(name->value, value->value)};
    return dagnodeNew(&dbobject, DEPART_NODE);
}

static void
addArriveContext(Vector *vec, Cons *context)
{
    String *name = (String *) context->car;
    Cons *cell2 = (Cons *) context->cdr;
    DagNode *context_node;
    /* Do not close the context, if it is the default. */
    if (objectCmp(cell2->car, cell2->cdr) != 0) {
	context_node = arriveContextNode(name, (String *) cell2->car);
	vectorPush(vec, (Object *) context_node);
	//fprintf(stderr, "SET CONTEXT %s(%s)\n", name->value,
	//	((String *) cell2->car)->value);
    }
}

static void
addDepartContext(Vector *vec, Cons *context)
{
    String *name = (String *) context->car;
    Cons *cell2 = (Cons *) context->cdr;
    DagNode *context_node;
    /* Do not close the context, if it is the default. */
    if (objectCmp(cell2->car, cell2->cdr) != 0) {
	context_node = departContextNode(name, (String *) cell2->car);
	vectorPush(vec, (Object *) context_node);
	//fprintf(stderr, "RESET CONTEXT %s(%s)\n", name->value,
	//	((String *) cell2->car)->value);
    }
}

static Cons *
getContextNavigation(DagNode *from, DagNode *target)
{
    Cons *from_contexts;
    Cons *target_contexts;
    Cons *this;
    Cons *this2;
    Cons *match;
    Cons *match2;
    String *name;
    Vector *departures = vectorNew(10);
    Vector *arrivals = vectorNew(10);
    Cons *result = consNew((Object *) departures, (Object *) arrivals);

    /* Contexts are lists of the form: (name value default) */
    from_contexts = getContexts(from);
    target_contexts = getContexts(target);
    while (target_contexts && (this = (Cons *) consPop(&target_contexts))) {
	name = (String *) this->car;
	if (from_contexts &&
	    (match = (Cons *) alistExtract(&from_contexts, 
					   (Object *) name))) {
	    /* We have the same context for both dagnodes. */
	    this2 = (Cons *) this->cdr;
	    match2 = (Cons *) match->cdr;
	    if (objectCmp(this2->car, match2->car) != 0) {
		/* Depart the old context, and arrive at the new. */
		addDepartContext(departures, match);
		addArriveContext(arrivals, this);
	    }
	    objectFree((Object *) match, TRUE);
	}
	else {
	    /* This is a new context. */
	    addArriveContext(arrivals, this);
	}
	objectFree((Object *) this, TRUE);
    }
    while (from_contexts && (this = (Cons *) consPop(&from_contexts))) {
	/* Close the final contexts.  Unless we are in a default
	 * context. */ 
	addDepartContext(departures, this);
	objectFree((Object *) this, TRUE);
    }
    return result;
}

/* Return a vector of DagNodes containing the navigation to get from
 * start to target */
Vector *
navigationToNode(DagNode *start, DagNode *target)
{
    Cons *context_nav;
    Vector *results;
    Vector *context_arrivals = NULL;
    Object *elem;
    DagNode *current = NULL;
    DagNode *next = NULL;
    DagNode *common_root = NULL;
    DagNode *navigation = NULL;

    BEGIN {
	if (handling_context) {
	    context_nav = getContextNavigation(start, target);
	    /* Context departures must happen before any other
	     * departures and arrivals after */
	    results = (Vector *) context_nav->car;
	    context_arrivals = (Vector *) context_nav->cdr;
	    objectFree((Object *) context_nav, FALSE);
	}
	else
	{
	    results = vectorNew(10);
	}
	if (start) {
	    common_root = getCommonRoot(start, target);
	    current = start;
	    while (!nodeEq(current, common_root)) {
		if ((current == start) &&
		    (current->build_type == DROP_NODE)) {
		    /* We don't need to depart from a drop node as
		     * the drop must perform the departure for us. */ 
		}
		else {
		    if (navigation = departNode(current)) {
			vectorPush(results, (Object *) navigation);
		    }
		}
		current = current->parent;
	    }
	}
	/* Now navigate from common root towards target */
	current = common_root;
	while (!nodeEq(current, target)) {
	    current = nextNodeFrom(current, target);
	    if ((current == target) &&
		(current->build_type == BUILD_NODE)) {
		/* We don't need to arrive at a build node as the build
		 * must perform the arrival for us. */
	    }
	    else {
		if (navigation = arriveNode(current)) {
		    vectorPush(results, (Object *) navigation);
		}
	    }
	}
	if (context_arrivals) {
	    /* Although this reverses the order of context departures,
	     * this should not be an issue as the order of contexts is
	     * expected to be irrelevant. */
	    while (elem = vectorPop(context_arrivals)) {
		vectorPush(results, elem);
	    }
	    objectFree((Object *) context_arrivals, FALSE);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) results, TRUE);
    }
    END;
    //dbgSexp(results);
    return results;
}

