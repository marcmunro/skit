/**
 * @file   deps.c
 * \code
 *     Copyright (c) 2011 - 2014 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for generating a Directed Acyclic Graph from the
 * dependencies specified in an xml stream.
 */


#include <string.h>
#include <limits.h>
#include "skit_lib.h"
#include "exceptions.h"

typedef struct ResolverState {
    Document *doc;
    Vector   *all_nodes;
    Hash     *by_fqn;
    Hash     *by_pqn;
    Hash     *deps_hash;
    Vector   *dependency_sets;
    int       fallback_no;
    Cons     *resolution_stack;
} ResolverState;


void
showDeps(DagNode *node)
{
    if (node) {
        printSexp(stderr, "NODE: ", (Object *) node);
	if (node->deps) {
	    printSexp(stderr, "-->", (Object *) node->deps);
	}
	else if (node->unidentified_deps) {
	    printSexp(stderr, "~~>", (Object *) node->unidentified_deps);
	}
    }
} 

void
showVectorDeps(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH (nodes, i) {
        node = (DagNode *) ELEM(nodes, i);
        showDeps(node);
    }
}


/* Like vectorPush but creates the vector if needed. 
 */
static void
myVectorPush(Vector **p_vector, Object *obj)
{
    if (!*p_vector) {
	*p_vector = vectorNew(10);
    }
    vectorPush(*p_vector, obj);
}


static void
initResolverState(volatile ResolverState *res_state)
{
    res_state->all_nodes = NULL;
    res_state->by_fqn = NULL;
    res_state->by_pqn = NULL;
    res_state->dependency_sets = NULL;
    res_state->deps_hash = NULL;
    res_state->fallback_no = 0;
    res_state->resolution_stack = NULL;
}

static Object *
hashDropContents(Cons *node_entry, Object *ignore)
{
    Object *contents = node_entry->cdr;;
    UNUSED(ignore);

    if (contents->type == OBJ_VECTOR) {
	objectFree(contents, FALSE);
    }
    return NULL;
}

/* Free the hash and any vectors that it contains, but not the DagNodes
 * contained within.
 */
static void
cleanupHash(Hash *hash)
{
    hashEach(hash, &hashDropContents, NULL);
    objectFree((Object *) hash, FALSE);
}


/*
 * Identify the type of build action expected for the supplied dbobject
 * node.
 */
static DagNodeBuildType
buildTypeForDagNode(Node *node)
{
    String *diff;
    String *action;
    String *tmp;
    String *fqn;
    char *errmsg = NULL;
    DagNodeBuildType build_type = UNSPECIFIED_NODE;

    assertNode(node);
    if (diff = nodeAttribute(node->node, "diff")) {
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
	else if (streq(diff->value, ACTIONREBUILD)) {
	    build_type = REBUILD_NODE;
	}
	else {
	    fqn = nodeAttribute(((Node *) node)->node, "fqn");
	    errmsg = newstr("buildTypeForDagnode: unexpected diff "
			    "type \"%s\" in %s", diff->value, fqn->value);
	}
	objectFree((Object *) diff, TRUE);
    }
    else if (action = nodeAttribute(node->node, "action")) { 
	tmp = action;
	action = stringLower(action);
	objectFree((Object *) tmp, TRUE);
	if (streq(action->value, ACTIONBUILD)) {
	    build_type = BUILD_NODE;
	}
	else if (streq(action->value, ACTIONDROP)) {
	    build_type = DROP_NODE;
	}
	else if (streq(action->value, ACTIONREBUILD)) {
	    build_type = REBUILD_NODE;
	}
	else if (streq(action->value, ACTIONNONE)) {
	    build_type = EXISTS_NODE;
	}
	else {
	    fqn = nodeAttribute(((Node *) node)->node, "fqn");
	    errmsg = newstr("buildTypeForDagnode: unexpected action "
			    "type \"%s\" in %s", action->value, fqn->value);
	}
	objectFree((Object *) action, TRUE);
    }
    else {
	if (dereference(symbolGetValue("build"))) {
	    if (dereference(symbolGetValue("drop"))) {
		build_type = REBUILD_NODE;
	    }
	    else {
		build_type = BUILD_NODE;
	    }
	}
	else {
	    if (dereference(symbolGetValue("drop"))) {
		build_type = DROP_NODE;
	    }
	    else {
		fqn = nodeAttribute(((Node *) node)->node, "fqn");
		errmsg = newstr("buildTypeForDagnode: cannot identify "
				"build type for node %s", fqn->value);
	    }
	}
    }
    if (errmsg) {
	objectFree((Object *) fqn, TRUE);
	RAISE(TSORT_ERROR, errmsg);
    }

    return build_type;
}


/* A TraverserFn to identify dbobject nodes, adding them to our vector.
 */
static Object *
addDagNodeToVector(Object *this, Object *vector)
{
    Node *node = (Node *) this;
    DagNode *dagnode = NULL;

    assertVector(vector);
    BEGIN {
	if (streq((char *) node->node->name, "dbobject")) {
	    dagnode = dagNodeNew(node->node, UNSPECIFIED_NODE);
	    dagnode->build_type = buildTypeForDagNode(node);
	    vectorPush((Vector *) vector, (Object *) dagnode);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) dagnode, TRUE);
    }
    END;

    return NULL;
}

/* Read the source document, creating a single Dagnode for each
 * dbobject.
 */
static Vector *
dagNodesFromDoc(Document *doc)
{
    Vector *volatile nodes = vectorNew(1000);

    BEGIN {
	(void) xmlTraverse(doc->doc->children, &addDagNodeToVector, 
			   (Object *) nodes);
    }
    EXCEPTION(ex) {
	objectFree((Object *) nodes, TRUE);
    }
    END;
    return nodes;

}

DependencyApplication
dependencyApplicationForString(String *direction)
{
    DependencyApplication result;
    if (direction) {
	if (streq(direction->value, "forwards")) {
	    result = FORWARDS;
	}
	else if (streq(direction->value, "backwards")) {
	    result = BACKWARDS;
	}
	else {
	    result = UNKNOWN_DIRECTION;
	}
	objectFree((Object *) direction, TRUE);
	return result;
    }
    return BOTH_DIRECTIONS;
}


/* Add a DagNode to a hash.  If there are already entries, the entry
 * will become a vector with node appended to it.  Key is not consumed
 * by this function.
 */
static void
addToHash(Hash *hash, String *key, DagNode *node)
{
    Object *obj;
    Vector *vec;
    String *key2;

    assertHash(hash);
    assertString(key);
    assertDagNode(node);
    if (obj = hashGet(hash, (Object *) key)) {
	if (obj->type == OBJ_VECTOR) {
	    vec = (Vector *) obj;
	    vectorPush(vec, (Object *) node);
	}
	else {
	    vec = vectorNew(10);
	    vectorPush(vec, obj);
	    vectorPush(vec, (Object *) node);
	    key2 = stringDup(key);
	    (void) hashAdd(hash, (Object *) key2, (Object *) vec);
	}
    }
    else {
	key2 = stringDup(key);
	(void) hashAdd(hash, (Object *) key2, (Object *) node);
    }
}

/* Return a Hash of Dagnodes keyed by fqn.
 */
static void
makeQnHashes(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    String *pqn;

    Hash *by_fqn = hashNew(TRUE);
    Hash *by_pqn = hashNew(TRUE);

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	assert(node->type == OBJ_DAGNODE, "Incorrect node type");
	addToHash(by_fqn, node->fqn, node);

	if (pqn = nodeAttribute(node->dbobject, "pqn")) {
	    addToHash(by_pqn, pqn, node);
	    objectFree((Object *) pqn, TRUE);
	}
    }

    res_state->by_fqn = by_fqn;
    res_state->by_pqn = by_pqn;
}


static DagNodeBuildType
mirroredBuildType(DagNodeBuildType type)
{
    switch(type) {
    case REBUILD_NODE: return DROP_NODE;
    case DIFF_NODE: return DIFFPREP_NODE;
    default: return DEACTIVATED_NODE;
    }
    return type;
}

static boolean
isPureDropSideNode(DagNode *node)
{
    assertDagNode(node);
    return (node->build_type == DROP_NODE) ||
	(node->build_type == DIFFPREP_NODE);
}

static boolean
isPureBuildSideNode(DagNode *node)
{
    assertDagNode(node);
    return (node->build_type == BUILD_NODE) ||
	(node->build_type == REBUILD_NODE) ||
	(node->build_type == DIFF_NODE) ||
	(node->build_type == EXISTS_NODE);
}

static boolean
isBuildSideNode(DagNode *node)
{
    assertDagNode(node);
    if (node->build_type == DEACTIVATED_NODE) {
	/* This is the mirror of another node, so we are a build node if
	 * the mirror is a drop node. */
	return isPureDropSideNode(node->mirror_node);
    }
    return isPureBuildSideNode(node);
}

static boolean
isDropSideNode(DagNode *node)
{
    assertDagNode(node);
    if (node->build_type == DEACTIVATED_NODE) {
	/* This is the mirror of another node, so we are a drop node if
	 * the mirror is a build node. */
	return isPureBuildSideNode(node->mirror_node);
    }
    return isPureDropSideNode(node);
}

/* We create mirror nodes for all nodes in our DAG.  Many of them will
 * be of type  DEACTIVATED_NODE.  Doing this makes things easier later
 * when we have to promote nodes to REBUILD_NODE, etc.
 */
static void
makeMirrorNode(DagNode *node, volatile ResolverState *res_state)
{
    DagNode *mirror;
    DagNode *tmp;
    Dependency *dep;

    assertDagNode(node);
    mirror = dagNodeNew(node->dbobject, 
			mirroredBuildType(node->build_type));
    mirror->mirror_node = node;
    node->mirror_node = mirror;
    vectorPush(res_state->all_nodes, (Object *) mirror);
    addToHash(res_state->by_fqn, node->fqn, mirror);

    if (!isBuildSideNode(node)) {
	tmp = mirror;
	mirror = node;
	node = tmp;
    }

    dep = dependencyNew(stringDup(node->fqn), TRUE, TRUE);
    dep->dep = mirror;
    node->unidentified_deps = vectorNew(10);
    vectorPush(node->unidentified_deps, (Object *) dep);
}


static void
makeMirrors(volatile ResolverState *res_state)
{
   int i;
    DagNode *node;

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	assertDagNode(node);
	if (!node->mirror_node) {
	    makeMirrorNode(node, res_state);
	}
    }
}

static xmlNode *
nextDependency(xmlNode *start, xmlNode *prev)
{
    xmlNode *node;
    if (prev) {
	node = firstElement(prev->next);
    }
    else {
	node = firstElement(start);
    }
    while (node && !isDepNode(node)) {
	node = firstElement(node->next);
    }
    return node;
}

static boolean
isForwards(xmlNode *depnode)
{
    DependencyApplication direction = 
	dependencyApplicationForString(directionForDep(depnode));
    return (direction == FORWARDS) || (direction == BOTH_DIRECTIONS);
}


static boolean
isBackwards(xmlNode *depnode)
{
    DependencyApplication direction = 
	dependencyApplicationForString(directionForDep(depnode));
    return (direction == BACKWARDS) || (direction == BOTH_DIRECTIONS);
}

static int
getPriority(xmlNode *node)
{
    String *priority = nodeAttribute(node, "priority");
    int result = 100;   /* 100 is the default priority */

    if (priority) {
	result = atoi(priority->value);
    }
    objectFree((Object *) priority, TRUE);
    return result;
}


static void
addDepToDepset(DependencySet *depset, Dependency *dep)
{
    assertDependencySet(depset);
    assertDependency(dep);
    dep->depset = depset;
    vectorPush(depset->deps, (Object *) dep);
}


static DependencySet *
makeDepset(DagNode *node, xmlNode *depnode, Vector *deps, boolean is_forwards)
{
    DependencySet *depset = dependencySetNew(node);
    int i;
    Dependency *dep;
    char *tmp;
    char *errmsg;

    assertDagNode(node);
    depset->fallback_expr = nodeAttribute(depnode, "fallback");
    depset->fallback_parent = nodeAttribute(depnode, "parent");
    if (depset->fallback_expr && !depset->fallback_parent) {
	objectFree((Object *) depset, TRUE);
	tmp = nodestr(depnode);
	errmsg = newstr("Must specify a parent xpath expression for "
			"the fallback in:\n  %s\n", tmp);
	skfree(tmp);
	RAISE(XML_PROCESSING_ERROR, errmsg);
    }
    depset->priority = getPriority(depnode);

    if (deps) {
	assertVector(deps);
	EACH(deps, i) {
	    dep = (Dependency *) ELEM(deps, i);
	    if (dep->is_forwards == is_forwards) {
		addDepToDepset(depset, dep);
	    }
	}
    }
    return depset;
}

static void
recordDependencyInVector(DagNode *node, xmlNode *depnode, Vector *vec,
                         volatile ResolverState *res_state)
{
    String *str;
    boolean fully_qualified;
    Dependency *dep;
    boolean qn_used = FALSE;
    DependencySet *depset;

    assertDagNode(node);
    assertVector(vec);
    if (str = nodeAttribute(depnode, "fqn")) {
        fully_qualified = TRUE;
    }
    else if (str = nodeAttribute(depnode, "pqn")) {
        fully_qualified = FALSE;
    }
    else {
        RAISE(XML_PROCESSING_ERROR,
              "No qualified name found in dependency for %s.",
              node->fqn->value);
    }

    if (node->is_fallback) {
	/* For fallbacks we always create a dependency set on the two
	 * matching nodes. */
	depset = makeDepset(node, depnode, NULL, TRUE);
        vectorPush(vec, (Object *) depset);
    }
    if (isForwards(depnode)) {
        dep = dependencyNew(str, fully_qualified, TRUE);
	if (node->is_fallback) {
	    addDepToDepset(depset, dep);
	}
	else {
	    vectorPush(vec, (Object *) dep);
	}
        qn_used = TRUE;
    }
    if (isBackwards(depnode)) {
        if (qn_used) {
            str = stringDup(str);
        }
        dep = dependencyNew(str, fully_qualified, FALSE);
	if (node->is_fallback) {
	    addDepToDepset(depset, dep);
	}
	else {
	    vectorPush(vec, (Object *) dep);
	}
    }
}


static void
recordDepNodeInVector(DagNode *node, xmlNode *depnode, Vector *vec,
		      volatile ResolverState *res_state);

static void
recordDependencySetInVector(DagNode *node, xmlNode *depnode, Vector *vec,
			    volatile ResolverState *res_state)
{
    xmlNode *this = NULL;
    DependencySet *depset;
    Vector *deps_in_depset = vectorNew(10);

    assertDagNode(node);
    assertVector(vec);
    BEGIN {
	while (this = nextDependency(depnode->children, this)) {
	    recordDepNodeInVector(node, this, deps_in_depset, res_state);
	}
	    
	if (isForwards(depnode)) {
	    depset = makeDepset(node, depnode, deps_in_depset, TRUE);
	    vectorPush(vec, (Object *) depset);
	}
	if (isBackwards(depnode)) {
	    depset = makeDepset(node->mirror_node, depnode, 
				deps_in_depset, FALSE);
	    vectorPush(vec, (Object *) depset);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) deps_in_depset, TRUE);
    }
    END;
    objectFree((Object *) deps_in_depset, FALSE);
}

static void
recordDepNodeInVector(DagNode *node, xmlNode *depnode, Vector *vec,
		      volatile ResolverState *res_state)
{
    xmlNode *this = NULL;
    char *tmp;
    char *errmsg;

    assertDagNode(node);
    assertVector(vec);
    if (isDependencies(depnode)) {   
        while (this = nextDependency(depnode->children, this)) {
            recordDepNodeInVector(node, this, vec, res_state);
        }
    }
    else if (isDependency(depnode)) {
	recordDependencyInVector(node, depnode, vec, res_state);
    }
    else if (isDependencySet(depnode)) {
	recordDependencySetInVector(node, depnode, vec, res_state);
    }
    else {
	tmp = nodestr(depnode);
	errmsg = newstr("Invalid dependency type: %s", tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
}

static void 
recordNodeDeps(DagNode *node, volatile ResolverState *res_state)
{
    xmlNode *depnode = NULL;
    Vector *volatile vec = vectorNew(20);
    int i;
    Object *obj;

    BEGIN {
	while (depnode = nextDependency(node->dbobject->children, depnode)) {
	    recordDepNodeInVector(node, depnode, vec, res_state);
	    EACH(vec, i) {
		if (obj = ELEM(vec, i)) {
		    if (obj->type == OBJ_DEPENDENCY) {
			myVectorPush(&node->unidentified_deps, obj);
		    }
		    else if (obj->type == OBJ_DEPENDENCYSET) {
			vectorPush(res_state->dependency_sets, obj);
		    }
		    else {
			dbgSexp(obj);
			RAISE(TSORT_ERROR, 
			      newstr("unhandled dependency type (%d) in "
				     "recordNodeDeps()", obj->type));
		    }
		}
	    }
	    vec->elems = 0;  /* Reset our vector. */
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) vec, TRUE);
    }
    END;
}


/* Record all dependencies without fully identifying them.  This creates
 * unidentified_defs vectors in each dagnode and DependencySets in
 * resolver_state.  The unidentified dependencies contain only a qn
 * entry and no dep reference.
 */
static void
recordDependencies(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    res_state->dependency_sets = vectorNew(res_state->all_nodes->elems * 2);

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	if (node->unidentified_deps) {
	    /* node->unidentified_deps will have been created for the
	     * build-side node of each mirrored pair and we only want to
	     * recordNodeDeps() for the build-side.  */
	    recordNodeDeps(node, res_state);
	}
    }
}


static Object *
findDepsByQn(String *qn, boolean qn_is_full, volatile ResolverState *res_state)
{
    if (qn_is_full) {
	return hashGet(res_state->by_fqn, (Object *) qn);
    }
    else {
	return hashGet(res_state->by_pqn, (Object *) qn);
    }
}


static Object *
append(Object *first, Object *next)
{
    Vector *vec;
    if (first) {
	if (first->type == OBJ_VECTOR) {
	    vec = (Vector *) first;
	}
	else {
	    vec = vectorNew(10);
	    vectorPush(vec, first);
	}
	vectorPush(vec, next);
	return (Object *) vec;
    }
    return next;
}

/* Given a vector of possible matches for a dependency, return the
 * smallest set that is approriate.  If the set is a single element,
 * return it as a DagNode *.  Note that the input vector is not to be
 * freed as is is part of one the by_Xqn hashes in resolver_state.
 */
static Object *
getAppropriateDepsFromVector(
    Vector *vec, 
    Dependency *dep)
{
    int i;
    DagNode *this;
    Object *result = NULL;

    EACH(vec, i) {
	this = (DagNode *) ELEM(vec, i);
	assertDagNode(this);

	if ((dep->is_forwards && isBuildSideNode(this)) ||
	    ((!dep->is_forwards) && isDropSideNode(this)))
	{
	    result = append(result, (Object *) this);
	}
    }
    return result;
}

static Dependency *
dupDependency(Dependency *dep) {
    String *qn = stringDup(dep->qn);
    Dependency *result = dependencyNew(qn, dep->qn_is_full, dep->is_forwards);

    assertDependency(dep);
    result->dep = dep->dep;
    return result;
}

static boolean
nodeIsActive(DagNode *node)
{
    assertDagNode(node);
    return (node->build_type != EXISTS_NODE) &&
	(node->build_type != DEACTIVATED_NODE);
}

static boolean
addOneDep(
    DagNode *node, 
    Dependency *dep, 
    DagNode *depnode,
    boolean make_copy,
    volatile ResolverState *res_state,
    DependencySet *depset)
{
    Dependency *this_dep;
    
    assertDagNode(node);
    assertDependency(dep);
    assertDagNode(depnode);

    if ((node->build_type != DEACTIVATED_NODE) &&
	(depnode->build_type != DEACTIVATED_NODE))
    {
	if ((dep->is_forwards && isBuildSideNode(depnode)) ||
	    ((!dep->is_forwards) && isDropSideNode(depnode)))
	{
	    if (make_copy) {
		this_dep = dupDependency(dep);
	    }
	    else {
		this_dep = dep;
	    }
	    
	    if (depset && !this_dep->depset) {
		addDepToDepset(depset, this_dep);
	    }
	    
	    if (this_dep->is_forwards) {
		this_dep->dep = depnode;
		myVectorPush(&(node->deps), (Object *) this_dep);
	    }
	    else {
		this_dep->dep = node;
		this_dep->from = depnode;
		myVectorPush(&(depnode->deps), (Object *) this_dep);
	    }
	    return TRUE;
	}
    }
    return make_copy;
}

static boolean
addFoundDeps(
    DagNode *node, 
    Dependency *dep, 
    Object *deps, 
    boolean make_copy,
    volatile ResolverState *res_state,
    DependencySet *depset)
{
    Vector *vec;
    int i;

    assertDagNode(node);
    assertDependency(dep);

    if (deps->type == OBJ_VECTOR) {
	if (!depset) {
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("not implemented - vectors without depsets "
			 "in addFoundDeps."));
	    depset = dependencySetNew(node);
	    vectorPush(res_state->dependency_sets, (Object *) depset);
	}
	vec = (Vector *) deps;
	EACH(vec, i) {
	    make_copy = addFoundDeps(node, dep, ELEM(vec, i),
				     make_copy, res_state, depset);
	}
	dbgSexp(depset);
	objectFree((Object *) vec, FALSE);

	return TRUE;
    }
    else if (deps->type == OBJ_DAGNODE) {
	return addOneDep(node, dep, (DagNode *) deps, 
			 make_copy, res_state, depset);
    }
    return make_copy;
}


/* Attempt to fully identify a dependency (set the dep attibute),
 * copying the dep to node->deps (and/or node->mirror_node->deps).
 * The result informs the caller whether any matching deps were found,
 * regarless of whether they were used.
 */
static boolean
identifyDep(DagNode *node, Dependency *dep, volatile ResolverState *res_state)
{
    Object *found;
    boolean used = FALSE;

    assertDagNode(node);
    assertDependency(dep);

    if (dep->dep) {
	/* dep has already been identified, so nothing more to do. */
	myVectorPush(&(node->deps), (Object *) dep);
    }
    else {
	found = findDepsByQn(dep->qn, dep->qn_is_full, res_state);
	if (!found) {
	    return FALSE;
	}
	assertVector(found);
	found = getAppropriateDepsFromVector((Vector *) found, dep);

	if (found) {
	    used = addFoundDeps(node, dep, found, FALSE, res_state, NULL);
	}
	if (!used) {
	    objectFree((Object *) dep, TRUE);
	}
    }
    return TRUE;
}


static void
identifyNodeDeps(DagNode *node, volatile ResolverState *res_state)
{
    Dependency *dep;
    DagNode *for_node;
    int i;

    assertDagNode(node);
    EACH(node->unidentified_deps, i) {
	if (dep = (Dependency *) ELEM(node->unidentified_deps, i)) {
	    assertDependency(dep);
	    for_node = (dep->is_forwards)? node: node->mirror_node;
		
	    if (identifyDep(for_node, dep, res_state)) {
		ELEM(node->unidentified_deps, i) = NULL;
	    }
	    else {
		RAISE(XML_PROCESSING_ERROR,
		      newstr("identifyNodeDeps: "
			     "cannot find dependency for %s in %s,",
			     dep->qn->value, node->fqn->value));
	    }
	}
    }
}

static int
cmpDepset(const void *p1, const void *p2)
{
    DependencySet **p_ds1 = (DependencySet **) p1;
    DependencySet **p_ds2 = (DependencySet **) p2;
    return (*p_ds1)->priority - (*p_ds2)->priority;
}

static void
sortDepsets(volatile ResolverState *res_state)
{
    qsort(res_state->dependency_sets->contents->vector, 
	  res_state->dependency_sets->elems, 
	  sizeof(Vector *), cmpDepset);
}

static void
closeUpDepsetArray(Vector *deps)
{
    Object *obj;
    Dependency *dep;
    int i;
    int write_idx = 0;

    if (deps) {
	EACH(deps, i) {
	    if (obj = ELEM(deps, i)) {
		dep = (Dependency *) dereference(obj);
		if (dep->dep) {
		    ELEM(deps, write_idx) = obj;
		    write_idx++;
		}
		else {
		    /* Remove and free this object. */
		    objectFree(obj, TRUE);
		}
	    }
	}
	deps->elems = write_idx;
    }
}

/* Remove dependencies from depset where no actual dep has been found. */
static void
closeUpDepset(DependencySet *depset)
{
    assertDependencySet(depset);
    closeUpDepsetArray(depset->deps);
    closeUpDepsetArray(depset->entangled_deps);
}


static xmlNode *
getParentByXpath(Document *doc, DagNode *node, String *parent_expr)
{
    xmlXPathObject *obj;
    xmlNodeSet *nodeset;
    xmlNode *result = NULL;

    assertDoc(doc);
    assertDagNode(node);
    assertString(parent_expr);

    if (obj = xpathEval(doc, node->dbobject, parent_expr->value)) {
	nodeset = obj->nodesetval;
	if (nodeset && nodeset->nodeNr) {
	    result = nodeset->nodeTab[0];
	}
	xmlXPathFreeObject(obj);
    }

    return result;
}


/* 
 * Create a simple fallback node, which we wil place into its own
 * document.  This will then be processed by the fallbacks.xsl script to
 * create a fully formed dbobject which will then be added to our 
 * document and to various hashes and vectors.
 */
static xmlNode *
makeXMLFallbackNode(String *fqn, volatile ResolverState *res_state)
{
    xmlNode  *fallback;
    xmlNode  *root;
    xmlNode  *dbobject;
    xmlDoc   *xmldoc;
    Document *doc;
    Document *fallback_processor;

    fallback = xmlNewNode(NULL, BAD_CAST "fallback");
    xmlNewProp(fallback, BAD_CAST "fqn", BAD_CAST fqn->value);
    xmldoc = xmlNewDoc(BAD_CAST "1.0");
    xmlDocSetRootElement(xmldoc, fallback);
    doc = documentNew(xmldoc, NULL);
    BEGIN {
	docStackPush(doc);
	fallback_processor = getFallbackProcessor();
	applyXSL(fallback_processor);
    }
    EXCEPTION(ex) {
	doc = docStackPop();
	objectFree((Object *) doc, TRUE);
	RAISE();
    }
    END;
    doc = docStackPop();
    root = xmlDocGetRootElement(doc->doc);
    dbobject = xmlCopyNode(getNextNode(root->children), 1);
    objectFree((Object *) doc, TRUE);
    if (!dbobject) {
	RAISE(TSORT_ERROR, newstr("Failed to create fallback for %s.",
				  fqn->value));
    }

    /* Add the new node into our source document, so that its memory
     * will not be leaked. */
    root = xmlDocGetRootElement(res_state->doc->doc);
    xmlAddChild(root, dbobject);
    return dbobject;
}

static DagNode *
initFallbackDagNode(
    xmlNode *dbobj, 
    DagNode *parent,
    DagNodeBuildType build_type,
    volatile ResolverState *res_state)
{
    DagNode *fallback;
    char *new;

    assertDagNode(parent);
    fallback = dagNodeNew(dbobj, build_type);
    fallback->is_fallback = TRUE;
    fallback->parent = parent;

    fallback->deps = vectorNew(10);

    vectorPush(res_state->all_nodes, (Object *) fallback);
    addToHash(res_state->by_pqn, fallback->fqn, fallback);

    /* Now add a suffix to the actual fqn - this allows us to tell
     * instances apart. */
    new = newstr("%s.%d", fallback->fqn->value, res_state->fallback_no);
    skfree(fallback->fqn->value);
    fallback->fqn->value = new;
    addToHash(res_state->by_fqn, fallback->fqn, fallback);

    return fallback;
}


static int 
dependencySetsForFallback(
    DagNode *fallback, volatile ResolverState *res_state)
{
    int i;
    int count = 0;
    DependencySet *depset;
    EACH(res_state->dependency_sets, i) {
	depset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	assertDependencySet(depset);
	if (depset->definition_node == fallback) {
	    count++;
	}
    }
    return count;
}

static void
depsetsIntoDeps(DependencySet *depset, volatile ResolverState *res_state)
{
    int i;
    Dependency *dep;
    boolean added;
    Object *found;

    assertDependencySet(depset);
    if (depset->deps->elems) {
	EACH(depset->deps, i) {
	    if (dep = (Dependency *) ELEM(depset->deps, i)) {
		if ((dep->type == OBJ_OBJ_REFERENCE) || dep->dep) {
		    /* This dependency set has already been processed,
		     * probably as part of fallback handling.  Since it
		     * has already been set up, we have nothing more to
		     * do. */
		    return;
		}
		added = FALSE;
		found = findDepsByQn(dep->qn, dep->qn_is_full, res_state);
		if (found) {
		    assertVector(found);
		    found = getAppropriateDepsFromVector(
			(Vector *) found, dep);
		}
		if (found) {
		    added = addFoundDeps(depset->definition_node, dep, found,
					 FALSE, res_state, depset);
		}
		if (added) {
		    /* Replace the dependency in the depset with a
		     * reference in order to prevent it from being freed
		     * multiple times. */
		    ELEM(depset->deps, i) = (Object *) 
			objRefNew((Object *) dep);
		}
	    }
	}
    }
}


static DagNode *
makeFallbackDagNodeWithDeps(
    xmlNode *dbobj, 
    DagNode *parent,
    DagNodeBuildType build_type,
    volatile ResolverState *res_state)
{
    DagNode *fallback;
    int new_depsets;
    int i;
    DependencySet *newdepset;

    fallback = initFallbackDagNode(dbobj, parent, build_type, res_state);
    recordNodeDeps(fallback, res_state);
    identifyNodeDeps(fallback, res_state);
    new_depsets = dependencySetsForFallback(fallback, res_state);
    for (i = res_state->dependency_sets->elems - new_depsets; 
	 i < res_state->dependency_sets->elems; i++)
    {
	newdepset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	depsetsIntoDeps(newdepset, res_state);
    }
    return fallback;
}

static void
entangleDepsets(DependencySet *target, DependencySet *source)
{
    int i;
    Dependency* deptarg;
    Dependency* depsrc;

    assertDependencySet(target);
    assertDependencySet(source);
    if (target->deps->elems != source->deps->elems) {
	dbgSexp(target);
	dbgSexp(source);
	RAISE(TSORT_ERROR, 
	      newstr("Incompatible number of elements for depset "
		     "entanglement."));
    }

    EACH (target->deps, i) {
	deptarg = (Dependency *) dereference(ELEM(target->deps, i));
	depsrc = (Dependency *) dereference(ELEM(source->deps, i));
	assertDependency(deptarg);
	assertDependency(depsrc);

	if (!streq(deptarg->qn->value, depsrc->qn->value)) {
	    dbgSexp(deptarg);
	    dbgSexp(depsrc);
	    RAISE(TSORT_ERROR, 
		  newstr("Dependencies incompatible for entanglement."));
	}
	depsrc->depset = target;
    }
    target->entangled_deps = source->deps;
    source->deps = NULL;
}

static void
addEntangledDeps(DependencySet *depset, Object *obj1, Object *obj2)
{
    Dependency *dep1 = (Dependency *) dereference(obj1);
    Dependency *dep2 = (Dependency *) dereference(obj2);

    assertDependencySet(depset);
    assertDependency(dep1);
    assertDependency(dep2);
    
    if (!depset->entangled_deps) {
	depset->entangled_deps = vectorNew(10);
    }
    if (depset->deps->elems != depset->entangled_deps->elems) {
	RAISE(TSORT_ERROR, 
	      newstr("Incompatible number of elements for depset "
		     "entanglement expansion."));
    }
    dep1->depset = depset;
    dep2->depset = depset;
    vectorPush(depset->deps, obj1);
    vectorPush(depset->entangled_deps, obj2);
}

static xmlNode *
makeFallbackDbobjectNode(
    DependencySet *depset, 
    volatile ResolverState *res_state)
{
    xmlNode *dbobj;
    assertDependencySet(depset);
    dbobj = makeXMLFallbackNode(depset->fallback_expr, res_state);

    if (!dbobj) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Failed to create fallback: \"%s\"", 
		     depset->fallback_expr->value));
    }
    return dbobj;
}

static DagNode *
makeParentDagNode(DependencySet *depset, volatile ResolverState *res_state)
{
    xmlNode *parent_node;
    String *fqn;
    Vector *found;
    DagNode *parent;

    parent_node = getParentByXpath(res_state->doc, depset->definition_node, 
				   depset->fallback_parent);
    if (!parent_node) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Failed to find parent element for fallback: \"%s\"",
		     depset->fallback_parent->value));
    }

    fqn = nodeAttribute(parent_node, "fqn");
    found = (Vector *) findDepsByQn(fqn, TRUE, res_state);
    objectFree((Object *) fqn, TRUE);

    if (!found) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Failed to find parent dagnode for node: \"%s\"", 
		     fqn->value));
    }
    assert(found->type == OBJ_VECTOR, "Found must be a vector");
    parent = (DagNode *) ELEM(found, 0);
    assert(parent && parent->type == OBJ_DAGNODE, "parent must be a dagnode");

    return parent;
}


static void
entangleFallbackPair(
    DagNode *fallback, 
    DagNode *endfallback,
    volatile ResolverState *res_state)
{
    int new_depsets = dependencySetsForFallback(fallback, res_state);
    int i;
    DependencySet *fb_depset;
    DependencySet *efb_depset;

    fallback->mirror_node = endfallback;
    endfallback->mirror_node = fallback;

    res_state->dependency_sets->elems -= new_depsets;
    for (i = res_state->dependency_sets->elems - new_depsets;
	 i < res_state->dependency_sets->elems; i++) 
    {
	fb_depset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	efb_depset = (DependencySet *) ELEM(res_state->dependency_sets, 
					    i + new_depsets);
	entangleDepsets(fb_depset, efb_depset);
	objectFree((Object *) efb_depset, FALSE);
    }
}

static DagNode *
createNewFallbackPair(DependencySet *depset, volatile ResolverState *res_state)
{
    xmlNode *dbobj;
    DagNode *parent;
    DagNode *fallback;
    DagNode *endfallback;

    assertDependencySet(depset);

    dbobj = makeFallbackDbobjectNode(depset, res_state);
    parent = makeParentDagNode(depset, res_state);

    res_state->fallback_no++;
    fallback = makeFallbackDagNodeWithDeps(dbobj, parent, 
					   FALLBACK_NODE, res_state);
    endfallback = makeFallbackDagNodeWithDeps(dbobj, parent, 
					      ENDFALLBACK_NODE, res_state);

    entangleFallbackPair(fallback, endfallback, res_state);
    return fallback;
}

static Dependency *
makeDepForFallback(DagNode *fallback, DagNode *dep_node, boolean is_forwards)
{
    String *qn = stringDup(fallback->fqn);
    Dependency *dep = dependencyNew(qn, TRUE, is_forwards);
    assertDagNode(fallback);
    assertDagNode(dep_node);
    dep->dep = (DagNode *) dep_node;
    return dep;
}


/* Add dependency to fallback from depset. 
 */
static void
addFallbackDeps(DependencySet *depset, DagNode *fallback)
{
    DagNode *defn_node = depset->definition_node;
    DagNode *endfallback;
    Dependency *fallback_dep;
    Dependency *endfallback_dep;

    assertDependencySet(depset);
    assertDagNode(fallback);
    endfallback = fallback->mirror_node;

    fallback_dep = makeDepForFallback(fallback, fallback, TRUE);
    endfallback_dep = makeDepForFallback(endfallback, defn_node, FALSE);
    myVectorPush(&defn_node->deps, (Object *) fallback_dep);
    myVectorPush(&endfallback->deps, (Object *) endfallback_dep);

    addEntangledDeps(depset, 
		     (Object *) objRefNew((Object *) fallback_dep), 
		     (Object *) objRefNew((Object *) endfallback_dep));
}

static Object *
findFallbacksByQn(String *qn, volatile ResolverState *res_state)
{
    Object *found;
    Object *result = NULL;
    DagNode *node;
    int i;

    if (found = findDepsByQn(qn, FALSE, res_state)) {
	if (found->type == OBJ_VECTOR) {
	    EACH(((Vector *) found), i) {
		node = (DagNode *) ELEM(((Vector *) found), i);
		assertDagNode(node);
		if (node->build_type == FALLBACK_NODE) {
		    result = append(result, (Object *) node);
		}
	    }
	}
    }
    return result;
}

/* Count how many fallbacks appear in the given depset.  There is
 * supposed to be a limit of 2 (1 for the build side of the DAG and one
 * for the drop side).
 */
static int
fallbacksInDepset(DependencySet *depset)
{
    Dependency *dep;
    int count = 0;
    int i;
    assertDependencySet(depset);
    for (i = depset->deps->elems - 1; i >= 0; i--) {
	dep = (Dependency *) dereference(ELEM(depset->deps, 
					      depset->deps->elems - 1));
	assertDependency(dep);
	if (dep->dep->build_type == FALLBACK_NODE) {
	    count++;
	}
	else {
	    break;
	}
    }
    return count;
}

static DagNode *
fallbackNodeFor(DagNode *node)
{
    assertDagNode(node);
    if (node->build_type == FALLBACK_NODE) {
	return node;
    }
    else if (node->build_type == ENDFALLBACK_NODE) {
	return node->mirror_node;
    }
    return FALSE;
}

static boolean
fallbackIsInDepset(DependencySet *depset, DagNode *node)
{
    Dependency *dep;
    int i;

    assertDependencySet(depset);
    assertDagNode(node);

    for (i = depset->deps->elems - 1; i >= 0; i--) {
	dep = (Dependency *) dereference(ELEM(depset->deps, 
					      depset->deps->elems - 1));
	if (dep->dep && (node == fallbackNodeFor(dep->dep))) {
	    return TRUE;
	}
    }
    return FALSE;
}

static DagNode *
lastFallbackFromDepset(DependencySet *depset)
{
    Dependency *dep;
    int i;

    assertDependencySet(depset);
    for (i = depset->deps->elems - 1; i >= 0; i--) {
	dep = (Dependency *) dereference(ELEM(depset->deps, 
					      depset->deps->elems - 1));
	assertDependency(dep);
	if (dep->dep->is_fallback) {
	    return dep->dep;
	}
    }
    return NULL;
}

/* Return, from found, only those dagnodes that are fallbacks and do not
 * appear already in depset.
 */
static Object *
unusedFallbacks(DependencySet *depset, Object *found)
{
    Object *result = NULL;
    int i;
    DagNode *this;

   assertDependencySet(depset);

    if (found->type == OBJ_VECTOR) {
	EACH(((Vector *) found), i) {
	    this = (DagNode *) ELEM(((Vector *) found), i);
	    if (this->is_fallback && !fallbackIsInDepset(depset, this)) {
		result = append(result, (Object *) this);
	    }
	}
	objectFree((Object *) found, FALSE);
    }
    else if (found->type == OBJ_DAGNODE) {
	if (!fallbackIsInDepset(depset, (DagNode *) found)) {
	    return found;
	}
    }
    return result;
}

static void
addDepBetweenFallbacks(DagNode *prev, DagNode *this)
{
    Dependency *dep;
    String *qn;
    assertDagNode(prev);
    assertDagNode(this);

    qn = stringNew(this->mirror_node->fqn->value);
    dep = dependencyNew(qn, TRUE, TRUE);
    dep->dep = this->mirror_node;
    myVectorPush(&prev->deps, (Object *) dep);
}

static void
addNewFallbackToDepset(
    DependencySet *depset, 
    volatile ResolverState *res_state)
{
    DagNode *fallback = createNewFallbackPair(depset, res_state);
    addFallbackDeps(depset, fallback);
}

static void
linkFallbackToDepset(
    DependencySet *depset, 
    volatile ResolverState *res_state)
{
    Object *found;
    int existing_fallbacks;
    DagNode *prev_fallback;
    DagNode *new_fallback;

    assertDependencySet(depset);
    if (depset->fallback_expr) {
	if (found = findFallbacksByQn(depset->fallback_expr, res_state)) {
	    found = unusedFallbacks(depset, found);
	}
	if (found) {
	    assertDagNode(found);
	    addFallbackDeps(depset, (DagNode *) found);
	}
	else {
	    existing_fallbacks = fallbacksInDepset(depset);
	    if (existing_fallbacks < 2) {
		if (existing_fallbacks) {
		    prev_fallback = lastFallbackFromDepset(depset);
		}
		addNewFallbackToDepset(depset, res_state);
		if (existing_fallbacks) {
		    new_fallback = lastFallbackFromDepset(depset);
		    addDepBetweenFallbacks(prev_fallback, new_fallback);
		}
	    }
	}
    }
}

/* On entry to this function, most depsets will have direct references
 * to their dependencies.  On exit they must contain references.  This
 * is because there must be only one direct reference to each dependency
 * to avoid multiple frees.  When most depsets are initially created
 * their deps are not recorded anywhere else (that is the purpose of
 * this function), so it is necessary to record them directly in their
 * depsets.
 */
static void
identifyDepsetDeps(DependencySet *depset, volatile ResolverState *res_state)
{
    assertDependencySet(depset);
    depsetsIntoDeps(depset, res_state);
    closeUpDepset(depset);

    if (!depset->deps->elems) {
	linkFallbackToDepset(depset, res_state);
    }
}



/* Identify the dependencies of depsets in priority order.  depsets
 * should be prioritised so that some fallbacks will prove to be
 * unnecessary once other fallbacks are activated.
 */
static void
identifyDepsForDepsets(volatile ResolverState *res_state)
{
    int i;
    DependencySet *depset;

    sortDepsets(res_state);

    EACH(res_state->dependency_sets, i) {
	depset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	if (depset->definition_node->build_type != DEACTIVATED_NODE) {
	    identifyDepsetDeps(depset, res_state);
	}	
    }
}


/* Identify the dependency objects from the dependencies' qualified
 * names.  For each found dependency, this creates a new, suitably
 * ordered, dependency in node->deps.  
 */
static void
identifyDependencies(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	if (node->unidentified_deps) {
	    /* Only process nodes that have directly been assigned
	     * dependencies.  The mirror nodes are handled at the same
	     * time as the primary node.  */
	    identifyNodeDeps(node, res_state);
	}
    }

    identifyDepsForDepsets(res_state);
}


/* Predicate identifying whether we can deactivate a dependency. */
static boolean
depMustBeDeactivated(DagNode *node, Dependency *dep)
{
    assertDagNode(node);
    assertDependency(dep);

    if (!nodeIsActive(node) || !nodeIsActive(dep->dep)) {
	return TRUE;
    }
    if (dep->depset) {
	return dep->depset->deactivated;
    }
    return FALSE;
}

static void
eliminateDepsInVector(Vector *vec)
{
    int i;
    Object *found;

    assertVector(vec);

    EACH(vec, i) {
	found = ELEM(vec, i);
	objectFree((Object *) found, FALSE);
	ELEM(vec, i) = NULL;
    }
}

static void
eliminateDep(Dependency *dep)
{
    assertDependency(dep);

    if (dep->depset) {
	dep->depset->deactivated = TRUE;
	eliminateDepsInVector(dep->depset->deps);
	if (dep->depset->entangled_deps) {
	    eliminateDepsInVector(dep->depset->entangled_deps);
	}
    }
    objectFree((Object *) dep, TRUE);
}

/* Eliminate dependencies where one end or the other is in inactive node
 * (such as an EXISTS_NODE).
 */
static void
cleanupDependencies(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    int j;
    Dependency *dep;
    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	EACH(node->deps, j) {
	    if (dep = (Dependency *) ELEM(node->deps, j)) {
		if (depMustBeDeactivated(node, dep)) {
		    eliminateDep(dep);
		    ELEM(node->deps, j) = NULL;
		}
	    }
	}
    }
}


static DagNode *
tryNextDepFromDepset(
    DependencySet *depset, 
    DagNode *cycle_node, 
    volatile ResolverState *res_state)
{
    Dependency *chosen_dep;
    DagNode *depnode;

    assertDependencySet(depset);
    assertDagNode(cycle_node);
    depset->chosen_dep++;
    if (depset->chosen_dep < depset->deps->elems) {
	chosen_dep = (Dependency *) dereference(ELEM(depset->deps, 
						     depset->chosen_dep));

	assertDependency(chosen_dep);

	depnode = chosen_dep->is_forwards? 
	    chosen_dep->dep: chosen_dep->from;

	if (depnode->status == UNVISITED) {
	    return NULL;
	}
	return depnode;
    }

    if (depset->fallback_expr) {
	linkFallbackToDepset(depset, res_state);
	//RAISE(NOT_IMPLEMENTED_ERROR, newstr("TEST THIS(17)"));

	return NULL;
    }

    depset->chosen_dep = 0;
    return cycle_node;
}

static Cons *
pushVisitNode(Cons *stack, DagNode *node)
{
    Cons *new = consNew((Object *) node, (Object *) stack);
    return new;
}

static Cons *
pushDepsetState(Cons *stack, DependencySet *depset)
{
    Int4 *chosen_dep = int4New(depset->chosen_dep);
    Cons *entry = consNew((Object *) depset, (Object *) chosen_dep);
    Cons *new = consNew((Object *) entry, (Object *) stack);
    return new;
}

static void
recordNodeVisit(DagNode *node, volatile ResolverState *res_state)
{
    res_state->resolution_stack = 
	pushVisitNode(res_state->resolution_stack, node);
}

static void
recordDepsetState(DependencySet *depset, volatile ResolverState *res_state)
{
    res_state->resolution_stack = 
	pushDepsetState(res_state->resolution_stack, depset);
}

static Cons *
popResolutionStack(volatile ResolverState *res_state)
{
    Cons *result = res_state->resolution_stack;
    if (result) {
	res_state->resolution_stack = (Cons *) result->cdr;
    }
    return result;
}

/* Return the node with which we were called, or a node that is further
 * down the stack, or already processed. */
static DagNode *
unwindToNode(DagNode *node, volatile ResolverState *res_state)
{
    Cons *head;
    boolean found = FALSE;
    DagNode *this_node;
    Cons *entry;
    DependencySet *depset = NULL;
    Int4 *chosen_dep;

    while (head = popResolutionStack(res_state)) {
	assertCons(head);
	if (head->car->type == OBJ_DAGNODE) {
	    this_node = (DagNode *) head->car;
	    node->status = UNVISITED;
	    found = (this_node == node);
	}
	else {
	    entry = (Cons *) head->car;
	    assertCons(entry);
	    depset = (DependencySet *) entry->car;
	    assertDependencySet(depset);
	    chosen_dep = (Int4 *) entry->cdr;
	    assertInt4(chosen_dep);
	    depset->chosen_dep = chosen_dep->value;
	    objectFree((Object *) chosen_dep, TRUE);
	    objectFree((Object *) entry, FALSE);
	}
	objectFree((Object *) head, FALSE);
	if (found) {
	    break;
	}
    }

    if (depset) {
	/* We have successfully unwound the stack back to node.  At this
	 * point, if we continue from here, nothing will change and we
	 * will simply retry the previous resolution attempt exactly,
	 * wwhich will achieve nothing.  What we must do now is try the
	 * next element for the last depset on the stack that we just
	 * unwound.  If that requires us to activate a dependency from
	 * an already visited node, then we will need to further unwind
	 * our state back to that node.  That further unwinding is the
	 * responsibility of our caller. */

	this_node = tryNextDepFromDepset(depset, node, res_state);
	if (this_node == node) {
	    dbgSexp(node);
	    RAISE(TSORT_ERROR, newstr("I think we are hosed!"));
	}
    }

    return this_node;
}

static void
clearResolutionStack(volatile ResolverState *res_state)
{
    Cons *head;
    Cons *entry;

    while (head = popResolutionStack(res_state)) {
	assertCons(head);
	if (head->car->type == OBJ_CONS) {
	    entry = (Cons *) head->car;
	    assertCons(entry);
	    entry->car = NULL;
	    objectFree((Object *) entry, TRUE);
	}
	objectFree((Object *) head, FALSE);
    }
}

static void
cleanUpResolverState(volatile ResolverState *res_state)
{
    int i;
    int j;
    DagNode *node;
    Object *obj;

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	assertDagNode(node);
	EACH(node->unidentified_deps, j) {
	    objectFree(ELEM(node->unidentified_deps, j), TRUE);
	    ELEM(node->unidentified_deps, j) = NULL;
	}
	EACH(node->deps, j) {
	    if (obj = ELEM(node->deps, j)) {
		if (obj->type != OBJ_DAGNODE) {
		    objectFree(ELEM(node->deps, j), TRUE);
		    ELEM(node->deps, j) = NULL;
		}
	    }
	}
    }
    cleanupHash(res_state->by_fqn);
    res_state->by_fqn = NULL;
    cleanupHash(res_state->by_pqn);
    res_state->by_pqn = NULL;
    objectFree((Object *) res_state->dependency_sets, TRUE);
    res_state->dependency_sets = NULL;
    objectFree((Object *) res_state->deps_hash, TRUE);
    res_state->deps_hash = NULL;
    clearResolutionStack(res_state);
}



#define DEBUG_RESOLVER 1
#ifdef DEBUG_RESOLVER
#define DEPTH , int depth
#define DEPTH0 , 1
#define NEXDEPTH , depth+1
#define PPREFIX(x) fprintf(stderr, "%*s%s", depth, " ", x);
#define PSEXP(x) dbgSexp(x);
#define PPSEXP(x, y) printSexp(stderr, x, (Object *) y);
#else
#define DEPTH
#define DEPTH0
#define NEXDEPTH
#define PPREFIX
#define PSEXP(x)
#define PPSEXP(x, y)
#endif

static DagNode *
resolveNode(DagNode *node, volatile ResolverState *res_state DEPTH);

static DagNode *
resolveDeps(DagNode *node, volatile ResolverState *res_state DEPTH)
{
    int i;
    Dependency *dep;
    DagNode *cycle_node;

    assertDagNode(node);
    PPREFIX("resolveDeps():") PSEXP(node);
    EACH (node->deps, i) {
	if (dep = (Dependency *) ELEM(node->deps, i)) {
	    assertDependency(dep);
	    node->cur_dep = i;
	    if (dep->depset) {
		PPREFIX("    [DEPSET HERE]\n");
		if (!depIsActive(dep)) {
		    /* This is not the chosen_dep from the depset. */
		    PPREFIX("    [DEPSET HERE - inactive dep]\n");
		    continue;
		}
		PPREFIX("    [DEPSET HERE]\n");
	    }
	    //printSexp(stderr, "Resolving ", (Object *) dep);
	    if (cycle_node = resolveNode(dep->dep, res_state NEXDEPTH)) {
		if (dep->depset) {
		    recordDepsetState(dep->depset, res_state);
		    cycle_node = tryNextDepFromDepset(
			dep->depset, cycle_node, res_state);
		    if (!cycle_node) {
			PPREFIX("    [restarting deps]\n");
			i = -1;
			continue;
		    }
		}
		PPREFIX("<--") PSEXP(cycle_node);
		return cycle_node;
	    }
	}
    }
    PPREFIX("<--") PSEXP(NULL);
    return NULL;
}

static Vector *
nodesInCycle(DagNode *node)
{
    Vector *vec = vectorNew(10);
    Dependency *next;
    vectorPush(vec, (Object *) node);
    do {
	next = (Dependency *) ELEM(node->deps, node->cur_dep);
	node = next->dep;
    } while (setPush(vec, (Object *) node));
    return vec;
}

static void
reportCycle(DagNode *node)
{
    Vector *vec = nodesInCycle(node);
    char *cycle_desc = objectSexp((Object *) vec);
    char *errmsg = newstr("Unresolved dependency cycle: %s", cycle_desc);
    skfree(cycle_desc);
    objectFree((Object *) vec, FALSE);
    RAISE(TSORT_ERROR, errmsg);
}

static DagNode *
resolveNode(DagNode *node, volatile ResolverState *res_state DEPTH)
{
    DagNode *cycle_node;
    int retries = 0;

    assertDagNode(node);
    PPREFIX("resolveNode():") PSEXP(node);
    switch (node->status) {
    case VISITED:
	return NULL;
    case VISITING:
	return node;
    case UNVISITED:
	node->status = VISITING;
	recordNodeVisit(node, res_state);
	if (node->build_type == REBUILD_NODE) {
	    node->build_type = BUILD_NODE;
	}
	cycle_node = resolveDeps(node, res_state NEXDEPTH);

	while (node == cycle_node) {
	    retries++;
	    if (retries > 4) {
		reportCycle(node);
	    }
	    cycle_node = resolveDeps(node, res_state NEXDEPTH);
	}

	if (cycle_node) {
	    unwindToNode(node, res_state);
	}
	else {
	    node->status = VISITED;
	}
	PPREFIX("<--") PSEXP(cycle_node);
	return cycle_node;
    default: 
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
	return NULL;
    }
}


/* Attempt to resolve our dependency graph into an acyclic graph by
 * eliminating optional dependencies that cause cycles; adding fallbacks
 * where no optional dependency can be found that does not lead to a
 * cycle; and adding cycle breakers where necessary and possible.
 */
static void
resolveDependencies(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    DagNode *cycle_node;
    int retries = 0;

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	if (cycle_node = resolveNode(node, res_state DEPTH0)) {
	    retries++;
	    if (retries > 4) {
		reportCycle(node);
	    }

	    /* Start the whole thing again, from the beginning.  Note:
	     * it might be possible to restart from cycle_node but it is
	     * not obvious that this is safe since other nodes would
	     * likely have been in the VISITING_STATE when it was first
	     * resolved.  Possibly we could record the root resolution
	     * node (ie the node examined directly from this function)
	     * for each node when it was resolved, but that looks like
	     * a premature optimisation. */
	    i = -1;  /* Restart the loop from the beginning. */
	}
    }
}

/* This converts the node->deps vector of dependencies into a vector of
 * DagNodes.
 */
static void
convertDependencies(Vector *nodes)
{
    DagNode *node;
    int i;
    Dependency *dep;
    int j;
    Vector *newvec;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (node->deps) {
	    newvec = vectorNew(node->deps->elems);
	    EACH(node->deps, j) {
		if (dep = (Dependency *) ELEM(node->deps, j)) {
		    assert(dep->type == OBJ_DEPENDENCY,
			   "Invalid object type");

		    if (dep->dep && depIsActive(dep)) {
			setPush(newvec, (Object *) dep->dep);
		    }
		    objectFree((Object *) dep, TRUE);
		}
	    }
	    objectFree((Object *) node->deps, FALSE);
	    node->deps = newvec;
	}
    }
}

static void
showDepsets(Vector *depsets)
{
    int i;
    DependencySet *depset;
    EACH(depsets, i) {
	depset = (DependencySet *) ELEM(depsets, i);
	dbgSexp(depset);
    }
}

Vector *
dagFromDoc(Document *doc)
{
    ResolverState volatile resolver_state;
    initResolverState(&resolver_state);
    resolver_state.doc = doc;
    resolver_state.all_nodes = dagNodesFromDoc(doc);

    BEGIN {
	makeQnHashes(&resolver_state);
	makeMirrors(&resolver_state);
	recordDependencies(&resolver_state);

	//showVectorDeps(resolver_state.all_nodes);
	//fprintf(stderr, "-----------------------\n");

	identifyDependencies(&resolver_state);

	cleanupDependencies(&resolver_state);

	//showVectorDeps(resolver_state.all_nodes);
	//showDepsets(resolver_state.dependency_sets);
	//fprintf(stderr, "-----------------------\n");

	resolveDependencies(&resolver_state);

	//showVectorDeps(resolver_state.all_nodes);
	//fprintf(stderr, "-----------------------\n");
	convertDependencies(resolver_state.all_nodes);
	cleanUpResolverState(&resolver_state);
    }
    EXCEPTION(ex) {
	cleanUpResolverState(&resolver_state);
	objectFree((Object *) resolver_state.all_nodes, TRUE);
	RAISE();
    }
    END;

    return resolver_state.all_nodes;
}

