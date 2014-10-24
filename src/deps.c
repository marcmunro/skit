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
} ResolverState;

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


void
showDeps(DagNode *node)
{
    if (node) {
        printSexp(stderr, "NODE: ", (Object *) node);
	if (node->deps) {
	    printSexp(stderr, "-->", (Object *) node->deps);
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
    if (hash) {
	hashEach(hash, &hashDropContents, NULL);
	objectFree((Object *) hash, FALSE);
    }
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

/* We create mirror nodes for all nodes in our DAG.  Many of them will
 * be of type  DEACTIVATED_NODE.  Doing this makes things easier later
 * when we have to promote nodes to REBUILD_NODE, etc.
 */
static void
makeMirrorNode(DagNode *node, volatile ResolverState *res_state)
{
    DagNode *mirror;
    String *pqn;

    assertDagNode(node);
    mirror = dagNodeNew(node->dbobject, 
			mirroredBuildType(node->build_type));
    mirror->mirror_node = node;
    node->mirror_node = mirror;
    vectorPush(res_state->all_nodes, (Object *) mirror);
    addToHash(res_state->by_fqn, node->fqn, mirror);

    if (pqn = nodeAttribute(node->dbobject, "pqn")) {
	addToHash(res_state->by_pqn, pqn, mirror);
	objectFree((Object *) pqn, TRUE);
    }
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

static boolean
isPureDropSideNode(DagNode *node)
{
    assertDagNode(node);
    return (node->build_type == DROP_NODE) ||
	(node->build_type == DIFFPREP_NODE) ||
	(node->build_type == DSFALLBACK_NODE) ||
	(node->build_type == DSENDFALLBACK_NODE);
}

static boolean
isPureBuildSideNode(DagNode *node)
{
    assertDagNode(node);
    return (node->build_type == BUILD_NODE) ||
	(node->build_type == REBUILD_NODE) ||
	(node->build_type == DIFF_NODE) ||
	(node->build_type == EXISTS_NODE) ||
	(node->build_type == FALLBACK_NODE) ||
	(node->build_type == ENDFALLBACK_NODE);
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

static void
recordDependencyInVector(DagNode *node, xmlNode *depnode, Vector *vec,
                         volatile ResolverState *res_state)
{
    String *str;
    boolean fully_qualified;
    Dependency *dep;
    boolean is_build_node = isBuildSideNode(node);

    assertDagNode(node);
    assertVector(vec);


    if ((isForwards(depnode) && is_build_node) ||
	(isBackwards(depnode) && !is_build_node))
    {
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

        dep = dependencyNew(str, fully_qualified, is_build_node);
	dep->from = node;
	vectorPush(vec, (Object *) dep);
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
    boolean is_build_node = isBuildSideNode(node);

    assertDagNode(node);
    assertVector(vec);
    BEGIN {
	while (this = nextDependency(depnode->children, this)) {
	    recordDepNodeInVector(node, this, deps_in_depset, res_state);
	}
	    
	if ((isForwards(depnode) && is_build_node) ||
	    (isBackwards(depnode) && !is_build_node)) 
	{
	    depset = makeDepset(node, depnode, deps_in_depset, is_build_node);
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
			myVectorPush(&node->deps, obj);
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
    res_state->dependency_sets = vectorNew(res_state->all_nodes->elems);

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	recordNodeDeps(node, res_state);
    }
}


static Object *
findNodesByQn(String *qn, boolean qn_is_full, volatile ResolverState *res_state)
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
	    if (this->is_fallback) {
		if ((this->build_type == FALLBACK_NODE) ||
		    (this->build_type == DSFALLBACK_NODE))
		{
		    result = append(result, (Object *) this);
		}
	    }
	    else {
		result = append(result, (Object *) this);
	    }
	}
    }
    return result;
}

/* Attempt to fully identify a dependency (set the dep attibute),
 */
static boolean
identifyDep(Dependency *dep, volatile ResolverState *res_state)
{
    Object *found = NULL;

    assertDependency(dep);

    if (!dep->dep) {
	found = findNodesByQn(dep->qn, dep->qn_is_full, res_state);
	if (found) {
	    if (found->type == OBJ_VECTOR) {
		found = getAppropriateDepsFromVector((Vector *) found, dep);
	    }
	}
	if (found) {
	    assertDagNode(found);
	    dep->dep = (DagNode *) found;
	}
	else {
	    return FALSE;
	}
    }
    return TRUE;
}

static void
identifyNodeDeps(DagNode *node, volatile ResolverState *res_state)
{
    Dependency *dep;
    int start_deps = node->deps->elems;
    int i;

    assertDagNode(node);
    for (i = 0; i < start_deps; i++) {
	if (dep = (Dependency *) ELEM(node->deps, i)) {
	    assertDependency(dep);

	    if (!identifyDep(dep, res_state)) {
		RAISE(XML_PROCESSING_ERROR,
		      newstr("identifyNodeDeps: "
			     "cannot find dependency for %s in %s,",
			     dep->qn->value, node->fqn->value));
	    }
	}
    }
}

static int
depsetcmp(const void *p1, const void *p2)
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
	  sizeof(Vector *), depsetcmp);
}

static boolean
addFoundDeps(
    DependencySet *depset, 
    Dependency *dep, 
    Object *deps, 
    boolean make_copy,
    volatile ResolverState *res_state)
{
    //Vector *vec;
    //int i;

    if (deps->type == OBJ_VECTOR) {
	dbgSexp(deps);
	dbgSexp(depset);
	RAISE(NOT_IMPLEMENTED_ERROR, newstr("UNTESTED"));
#ifdef QQ
	if (!depset) {
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("untested - vectors without depsets "
			 "in addFoundDeps."));
	    depset = dependencySetNew(node);
	    vectorPush(res_state->dependency_sets, (Object *) depset);
	}
	vec = (Vector *) deps;
	EACH(vec, i) {
	    make_copy = addFoundDeps(depset, dep, ELEM(vec, i),
				     make_copy, res_state);
	}
	objectFree((Object *) vec, FALSE);
#endif
	return TRUE;
    }
    else if (deps->type == OBJ_DAGNODE) {
	dep->dep = (DagNode *) deps;
	if (isBuildSideNode(depset->definition_node) != 
	    isBuildSideNode(dep->dep))
	{
	    dbgSexp(dep);
	    dbgSexp(dep->dep);
	    dbgSexp(dep->dep->mirror_node);
	    dbgSexp(depset->definition_node);
	    RAISE(TSORT_ERROR, 
		  newstr("Bad dependency from build side to drop, "
			 "or vice versa."));
	}
	if (dep->dep->is_fallback && !dep->is_forwards) {
	    /* This is a backwards dep on a dsfallback node, which should
	     * not happen.  Instead we will make it a backwards dep
	     * on the dsendfallback node.  When we later find this in
	     * redirectDependencies(), we will add the correct
	     * dependency to the dsfallback node. */

	    dep->dep = dep->dep->mirror_node;
	}
	myVectorPush(&depset->definition_node->deps, (Object *) dep);
	return TRUE;
    }
    else {
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected objet type (%d) in addFoundDeps()", 
		     deps->type));
    }
    return make_copy;
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
		found = findNodesByQn(dep->qn, dep->qn_is_full, res_state);
		if (found && (found->type == OBJ_VECTOR)) {
		    found = getAppropriateDepsFromVector(
			(Vector *) found, dep);
		}
		if (found) {
		    added = addFoundDeps(depset, dep, found, FALSE, res_state);
		}
		if (added) {
		    /* Replace the dependency in the depset with a
		     * reference in order to prevent it from being freed
		     * multiple times. */
		    ELEM(depset->deps, i) = (Object *) 
			objRefNew((Object *) dep);
		}
	    }
	    if (depset->deactivated) {
		depset->chosen_dep = i;
		break;
	    }
	}
    }
}

static int
depsetSelectableOptions(DependencySet *depset)
{
    int i;
    Dependency *dep;
    int result = 0;

    assertDependencySet(depset);
    EACH(depset->deps, i) {
	dep = (Dependency *) dereference(ELEM(depset->deps, i));
	if (dep && dep->dep) {
	    result++;
	}
    }
    return result;
}


static Dependency *
getDep(DependencySet *depset, int i)
{
    if (i < depset->deps->elems) {
	return (Dependency *) dereference(ELEM(depset->deps, i));
    }
    return NULL;
}

static Dependency *
chosenDep(DependencySet *depset)
{
    return getDep(depset, depset->chosen_dep);
}

static void
depsetNextDep(DependencySet *depset)
{
    Dependency *dep;
    while (TRUE) {
	depset->chosen_dep++;
	if (depset->chosen_dep >= depset->deps->elems) {
	    return;
	}
	dep = chosenDep(depset);
	if (dep && dep->dep) {
	    return;
	}
    }
}

static void
depsetInit(DependencySet *depset)
{
    depset->chosen_dep = -1;
    depsetNextDep(depset);
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
	      newstr("Failed to find parent element (%s)for fallback %s",
		     depset->fallback_parent->value,
		     depset->fallback_expr->value));
    }

    fqn = nodeAttribute(parent_node, "fqn");
    found = (Vector *) findNodesByQn(fqn, TRUE, res_state);
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

/* 
 * Create a simple fallback node, which we wil place into its own
 * document.  This will then be processed by the fallbacks.xsl script to
 * create a fully formed dbobject which will then be added to our 
 * document.
 */
static xmlNode *
makeXMLFallbackNode(
    String *fqn, 
    DagNode *parent, 
    volatile ResolverState *res_state)
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

    // TODO: determine whether this is the right place to set the
    // parent, or should it be the responsibility of the xsl transform
    // above? 
    xmlNewProp(dbobject, BAD_CAST "parent", BAD_CAST parent->fqn->value);
    objectFree((Object *) doc, TRUE);
    if (!dbobject) {
	RAISE(TSORT_ERROR, newstr("Failed to create fallback for %s.",
				  fqn->value));
    }

    /* Add the new node into our source document, so that its memory
     * will not be leaked. */
    xmlAddChild(parent->dbobject, dbobject);
    return dbobject;
}

static xmlNode *
makeFallbackDbobjectNode(
    DependencySet *depset, 
    DagNode *parent,
    volatile ResolverState *res_state)
{
    xmlNode *dbobj;
    assertDependencySet(depset);
    dbobj = makeXMLFallbackNode(depset->fallback_expr, parent, res_state);

    if (!dbobj) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Failed to create fallback: \"%s\"", 
		     depset->fallback_expr->value));
    }
    return dbobj;
}

static DagNode *
makeFallbackDagNode(
    xmlNode *dbobj, 
    DagNode *parent,
    volatile ResolverState *res_state)
{
    DagNode *fallback;
    char *new;

    assertDagNode(parent);
    fallback = dagNodeNew(dbobj, FALLBACK_NODE);
    fallback->is_fallback = TRUE;
    fallback->parent = parent;

    vectorPush(res_state->all_nodes, (Object *) fallback);
    addToHash(res_state->by_fqn, fallback->fqn, fallback);

    /* Now add a suffix to the actual fqn - this allows us to tell
     * instances apart. */
    new = newstr("%s.%d", fallback->fqn->value, res_state->fallback_no);
    skfree(fallback->fqn->value);
    fallback->fqn->value = new;

    return fallback;
}


/* This highlights a possible issue with fallbacks.  What does it mean
 * if we depend on the build side of an object being dropped?  Should we
 * do an inverted dep from the dop node instead?  This requires careful
 * thought.  Do not remove this check without extensive thinking and
 * testing. 
 */
static void
checkDeactivatedNodeDeps(
    DagNode *defn_node,
    DagNode *fallback, 
    volatile ResolverState *res_state) 
{
    int i;
    Dependency *dep;

    if (fallback->deps) {
	EACH(fallback->deps, i) {
	    dep = (Dependency *) ELEM(fallback->deps, i);
	    assertDependency(dep);
	    if (dep->dep->build_type == DEACTIVATED_NODE) {
		fprintf(stderr, 
			"CONSIDER: Fallback %s depends on "
			"deactivated node %s\n\n",
			fallback->fqn->value, dep->dep->fqn->value);
	    }
	}
    }
}

static DagNode *
newFallbackPair(DependencySet *depset, volatile ResolverState *res_state)
{
    xmlNode *dbobj;
    DagNode *parent;
    DagNode *fallback;
    DagNode *endfallback;

    assertDependencySet(depset);
    parent = makeParentDagNode(depset, res_state);
    dbobj = makeFallbackDbobjectNode(depset, parent, res_state);

    res_state->fallback_no++;

    fallback = makeFallbackDagNode(dbobj, parent, res_state);
    endfallback = makeFallbackDagNode(dbobj, parent, res_state);

    /* They are not really mirror nodes as such, but this it is useful
     * to treat them as such. */ 
    fallback->mirror_node = endfallback;
    endfallback->mirror_node = fallback;
    if (isBuildSideNode(depset->definition_node)) {
	endfallback->build_type = ENDFALLBACK_NODE;
    }
    else {
	fallback->build_type = DSFALLBACK_NODE;
	endfallback->build_type = DSENDFALLBACK_NODE;
    }

    recordNodeDeps(fallback, res_state);
    identifyNodeDeps(fallback, res_state);
    recordNodeDeps(endfallback, res_state);
    identifyNodeDeps(endfallback, res_state);
    checkDeactivatedNodeDeps(depset->definition_node, fallback, res_state);

    return fallback;
}

static Dependency *
makeDep(String *fqn, DagNode *dep_node, boolean is_forwards)
{
    String *qn = stringDup(fqn);
    Dependency *dep = dependencyNew(qn, TRUE, is_forwards);
    assertDagNode(dep_node);
    dep->dep = (DagNode *) dep_node;
    return dep;
}


/* Add dependencies to fallback from depset.  The rules for these are a
 * little complex.
 *
 * For nodes on the drop side of the DAG, after we have resolved
 * optionality, and added fallbacks and cycle breakers, we will invert
 * all of the dependencies (these are marked as !is_forwards).  However,
 * for fallbacks, whether on the build or drop side the direction of the
 * final dependency must be from the dependent node to the fallback, and
 * the endfallback to the dependent node.  This means that in order for
 * the poste-resolution drop-side inversion to work correctly, the
 * dependency from the depset to the fallback must be inverted here.
 * Note that when the fallback dependency is activated, so too must be
 * the endfallback dependency.  This is achieved by special case code in
 * depIsActive().
 */
static void
addFallbackDeps(DependencySet *depset, DagNode *fallback)
{
    DagNode *depset_node = depset->definition_node;
    Dependency *fallback_dep;
    DagNode *endfallback;
    Dependency *endfallback_dep;

    assertDependencySet(depset);
    assertDagNode(fallback);
    endfallback = fallback->mirror_node;

    if (isBuildSideNode(depset_node)) {
	fallback_dep = makeDep(fallback->fqn, fallback, TRUE);
	fallback_dep->from = depset_node;
	endfallback_dep = makeDep(depset_node->fqn, depset_node, TRUE);
	endfallback_dep->from = endfallback;
	myVectorPush(&depset_node->deps, (Object *) fallback_dep);
	myVectorPush(&endfallback->deps, (Object *) endfallback_dep);
    }
    else {
	fallback_dep = makeDep(depset_node->fqn, depset_node, FALSE);
	fallback_dep->from = fallback;
	endfallback_dep = makeDep(fallback->fqn, endfallback, FALSE);
	endfallback_dep->from = depset_node;
	myVectorPush(&fallback->deps, (Object *) fallback_dep);
	myVectorPush(&depset_node->deps, (Object *) endfallback_dep);
    }
    fallback_dep->endfallback = endfallback_dep;
    fallback_dep->depset = depset;
    endfallback_dep->depset = depset;
    myVectorPush(&depset->deps, (Object *) objRefNew((Object *) fallback_dep));
}

static boolean
matchingFallback(Dependency *dep, DagNode *fallback)
{
    int len = strlen(dep->qn->value);
    return strncmp(dep->qn->value, fallback->fqn->value, len) == 0;
}


static boolean
depExistsInDepset(DependencySet *depset, DagNode *depnode)
{
    int i;
    Dependency *dep;
    EACH(depset->deps, i) {
	dep = getDep(depset, i);
	if (dep->dep == depnode) {
	    return TRUE;
	}
    }
    return FALSE;
}

/* If the new fallback matches one of our dependencies, we add it to the
 * dependency set. 
 */
static void
maybeAddFallbackToDepset(
    DagNode *fallback, 
    DependencySet *depset, 
    volatile ResolverState *res_state)
{
    int i;
    Dependency *dep;
    int limit = depset->deps->elems;

    for (i = 0; i < limit; i++) {
	if (dep = getDep(depset, i)) {
	    if (matchingFallback(dep, fallback)) {
		if (!depExistsInDepset(depset, fallback)) {
		    dbgSexp(depset);
		    dbgSexp(fallback);
		    RAISE(NOT_IMPLEMENTED_ERROR, newstr("test this!"));
		    //addFallbackDeps(depset, fallback);
		}
	    }
	}
    }
}

static DagNode *
findMatchingFallback(DagNode *fallback, volatile ResolverState *res_state)
{
    char *dotpos;
    String *basename;
    Vector *found;
    int i;
    DagNode *node;
    basename = stringNew(fallback->fqn->value);
    if (dotpos = strrchr(basename->value, '.')) {
	*dotpos = '\0';
	found = (Vector *) findNodesByQn(basename, TRUE, res_state);
	objectFree((Object *) basename, TRUE);
	if (found) {
	    assertVector(found);
	    EACH(found, i) {
		node = (DagNode *) ELEM(found, i);
		assertDagNode(node);
		if (node->build_type == FALLBACK_NODE) {
		    if (node != fallback) {
			return node;
		    }
		}
	    }
	}
    }
    return NULL;
}

static void
setInterFallbackDependency(DagNode *build_fb, DagNode *drop_fb)
{
    DagNode *drop_efb = drop_fb->mirror_node;
    String *fqn = stringNew(drop_efb->fqn->value);
    Dependency *dep = dependencyNew(fqn, TRUE, TRUE);
    dep->dep = drop_efb;
    myVectorPush(&build_fb->deps, (Object *) dep);
}


static void
activateFallbackForDepset(
    DependencySet *depset, 
    volatile ResolverState *res_state)
{
    DagNode *fallback;
    DagNode *other;
    int i;
    DependencySet *prevset;

    fallback = newFallbackPair(depset, res_state);
    addFallbackDeps(depset, fallback);
    depset->has_fallback = TRUE;

    EACH(res_state->dependency_sets, i) {
	prevset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	if (prevset == depset) {
	    break;
	}
	if (isBuildSideNode(depset->definition_node) ==
	    isBuildSideNode(prevset->definition_node))
	{
	    maybeAddFallbackToDepset(fallback, prevset, res_state);
	}
    }

    if (other = findMatchingFallback(fallback, res_state)) {
	if (isBuildSideNode(depset->definition_node)) {
	    setInterFallbackDependency(fallback, other);
	}
	else {
	    setInterFallbackDependency(other, fallback);
	}
    }
}

static void
initChosenDep(DependencySet *depset)
{
    if (!depset->deactivated) {
	depset->chosen_dep = -1;
	depsetNextDep(depset);

	if (!chosenDep(depset)) {
	    if (depset->definition_node->build_type != DEACTIVATED_NODE) {
		//dbgSexp(depset);
		RAISE(TSORT_ERROR, 
		      newstr("No selectable option in dependency set."));
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
    if ((depsetSelectableOptions(depset) == 0) && depset->fallback_expr) {
	activateFallbackForDepset(depset, res_state);
    }
    initChosenDep(depset);
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
	if (node->deps) {
	    identifyNodeDeps(node, res_state);
	}
    }

    identifyDepsForDepsets(res_state);
}

static boolean
conditionallyPromoteToRebuild(DagNode *node)
{
    Dependency *dep;
    int i;
    if (node->build_type == REBUILD_NODE) {
	return TRUE;
    }
    if (node->status == UNVISITED) {
	node->status = VISITED;
	if ((node->build_type == DIFF_NODE) ||
	    (node->build_type == EXISTS_NODE)) 
	{
	    EACH(node->deps, i) {
		dep = (Dependency *) ELEM(node->deps, i);
		assertDependency(dep);
		if (dep->dep && conditionallyPromoteToRebuild(dep->dep)) {
		    node->status = REBUILD_NODE;
		    node->mirror_node->status = DROP_NODE;
		    RAISE(NOT_IMPLEMENTED_ERROR, newstr("test this2!"));
		    return TRUE;
		}
	    }
	}
    }
    return FALSE;
}


/* Any node that depends on a node with a buld_type of REBUILD, must
 * itself be promoted to a rebuild. */
static void
promoteRebuilds(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
        node = (DagNode *) ELEM(nodes, i);
	assertDagNode(node);
	(void) conditionallyPromoteToRebuild(node);
    }
}

/* Reset the node->status back to UNVISITED. */
static void
resetNodeStates(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
        node = (DagNode *) ELEM(nodes, i);
	assertDagNode(node);
	node->status = UNVISITED;
    }
}

/* Set the build side of a rebuild node pair, into a BUILD_NODE.
 */
static void
setRebuildsToBuilds(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
        node = (DagNode *) ELEM(nodes, i);
	assertDagNode(node);
	if (node->build_type == REBUILD_NODE) {
	    node->build_type = BUILD_NODE;
	}
    }
}

static boolean
nodeIsActive(DagNode *node)
{
    assertDagNode(node);
    return (node->build_type != EXISTS_NODE) &&
	(node->build_type != DEACTIVATED_NODE);
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
	objectFree((Object *) found, TRUE);
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
    }
    objectFree((Object *) dep, TRUE);
}

/* Eliminate dependencies where one end or the other is in inactive node
 * (such as an EXISTS_NODE).
 */
static void
cleanupDependencies(Vector *nodes)
{
    int i;
    DagNode *node;
    int j;
    Dependency *dep;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
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

static Dependency *
curDep(DagNode *node)
{
    return (Dependency *) ELEM(node->deps, node->cur_dep);
}

static Dependency *
rootDepInCycle(Dependency *dep)
{
    Dependency *next = dep;
    int this_depth = dep->dep->resolver_depth;
    int prev_depth;
    do {
	prev_depth = this_depth;
	next = curDep(next->dep);
	this_depth = next->dep->resolver_depth;
    } while (this_depth > prev_depth);
    return next;
}

static Vector *
depsInCycle(Dependency *dep)
{
    Vector *vec = vectorNew(10);
    Dependency *next = rootDepInCycle(dep);
    vectorPush(vec, (Object *) next);
    do {
	next = curDep(next->dep);
	assertDependency(next);
	if (next->depset) {
	    assertDependencySet(next->depset);
	}
    } while (setPush(vec, (Object *) next));
    return vec;
}

static char *
cycleDescription(Dependency *start)
{
    Vector *cycle = depsInCycle(start);
    char *result = NULL;
    char *tmp;
    int i;
    Dependency *dep;
    EACH(cycle, i) {
	dep = (Dependency *) ELEM(cycle, i);
	assertDependency(dep);
	assertDagNode(dep->dep);
	if (result) {
	    tmp = result;
	    result = newstr("%s -> (%s) %s", tmp, 
			    nameForBuildType(dep->dep->build_type),
			    dep->dep->fqn->value);
	    skfree(tmp);
	}
	else {
	    result = newstr("(%s) %s", nameForBuildType(dep->dep->build_type),
			    dep->dep->fqn->value);
	}
    }
    objectFree((Object *) cycle, FALSE);
    return result;
}

static boolean
depIsOptional(Dependency *dep)
{
    int options;
    if (dep->depset) {
	options = depsetSelectableOptions(dep->depset);
	if (options >= 1) {
	    if (options == 1) {
		return (dep->depset->fallback_expr != NULL);
	    }
	    return TRUE;
	}
    }
    return FALSE;
}

static DagNode *
makeBreakerNode(DagNode *from_node, String *breaker_type)
{
    xmlNode *dbobject = from_node->dbobject;
    xmlChar *old_fqn = xmlGetProp(dbobject, (const xmlChar *) "fqn");
    char *fqn_suffix = strstr((const char *) old_fqn, (char *) ".");
    char *new_fqn = newstr("%s%s", breaker_type->value, fqn_suffix);
    xmlNode *breaker_dbobject = xmlCopyNode(dbobject, 1);
    DagNode *breaker;
    xmlSetProp(breaker_dbobject, (const xmlChar *) "type", 
	       (const xmlChar *) breaker_type->value);
    xmlUnsetProp(breaker_dbobject, (const xmlChar *) "cycle_breaker");
    xmlSetProp(breaker_dbobject, (const xmlChar *) "fqn", 
	       (const xmlChar *) new_fqn);
    xmlFree(old_fqn);
    skfree(new_fqn);

    breaker = dagNodeNew(breaker_dbobject, from_node->build_type);
    breaker->parent = from_node->parent;

    /* Add the new xml node into our source document, so that its memory
     * will not be leaked. */
    xmlAddChild(dbobject->parent, breaker_dbobject);

    return breaker;
}

static DagNode *
ensureUniqueBreaker(DagNode *breaker, volatile ResolverState *res_state)
{
   Object *exists;
   if (exists = findNodesByQn(breaker->fqn, TRUE, res_state)) {
       if (exists->type == OBJ_DAGNODE) {
	   if (((DagNode *) exists)->build_type == breaker->build_type) {
	       /* A breaker has already been created for this node, it
		* obviously didn't solve everything, so let's not create
		* another.  */
	       objectFree((Object *) breaker, TRUE);
	       return (DagNode *) exists;
	   }
       }
       else {
	   RAISE(NOT_IMPLEMENTED_ERROR, 
		 newstr("Untested code path - 2"));
       }
   }
   addToHash(res_state->by_fqn, breaker->fqn, breaker);
   vectorPush(res_state->all_nodes, (Object *) breaker);
   return breaker;
}

static DagNode *
getBreakerFor(DagNode *node, volatile ResolverState *res_state)
{
    String *breaker_type = nodeAttribute(node->dbobject, "cycle_breaker");
    DagNode *breaker = NULL;
 
    if (breaker_type) {
	breaker = makeBreakerNode(node, breaker_type);
	objectFree((Object *) breaker_type, TRUE);
	
	breaker = ensureUniqueBreaker(breaker, res_state);
    }
    return breaker;
}

static Dependency *
dupDependency(Dependency *dep) {
    String *qn = stringDup(dep->qn);
    Dependency *result = dependencyNew(qn, dep->qn_is_full, dep->is_forwards);

    assertDependency(dep);
    result->dep = dep->dep;
    return result;
}


static void 
copyBreakerDeps(
    DagNode *node, 
    DagNode *breaker, 
    volatile ResolverState *res_state)
{
    int i;
    Dependency *dep;
    Dependency *new_dep;
    if (!breaker->deps) {
	EACH(node->deps, i) {
	    if (dep = (Dependency *) ELEM(node->deps, i)) {
		assertDependency(dep);
		new_dep = dupDependency(dep);
		new_dep->from = breaker;
		myVectorPush(&(breaker->deps), (Object *) new_dep);
	    }
	}
    }
}

/* Eliminate the dependency copied into breaker from node, that
 * completes the cycle.
 */
static void
removeCycleDep(DagNode *node, DagNode *breaker)
{
    Dependency *dep;
    DagNode *next;
    int i;
    boolean removed = FALSE;

    dep = curDep(node);
    assertDependency(dep);
    next = dep->dep;
    assertDagNode(next);
    EACH(breaker->deps, i) {
	dep = (Dependency *) ELEM(breaker->deps, i);
	if (dep) {
	    assertDependency(dep);
	    if (dep->dep == next) {
		objectFree((Object *) dep, TRUE);
		ELEM(breaker->deps, i) = NULL;
		removed = TRUE;
	    }
	}
    }
    if (!removed) {
	RAISE(TSORT_ERROR, 
	      newstr("Failed to remove dependency from cycle_breaker."));
    }
}

static Dependency *
getRefererFromCycle(DagNode *node)
{
    DagNode *this = node;
    Dependency *dep;
    while (TRUE) {
	dep = curDep(this);
	assertDependency(dep);
	if (dep->dep == node) {
	    return dep;
	}
	this = dep->dep;
	assertDagNode(this);
    }
}


static void
switchCycleReferer(DagNode *node, DagNode *breaker)
{
    Dependency *referer;

    referer = getRefererFromCycle(node);
    assertDagNode(referer->dep);
    referer->dep = breaker;
}


static DagNode *
findMatchingBreaker(DagNode *node, DagNode *breaker)
{
    DagNode *cycle_node = node;
    DagNode *mirror;
    int i;
    Dependency *dep;
    String *prefix = stringNew(breaker->fqn->value);
    char *dotpos = strchr(prefix->value, '.');
    int len;
    assert(dotpos, "Could not find prefix for %s", prefix->value);
    *(dotpos + 1) = '\0';
    len = strlen(prefix->value);

    while (TRUE) {
	if (mirror = cycle_node->mirror_node) {
	    assertDagNode(mirror);
	    EACH(mirror->deps, i) {
		dep = (Dependency *) ELEM(mirror->deps, i);
		assertDependency(dep);
		assertDagNode(dep->dep);
		if (strncmp(prefix->value, dep->dep->fqn->value, len) == 0) {
		    /* Yay, we found a match! */
		    objectFree((Object *) prefix, TRUE);
		    return dep->dep;
		}
	    }
	}

	dep = curDep(cycle_node);
	assertDependency(dep);
	cycle_node = dep->dep;
	assertDagNode(cycle_node);
	if (cycle_node == node) {
	    /* We have been to each node in the cycle, so we are done. */
	    break;
	}
    }
    objectFree((Object *) prefix, TRUE);
    return NULL;
}

static boolean
tryCycleBreaker(DagNode *node, volatile ResolverState *res_state)
{
    DagNode *breaker = getBreakerFor(node, res_state);
    DagNode *other;
    Dependency *dep;

    if (breaker) {
	if (other = findMatchingBreaker(node, breaker)) {
	    if (isBuildSideNode(other)) {
		dep = makeDep(breaker->fqn, breaker, TRUE);
		myVectorPush(&other->deps, (Object *) dep);
	    }
	    else {
		dep = makeDep(other->fqn, other, TRUE);
		myVectorPush(&breaker->deps, (Object *) dep);
	    }
	}
	copyBreakerDeps(node, breaker, res_state);
	if (isBuildSideNode(node)) {
	    removeCycleDep(node, breaker);
	    switchCycleReferer(node, breaker);
	}
	else {
	    removeCycleDep(node, node);
	}
	return FALSE;
    }
    return TRUE;
}

//#define DEBUG_RESOLVER 1
#ifdef DEBUG_RESOLVER
#define DEPTH , int depth
#define DEPTHVAR int depth = 1
#define DEPTH0 , 1
#define NEXDEPTH , depth+1
#define THISDEPTH , depth
#define PPREFIX(x) fprintf(stderr, "%*s%s", depth, " ", x);
#define PSEXP(x) dbgSexp(x);
#define PPSEXP(x, y) printSexp(stderr, x, (Object *) y);
#else
#define DEPTH
#define DEPTHVAR
#define DEPTH0
#define NEXDEPTH
#define THISDEPTH
#define PPREFIX
#define PSEXP(x)
#define PPSEXP(x, y)
#endif


static boolean
resolveNode(DagNode *node, volatile ResolverState *res_state, 
	    int depth, boolean apply_fallbacks);

static boolean
resolveThisDep(
    DagNode *node, 
    Dependency *dep, 
    volatile ResolverState *res_state, 
    int depth,
    boolean apply_fallbacks)
{
    boolean cycle;

    PPREFIX("resolveThisDep() ") PSEXP(dep);
    if (cycle = resolveNode(dep->dep, res_state, depth, apply_fallbacks)) {
	if (depIsOptional(dep)) {
	    /* This is an optional dep, so we have a chance of resolving
	     * this cycle here. */

	    PPREFIX("\n");
	    PPREFIX("--- Trying to resolve cycle at ") PSEXP(dep);
	    PPREFIX("---                       for ") PSEXP(node);
	    depsetNextDep(dep->depset);
	    if (chosenDep(dep->depset)) {
		/* We can try the next option. */
		return FALSE;
	    }
	    if (apply_fallbacks && dep->depset->fallback_expr) {
		/* Maybe we can apply a fallback. */
		if (!dep->depset->has_fallback) {
		    RAISE(NOT_IMPLEMENTED_ERROR, newstr("ADD FALLBACK"));
		}
	    }
	    depsetInit(dep->depset);
	}
    }

    if (cycle) {
	cycle = tryCycleBreaker(node, res_state);
    }
    return cycle;
}

static boolean 
resolveDeps(
    DagNode *node, 
    volatile ResolverState *res_state, 
    int depth,
    boolean apply_fallbacks)
{
    Dependency *dep;

    assertDagNode(node);
    
    if (node->deps) {
	while (node->cur_dep < node->deps->elems) {
	    if (dep = curDep(node)) {
		assertDependency(dep);
		if (depIsActive(dep)) {
		    if (resolveThisDep(node, dep, res_state, 
				       depth, apply_fallbacks)) {
			return TRUE;
		    }
		}
	    }
	    node->cur_dep++;
	}
    }
    return FALSE;
}

static boolean
resolveNode(
    DagNode *node, 
    volatile ResolverState *res_state, 
    int depth,
    boolean apply_fallbacks)
{
    assertDagNode(node);
    switch (node->status) {
    case VISITED:
	return FALSE;
    case VISITING:
	return TRUE;
    case UNVISITED:
	node->status = VISITING;
	node->resolver_depth = depth;
	node->cur_dep = 0;
	    
	if (resolveDeps(node, res_state, depth + 1, apply_fallbacks)) {
	    /* Make the cycle the caller's problem. */
	    node->status = UNVISITED;
	    PPREFIX("UNWINDING FROM: ") PSEXP(node);
	    return TRUE;
	}
	PPREFIX("VISITED: ") PSEXP(node);
	node->status = VISITED;
	return FALSE;
    default: 
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
	return FALSE;
    }
}
/* This traverses the current dependency graph using a tsort like
 * mechanism to identify cycles.  When a cycle is found we try each
 * optional dependency in turn to try to eliminate all cycles.  If we
 * are unable to eliminate the cycles this way, we will also try adding
 * fallbacks, and cycle-breaker nodes.  Once this function has
 * completed, we will effectively have 2 DAGS, one for the build side of
 * the graph and one for the drop side.  The drop side can then be
 * inverted and the DAGS combined to create a single final DAG which
 * tsort can then traverse without concern.
 */
static void
resolveDependencySets(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    char *errmsg;
    char *tmp;
    Dependency *dep;
    DependencySet *depset;
    DEPTHVAR;

    EACH(res_state->dependency_sets, i) {
	depset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	assertDependencySet(depset);
	depsetInit(depset);
    }

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	PPREFIX("\nresolveDependencySets():") PSEXP(node);
	if (node->build_type != DEACTIVATED_NODE) {
	    if (resolveNode(node, res_state, 1, FALSE)) {
		dep = curDep(node);
		tmp = cycleDescription(dep);
		errmsg = newstr("Unresolved dependency cycle: %s", tmp);
		skfree(tmp);
		RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg);
	    }
	}
    }
}

static void
redirectNodeDeps(DagNode *node)
{
    int i;
    Dependency *dep;
    Dependency *fb_dep;
    String *qn;
    DagNode *new_from;

    EACH(node->deps, i) {
	if (dep = (Dependency *) ELEM(node->deps, i)) {
	    assertDependency(dep);
	    if (!dep->is_forwards) {
		if (!node->tmp_deps) {
		    node->tmp_deps = vectorNew(10);
		}

		if (dep->dep->build_type == DSENDFALLBACK_NODE) {
		    /* We must ensure that there is a matching
		     * dependency on the fallback node.  See
		     * addFoundDeps() for more notes on this. */ 

		    qn = stringNew(dep->dep->fqn->value);
		    fb_dep = dependencyNew(qn, TRUE, TRUE);
		    fb_dep->dep = dep->dep->mirror_node;
		    fb_dep->from = node;
		    myVectorPush(&(node->tmp_deps), (Object *) fb_dep);    
		}

		new_from = dep->dep;
		dep->dep = dep->from;
		dep->from = new_from;
		myVectorPush(&(new_from->tmp_deps), (Object *) dep);

	    }
	}
    }
}

static void
redirectDependencies(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	assertDagNode(node);
	if (node->deps) {
	    redirectNodeDeps(node);
	}
    }    

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	assertDagNode(node);
	if (node->tmp_deps) {
	    objectFree((Object *) node->deps, FALSE);
	    node->deps = node->tmp_deps;
	    node->tmp_deps = NULL;
	}
    }
}

/* Re-type any drop side fallback nodes.
 * TODO: remove redundant fallback nodes - see TODO file
 */
static void
finaliseFallbacks(Vector *nodes)
{
    DagNode *node;
    int i;
    EACH(nodes, i) {
        node = (DagNode *) ELEM(nodes, i);
	if (node->build_type == DSFALLBACK_NODE) {
	    node->build_type = FALLBACK_NODE;
	}
	else if (node->build_type == DSENDFALLBACK_NODE) {
	    node->build_type = ENDFALLBACK_NODE;
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
    Vector *old_deps = vectorNew(nodes->elems);

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
                }
            }
	    vectorPush(old_deps, (Object *) node->deps);
            node->deps = newvec;
        }
    }
    objectFree((Object *) old_deps, TRUE);
}

static void
addDepsForMirrors(Vector *nodes)
{
    DagNode *node;
    int i;
    EACH(nodes, i) {
        node = (DagNode *) ELEM(nodes, i);
	if (node->mirror_node) {
	    if ((node->build_type == BUILD_NODE) ||
		(node->build_type == DIFF_NODE))
	    {
		if ((node->mirror_node->build_type == DROP_NODE) ||
		    (node->mirror_node->build_type == DIFFPREP_NODE))
		{
		    myVectorPush(&(node->deps), (Object *) node->mirror_node);
		}
	    }
	}
    }
}

static void
removeDeactivatedNodes(ResolverState volatile *res_state)
{
    int i;
    Vector *new = vectorNew(res_state->all_nodes->elems);
    DagNode *node;

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	if (node->build_type == DEACTIVATED_NODE) {
	    /* We will be freeing this node, so clear the mirror
	     * reference to it. */
	    if (node->mirror_node) {
		node->mirror_node->mirror_node = NULL;
	    }
	}
	else {
	    vectorPush(new, (Object *) node);
	    ELEM(res_state->all_nodes, i) = NULL;
	}
    }
    objectFree((Object *) res_state->all_nodes, TRUE);
    res_state->all_nodes = new;
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
	identifyDependencies(&resolver_state);

	promoteRebuilds(resolver_state.all_nodes);
	resetNodeStates(resolver_state.all_nodes);
	setRebuildsToBuilds(resolver_state.all_nodes);
	cleanupDependencies(resolver_state.all_nodes);

	removeDeactivatedNodes(&resolver_state);
	resolveDependencySets(&resolver_state);
	resetNodeStates(resolver_state.all_nodes);

	redirectDependencies(resolver_state.all_nodes);

	finaliseFallbacks(resolver_state.all_nodes);
	convertDependencies(resolver_state.all_nodes);
	addDepsForMirrors(resolver_state.all_nodes);

	cleanUpResolverState(&resolver_state);
	//showVectorDeps(resolver_state.all_nodes);
	//fprintf(stderr, "------------------------\n\n");
    }
    EXCEPTION(ex) {
	cleanUpResolverState(&resolver_state);
	objectFree((Object *) resolver_state.all_nodes, TRUE);
	RAISE();
    }
    END;

    return resolver_state.all_nodes;
}
