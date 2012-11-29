/**
 * @file   deps.c
 * \code
 *     Copyright (c) 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for generating a Directed Acyclic Graph from the
 * dependencies specified in an xml stream.
 * 




 * The dependencies between objects are identfied and added to the
 * source xml stream by the adddeps.xsl xsl transform.  An intial set of
 * dependencies based solely on this information, and the parent child
 * relationships between objects is determined by nodesFromDoc().
 * 
 * In converting this basic dependency graph into a DAG we have to deal
 * with the following issues:
 * - there are optional dependencies
 * - we may have both drop and build actions for some or all nodes
 * - there may be cyclic dependencies
 *
 * This is a new implementation of deps handling.  The set of
 * dependencies defined in the document graph must eventually form a
 * DAG.  There are two distinct challenges that we face in doing this:
 * 1) optional dependencies
 * 2) cyclic dependencies
 *
 * Both of these issues are dealt with in the resolving_tsort()
 * function.  This traverses the graph, eliminating optional
 * dependencies and placing breaker nodes where needed in order to deal
 * with cyclic dependencies.  Once this function is complete, the
 * resulting dependency graph is a true DAG.
 *
 * Optional dependencies are handled as follows.  Consider a dependency
 * specification that looks something like this:
 * A -> (B or C)
 *
 * Although this can be dealt with by making the tsort code smart enough
 * to handle optional dependencies, there is no usable way to invert the
 * resulting graph.  The naive solution:
 * (B or C) -> A
 * 
 * leads to a pair of nodes on the left hand side of the dependency,
 * which is not something the standard tsort algorithm can be easily
 * modified to deal with.  Since we need to be able to invert the
 * dependency graph (in order to handle generation of drop scripts, and
 * also to manage the drop parts of code generation for diffs), we need
 * a better solution.  The adopted solution is to make nodes with
 * optional dependencies contain a subnode for each optional part, and
 * to make resolving tsort able to deal with subnodes.  Consider the
 * following graph definition: A -> (B or C) A -> D F -> A
 *
 * Using subnodes, this becomes:
 * A1 -> B
 * A2 -> C 
 * A -> D
 * F -> A
 * 
 * This has the advantage of being able to be inverted without multiple
 * nodes appearing on the left hand side of the dependencies.
 * 
 * Cyclic dependencies are handled by adding a cycle breaking node.
 * Only a limited number of database objects are capable of being dealt
 * with in this way, so only those dbobjects will specify cycle
 * breakers.  In postgres, views are able to be defined in terms of each
 * other.  The following code creates a pair of views that are mutually
 * dependent (though not very useful):
 *   create view x as select 1 as a, 2 as b;
 *   create view y as select * from x;
 *   create or replace view x as select * from y;
 *
 * From the DAG point of view, given the following cyclic dependency
 * graph:
 *   A -> B
 *   B -> A
 *   B -> C
 *   A -> D
 * We resolve the cyclic dependency by adding a cycle breaker for one of
 * the nodes in the cycle (eg B).  This gives us the following
 * dependencies:
 * B -> A
 * B -> C
 * A -> B'
 * A -> D
 * B'-> C
 * 
 * Our cycle breaking strategy:
 * - if we have a cycle breaker available we use it
 *   Done in tsort_deps
 * - if we have a bunch of optional dependencies, none of which are
 *   satisfied, and we have a fallback available, we use it.
 *   Done where??????
 * - if we have a bunch of optional dependents, none of which have been
 *   satisfied, and we have a fallback available, we use it.
 *   Done where??????
 */

#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"



void
showDeps(DagNode *node)
{
    DagNode *sub = node->subnodes;
    if (node) {
	printSexp(stderr, "NODE: ", (Object *) node);
	printSexp(stderr, "-->", (Object *) node->dependencies);
	while (sub) {
	    if (sub->dependencies || sub->fallback) {
		printSexp(stderr, "deps->", (Object *) sub->dependencies);
		if (sub->fallback) {
		    printSexp(stderr, "flbk->", (Object *) sub->fallback);
		}
	    }
	    sub = sub->subnodes;
	}
	printSexp(stderr, "<--", (Object *) node->dependents);
    }
}

static Object *
hashEachShowDeps(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    showDeps(node);
    return (Object *) node;
}

void
showHashDeps(Hash *nodes)
{
    hashEach(nodes, &hashEachShowDeps, (Object *) nodes);
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


#define DEPENDENCIES_STR "dependencies"
#define DEPENDENCY_STR "dependency"
#define DEPENDENCY_SET_STR "dependency-set"

#define BUILD_STR "build"
#define DROP_STR "drop"
#define EXISTS_STR "exists"
#define DIFF_STR "diff"

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


#define makeVector(x) (x) ? (x): (x = vectorNew(10))

static void
addDependency(DagNode *node, DagNode *dep, BuildTypeBitSet condition)
{
    Vector *node_deps = makeVector(node->dependencies);
    Dependency *cond_dep;
    if (dep->type != OBJ_DAGNODE) {
	cond_dep = NULL;
    }
    assert((dep->type == OBJ_DAGNODE), "DEP IS NOT A DAGNODE!");
    cond_dep = dependencyNew(dep, condition);
    if (!setPush(node_deps, (Object *) cond_dep)) {
	objectFree((Object *) cond_dep, FALSE);
    }
}

static void
addConditionalDependency(DagNode *node, DagNode *dep, BuildTypeBitSet condition)
{
    boolean do_it = TRUE;
    if (condition) {
	switch (node->build_type) {
	case BUILD_NODE:
	    do_it = condition & BUILD_NODE_BIT; break;
	case DROP_NODE:
	    do_it = condition & DROP_NODE_BIT; break;
	case DIFF_NODE:
	    do_it = condition & DIFF_NODE_BIT; break;
	}
    }
    if (do_it) {
	addDependency(node, dep, condition);
    }
}

static void
addDependent(DagNode *node, DagNode *dep, BuildTypeBitSet condition)
{
    Vector *node_deps = makeVector(node->dependents);
    Dependency *cond_dep;
    assert((dep->type == OBJ_DAGNODE), "DEP IS NOT A DAGNODE!");
    cond_dep = dependencyNew(dep, condition);
    if (!setPush(node_deps, (Object *) cond_dep)) {
	objectFree((Object *) cond_dep, FALSE);
    }
}

void
addDep(DagNode *node, DagNode *dep, BuildTypeBitSet condition)
{
    assert((dep->type == OBJ_DAGNODE), "DEP IS NOT A DAGNODE!");
    assert((node->type == OBJ_DAGNODE), "NODE IS NOT A DAGNODE!");
    addDependency(node, dep, condition);
    addDependent(dep, node, condition);
}

static void
rmFromDepVector(Vector *vec, DagNode *node)
{
    int i;
    Dependency *dep;
    EACH(vec, i) {
	dep = (Dependency *) ELEM(vec, i);
	if (dep->dependency == node) {
	    vectorRemove(vec, i);
	    objectFree((Object *) dep, FALSE);
	}
    }
}

static void
rmDep(DagNode *node, DagNode *dep)
{
    if (dep->dependents) {
	rmFromDepVector(dep->dependents, node);
    }
    if (node->dependencies) {
	rmFromDepVector(node->dependencies, dep);
    }
}

/* This takes a vector of DagNodes or references to DagNodes. */
static void
addDepsVector(DagNode *node, Vector *deps)
{
    int i;
    Object *obj;
    Dependency *dep;

    EACH(deps, i) {
	obj = dereference(ELEM(deps, i));
	if (obj->type == OBJ_DAGNODE) {
	    addDep(node, (DagNode *) obj, 0);
	}
	else {
	    dep = (Dependency *) obj;
	    addDep(node, dep->dependency, dep->condition);
	    objectFree((Object *) dep, FALSE);
	}
    }
}

static boolean
isDependencySet(xmlNode *node)
{
    return streq(node->name, DEPENDENCY_SET_STR);
}

static boolean
isDependency(xmlNode *node)
{
    return streq(node->name, DEPENDENCY_STR);
}

static boolean
isDependencies(xmlNode *node)
{
    return streq(node->name, DEPENDENCIES_STR);
}

static boolean
isDepNode(xmlNode *node)
{
    return isDependency(node) || isDependencySet(node) || isDependencies(node);
}

static xmlNode *
nextDependency(xmlNode *cur) 
{
    xmlNode *node = firstElement(cur);
    while (node && !isDepNode(node)) {
	node = firstElement(node->next);
    }
    if (node && isDependencies(node)) {
	return nextDependency(node->children);
    }
    return node;
}

static Vector *
cons2Vector(Cons *cons)
{
    Vector *vector = vectorNew(10);
    Cons *cur;
    for (cur = cons; cur; cur = (Cons *) cur->cdr) {
	vectorPush(vector, cur->car);
    }
    return vector;
}

BuildTypeBitSet 
conditionForDep(xmlNode *node)
{
    String *condition_str = nodeAttribute(node, "condition");
    String separators = {OBJ_STRING, " ()"};
    Cons *contents;
    Cons *elem;
    char *head;
    BuildTypeBitSet bitset = 0;
    boolean inverted = FALSE;

    if (condition_str) {
	stringLowerOld(condition_str);
	elem = contents = stringSplit(condition_str, &separators);
	while (elem) {
	    head = ((String *) elem->car)->value;
	    if (streq(head, "build")) {
		bitset |= BUILD_NODE_BIT;
	    }
	    else if (streq(head, "drop")) {
		bitset |= DROP_NODE_BIT;
	    }
	    else if (streq(head, "not")) {
		inverted = TRUE;
	    }
	    else {
		RAISE(NOT_IMPLEMENTED_ERROR, 
		      newstr("no conditional dep handling for token %s", head));
	    }
	    elem = (Cons *) elem->cdr;
	}

	objectFree((Object *) condition_str, TRUE);
	objectFree((Object *) contents, TRUE);
	return inverted? (ALL_BUILDTYPE_BITS - bitset): bitset;
    }
    return 0;
}

static Vector *
explicitDepsForNode(
    xmlNode *node, 
    Hash *nodes_by_fqn, 
    Hash *nodes_by_pqn,
    BuildTypeBitSet parent_condition)
{
    Vector *vector = NULL;
    int i;
    Vector *next;
    Object *elem;
    Object *tmp;
    xmlNode *dep_node;
    String *fqn;
    String *pqn;
    BuildTypeBitSet condition;

    if (isDependencySet(node)) {
	condition = conditionForDep(node);
	for (dep_node = node->children;
	     dep_node = nextDependency(dep_node);
	     dep_node = dep_node->next) 
	{
	    if (next = explicitDepsForNode(dep_node, nodes_by_fqn, 
					   nodes_by_pqn, condition)) {
		if (vector) {
		    vectorAppend(vector, next);
		    objectFree((Object *) next, FALSE);
		}
		else {
		    vector = next;
		}
	    }
	}
    }
    else {
	condition = parent_condition;
	dep_node = node;
	if (fqn = nodeAttribute(dep_node, "fqn")) {
	    if (elem = hashGet(nodes_by_fqn, (Object *) fqn)) {
		vector = vectorNew(10);
		if (condition) {
		    elem = (Object *) dependencyNew((DagNode *) elem, 
						    condition);
		}
		vectorPush(vector, elem);
	    }
	    objectFree((Object *) fqn, TRUE);
	}
	else if (pqn = nodeAttribute(dep_node, "pqn")) {
	    if (elem = hashGet(nodes_by_pqn, (Object *) pqn)) {
		vector = cons2Vector((Cons *) elem);
		if (condition) {
		    EACH(vector, i) {
			elem = dereference(ELEM(vector, i));
			elem = (Object *) dependencyNew((DagNode *) elem, 
							condition);
			ELEM(vector, i) = elem;
		    }
		}
	    }
	    objectFree((Object *) pqn, TRUE);
	}
    }
    return vector;
}



static DagNodeBuildType
buildTypeForNode(Node *node)
{
    String *diff;
    String *action;
    String *tmp;
    String *fallback;
    String *fqn;
    char *errmsg = NULL;
    DagNodeBuildType build_type = UNSPECIFIED_NODE;

    if (fallback = nodeAttribute(node->node , "fallback")) {
	objectFree((Object *) fallback, TRUE);
	/* By default, we don't want to do anything for a fallback
 	 * node.  Marking it as an exists node achieves that.  The
 	 * build_type will be modified if anything needs to actually
 	 * reference the fallback node.  */
	build_type = EXISTS_NODE;
    }
    else if (diff = nodeAttribute(node->node , "diff")) {
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
	    errmsg = newstr("identifyBuildTypes: unexpected diff "
			    "type \"%s\" in %s", diff->value, fqn->value);
	}
	objectFree((Object *) diff, TRUE);
    }
    else if (action = nodeAttribute(node->node , "action")) { 
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
	    errmsg = newstr("identifyBuildTypes: unexpected action "
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
		errmsg = newstr("identifyBuildTypes: cannot identify "
				"build type for node %s", 
				diff->value, fqn->value);
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
addNodeToVector(Object *this, Object *vector)
{
    Node *node = (Node *) this;
    DagNodeBuildType build_type;
    DagNode *dagnode;

    if (streq(node->node->name, "dbobject")) {
	dagnode = dagnodeNew(node->node, UNSPECIFIED_NODE);
	dagnode->build_type = buildTypeForNode(node);
	vectorPush((Vector *) vector, (Object *) dagnode);
    }

    return NULL;
}

Hash *
hashByFqn(Vector *vector)
{
    int i;
    DagNode *node;
    String *key;
    Object *old;
    Hash *hash = hashNew(TRUE);

    EACH(vector, i) {
	node = (DagNode *) ELEM(vector, i);
	assert(node->type == OBJ_DAGNODE, "Incorrect node type");
	key = stringDup(node->fqn);

	if (old = hashAdd(hash, (Object *) key, (Object *) node)) {
	    RAISE(GENERAL_ERROR, 
		  newstr("hashbyFqn: duplicate node \"%s\"", key->value));
	}
    }
    return hash;
}

static Hash *
hashByPqn(Vector *vector)
{
    int i;
    DagNode *node;
    String *key;
    Cons *entry ;
    Hash *hash = hashNew(TRUE);
    Object *new;
    xmlChar *pqn;

    EACH(vector, i) {
	node = (DagNode *) ELEM(vector, i);
	assert(node->type == OBJ_DAGNODE, "Incorrect node type");
	pqn = xmlGetProp(node->dbobject, "pqn");
	if (pqn) {
	    key = stringNew(pqn);
	    new = (Object *) objRefNew((Object *) node);
	    if (entry = (Cons *) hashGet(hash, (Object *) key)) {
		consAppend(entry, new);
	    }
	    else {
		entry = consNew(new, NULL);
		hashAdd(hash, (Object *) key, (Object *) entry);
	    }
	    xmlFree(pqn);
	}
    }
    return hash;
}


static void
identifyParents(Vector *nodes, Hash *nodes_by_fqn)
{
    int i;
    DagNode *node;
    xmlNode *parent;
    String *parent_fqn;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	assert(node->type == OBJ_DAGNODE, "incorrect node type");
	assert(node->dbobject, "identifyParents: no dbobject node");
	parent = findAncestor(node->dbobject, "dbobject");
	parent_fqn  = nodeAttribute(parent, "fqn");
	if (parent_fqn) {
	    node->parent = (DagNode *) hashGet(nodes_by_fqn, 
					       (Object *) parent_fqn);
	    assert(node->parent, 
		   "identifyParents: parent of %s (%s) not found",
		   node->fqn->value, parent_fqn->value);
	    objectFree((Object *) parent_fqn, TRUE);
	}
    }
}

#define CURDEP(node) ELEM(node->dependencies, node->dep_idx)


static void recordRebuildNode(DagNode *node, Vector *nodelist);

static void
findRebuildNodesInVector(
    Vector *vector, 
    BuildTypeBitSet looking_for,
    Vector *foundlist)
{
    int i;
    Dependency *dep;
    DagNode *node;
    if (vector) {
	EACH(vector, i) {
	    dep = (Dependency *)ELEM(vector, i);
	    node = dep->dependency;
	    if (node->status == RESOLVED) {
		if (inBuildTypeBitSet(looking_for, node->build_type)) {
		    recordRebuildNode(node, foundlist);
		}
	    }
	}
    }
}

static void
recordRebuildNode(DagNode *node, Vector *nodelist)
{
    node->status = UNVISITED;
    vectorPush(nodelist, (Object *) node);
    findRebuildNodesInVector(node->dependents, BUILD_NODE_BIT + DIFF_NODE_BIT,
				  nodelist);
    findRebuildNodesInVector(node->dependencies, DROP_NODE_BIT,
				  nodelist);
}

static void
checkRebuildParent(DagNode *node)
{
    if (node->parent && (node->parent->build_type == BUILD_NODE)) {
	RAISE(TSORT_ERROR, 
	      newstr("Cannot rebuild %s when parent (%s) is designated "
		     "for build", node->fqn->value, node->parent->fqn->value));
    }
}

/* Return a vector of nodes that must be dropped and built.  Such nodes
 * will either have been directly designated as such, or will have been
 * promoted due to their dependencies on other such nodes. 
 */
static Vector *
identifyRebuildNodes(Vector *nodes)
{
    Vector *rebuild_nodes = vectorNew(nodes->elems);
    DagNode *node;
    int i;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	assert(node->type == OBJ_DAGNODE, "incorrect node type");
	if (node->status == RESOLVED) {
	    if (node->build_type == REBUILD_NODE) {
		checkRebuildParent(node);
		recordRebuildNode(node, rebuild_nodes);
	    }
	}
    }
    
    return rebuild_nodes;
}

/* We duplicate both the dependencies and dependents of each primary to
 * the dup node.  Although in principle we only need to do one or the
 * other, since addDup creates both dependencies and dependents, it is
 * not possible to use either just the dependencies or just the
 * dependents as source data.  This is because when we propagated our
 * rebuild_nodes some will have been done in the dependency
 * direction and some in the dependent direction.
 */
static void
duplicateDeps(DagNode *primary, DagNode *dup)
{
    Vector *deps;
    Dependency *dep;
    DagNode *depnode;
    boolean doing_dependencies = FALSE;
    int i;
    while (TRUE) {
	deps = doing_dependencies? primary->dependencies: primary->dependents;

	if (deps) {
	    EACH(deps, i) {
		dep = (Dependency *) ELEM(deps, i);
		depnode = dep->dependency;
		if (depnode->mirror_node) {
		    depnode = depnode->mirror_node;
		}
		if (doing_dependencies) {
		    addDep(dup, depnode, dep->condition);
		}
		else {
		    addDep(depnode, dup, dep->condition);
		}
	    }
	}
	if (doing_dependencies) {
	    break;
	}
	doing_dependencies = TRUE;
    }
}

/* For each node designated to be dropped and rebuilt, duplicate the
 * node including its dependencies etc, add it to our vector of nodes
 * and set the build_type for each node of the pair.
 */
static void
duplicateRebuildNodes(Vector *nodes, Vector *rebuild_nodes)
{
    DagNode *primary;
    DagNode *dup;
    int i;

    EACH(rebuild_nodes, i) {
	primary = (DagNode *) ELEM(rebuild_nodes, i);
	assert(primary->type == OBJ_DAGNODE, "incorrect node type!");
	dup = dagnodeNew(primary->dbobject, DROP_NODE);
	primary->build_type = BUILD_NODE;
	primary->mirror_node = dup;
	vectorPush(nodes, (Object *) dup);
    }
    EACH(rebuild_nodes, i) {
	primary = (DagNode *) ELEM(rebuild_nodes, i);
	assert(primary->type == OBJ_DAGNODE, "incorrect node type!");
	dup = primary->mirror_node;
	duplicateDeps(primary, dup);
        addDep(primary, dup, 0);
	if (primary->breaker_for) {
	    dup->breaker_for = primary->breaker_for->mirror_node;
	    /* Duplicate thd dbobject so that it can be safely freed in
	     * dgnodeFree() */
	    dup->dbobject = xmlCopyNode(primary->dbobject, 1);
	}
    }
    return;
}

typedef enum {
    REDIRECT_RETAIN,
    REDIRECT_INVERT,
    REDIRECT_DROP,
    REDIRECT_CHECK_CYCLE,
    REDIRECT_ERROR,
    REDIRECT_UNIMPLEMENTED
} RedirectAction;

/* Return the type of redirect action needed to satisfy dependencies
 * from node to dep.  Eg from a build node to a buld node, the
 * dependency is kept as is (REDRIRECT_RETAIN), and from a drop node to
 * a drop node, the dependency must be inverted (REDIRECT_INVERT).
 */
static RedirectAction 
getRedirectionAction(DagNode *node, DagNode *dep)
{
    RedirectAction result;
    static RedirectAction redirect_action[4][4] = {
	{REDIRECT_RETAIN, REDIRECT_RETAIN,
	 REDIRECT_RETAIN, REDIRECT_DROP},          /* BUILD_NODE */
	{REDIRECT_INVERT, REDIRECT_INVERT,
	 REDIRECT_RETAIN, REDIRECT_DROP},          /* DROP_NODE */
	{REDIRECT_UNIMPLEMENTED, REDIRECT_UNIMPLEMENTED,
	 REDIRECT_UNIMPLEMENTED, REDIRECT_DROP},          /* DIFF_NODE */
	{REDIRECT_RETAIN, REDIRECT_RETAIN,
	 REDIRECT_DROP, REDIRECT_DROP}                    /* EXISTS_NODE */
    };

    assert(node->type == OBJ_DAGNODE, 
	   "Unexpected node type (%d)\n", node->type);
    assert(dep->type == OBJ_DAGNODE, 
	   "Unexpected dep type (%d)\n", dep->type);
    assert(node->build_type <= EXISTS_NODE,
	   "Unexpected build_type (%d) in node\n", node->build_type);
    assert(dep->build_type <= EXISTS_NODE,
	   "Unexpected build_type (%d) in dep\n", dep->build_type);

    result = redirect_action[node->build_type][dep->build_type];

    if (result == REDIRECT_INVERT) {
	if (dep->breaker_for) {
	    /* We do not invert dependencies on breaker nodes.  This
	     * is because the breaker node has to perform special
	     * operations for both build and drop and they must be
	     * performed before the  normal operations in both cases.
	     */
	    return REDIRECT_CHECK_CYCLE;
	    
	}
	if (node->is_fallback) {
	    /* This is a fallback node.  We don't invert the
	     * dependencies from this node. */
	    return REDIRECT_RETAIN;
	}
    }
    return result;
}

static boolean
isCycleNode(DagNode *this, DagNode *breaker)
{
    DagNode *elem;
    int i;
    EACH(breaker->dependents, i) {
	if (this == (DagNode *) ELEM(breaker->dependents, i)) {
	    assert(this->type == OBJ_DAGNODE, "incorrect node type");
	    return TRUE;
	}
	assert(this->type == OBJ_DAGNODE, "incorrect node type");
    }
    return FALSE;
}

static void
eliminateDependency(DagNode *node, int i, DagNode *breaker)
{
    DagNode *dep = (DagNode *) ELEM(node->original_dependencies, i);

    assert(dep->type == OBJ_DAGNODE,
	   "Need to be able to handle DEPENDENCY OBJECTS");
    /* Remove from original_dependendencies first.  Since we are
     * iterating over that vector in a caller, we will simply replace
     * the entry with one on breaker, which should be safe. */
    ELEM(node->original_dependencies, i) = (Object *) breaker;
    
    /* Since we may have already built our new dependencies for this
     * node, we must eliminate the potentially recreated (and inverted)
     * dependency. */
    setDel(dep->dependencies, (Object *) node);
}

static DagNode *
findCycleEnd(DagNode *start, DagNode *breaker)
{
    DagNode *this = start;
    DagNode *next;
    int i;
    boolean continuing = TRUE;

    while (continuing) {
	continuing = FALSE;
	EACH(this->original_dependencies, i) {
	    next = (DagNode *) ELEM(this->original_dependencies, i);
	    assert(next->type == OBJ_DAGNODE, "NEED TO HANDLE DEP TYPES");
	    if (isCycleNode(next, breaker)) {
		this = next;
		continuing = TRUE;
		break;
	    }
	}
    }
    return this;
}

static void
maybeInvertCycle(DagNode *node, DagNode *breaker)
{
    int i;
    Dependency *dep;
    DagNode *this;
    Vector *deps;

    if (node == breaker->breaker_for) {
	EACH(node->original_dependencies, i) {
	    dep = (Dependency *) ELEM(node->original_dependencies, i);
	    this = dep->dependency;
	    if (isCycleNode(this, breaker)) {
		/* First, eliminate the dependency on our cycle_node.  */
		eliminateDependency(node, i, breaker);

		/* Next, add a dependency on node from the last element
		 * of the cycle chain.  */
		this = findCycleEnd(this, breaker);
		
		/* Note that this dependency has to be inverted.  */
		addConditionalDependency(node, this, 0);
	    }
	}
    }
}

/* The dependency graph on entry to this function is solely directed in
 * the direction of our declared and implicit (child->parent)
 * dependencies regardless of the operation (build, drop, diff) being
 * performed.  This function redirects dependencies based upon the
 * operations being performed at each node.  On exit from this function
 * we have a DAG which can be used to establish the order of
 * operations using a tsort.
 * 
 * When inverting a dependency cycle that we have previously broken, the
 * cycle must be rebuilt and the break placed elsewhere in the cycle
 * before we invert the cycle's dependencies.
 * Eg given an original cycle: 
 *   v1->v2, v2->v3, v3->v1
 * The broken version would be:
 *   v1->v2, v1->v1b, v2->v3, v2->v1b, v3->v1b
 * When we rebuild the cycle we will get:
 *   v1->v2, v1->v1b, v2->v3, v2->v1b, v3->v1, v3->v1b
 * And when we re-break it, we get:
 *   v1->v1b, v2->v3, v2->v1b, v3->v1, v3->v1b
 * This can now be inverted to give:
 *   v1->v3, v1->v1b, v2->v1b, v3->v2, v3->v1b
 * Note that the dependencies on the breaker node are not inverted.
 */
static void
redirectDependencies(Vector *nodes)
{
    DagNode *node;
    Dependency *dep;
    DagNode *depnode;
    Vector *deps;
    Object *ref;
    RedirectAction action;
    char *errstr;
    char *nodestr;
    char *depstr;
    int i, j;
    int elems;

    EACH(nodes, i) {
	/* Take a temporary copy of the original dependencies, and then
	 * recreate the dependencies vector.  We will re-populate it
 	 * below. */ 
	node = (DagNode *) ELEM(nodes, i);
	elems = node->dependencies? node->dependencies->elems: 1;
	node->original_dependencies = node->dependencies;
	node->dependencies = vectorNew(elems);
    }
	
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
    	assert(node->type == OBJ_DAGNODE, "incorrect node type");
	if (deps = node->original_dependencies) {
	    EACH(deps, j) {
		dep = (Dependency *) ELEM(deps, j);
		depnode = dep->dependency;
		action = getRedirectionAction(node, depnode);
		switch (action) {
		case REDIRECT_INVERT:
		    addConditionalDependency(depnode, node, dep->condition);
		    break;
		case REDIRECT_CHECK_CYCLE:
		    /* If node is part of a cycle for which we have a
		     * cycle breaker, then inversion of the cycle
		     * requires that the dependency originally removed
		     * from the cycle must be replaced, and the cyclic
		     * dependency from the cycle node must be removed
		     * (see notes above). */
		    maybeInvertCycle(node, depnode);
		    /* Deliberate flow-thru to the next case. */
		case REDIRECT_RETAIN:
		    addConditionalDependency(node, depnode, dep->condition);
		    /* Deliberate flow-thru to the next case. */
		case REDIRECT_DROP:
		    break;
		case REDIRECT_UNIMPLEMENTED:
		    nodestr = objectSexp((Object *) node);
		    depstr = objectSexp((Object *) depnode);
		    errstr = newstr("No dependency redirection defined "
				    "for %s -> %s", nodestr, depstr);
		    skfree(nodestr);
		    skfree(depstr);
		    RAISE(NOT_IMPLEMENTED_ERROR, errstr);
		    
		default:
		    dbgSexp(node);
		    dbgSexp(depnode);
		    RAISE(NOT_IMPLEMENTED_ERROR, 
			  newstr("Nothing defined for redirect action "
				 "type %d", action));
		}
	    }
	}
    }
    /* Remove vectors that are no longer up to date or no longer
     * useful. */
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	objectFree((Object *) node->dependents, TRUE);
	objectFree((Object *) node->original_dependencies, TRUE);
	node->original_dependencies = NULL;
	node->dependents = NULL;
    }
}

static DagNode *
makeBreakerNode(DagNode *from_node, String *breaker_type)
{
    xmlNode *dbobject = from_node->dbobject;
    xmlChar *old_fqn = xmlGetProp(dbobject, "fqn");
    char *fqn_suffix = strstr(old_fqn, ".");
    char *new_fqn = newstr("%s%s", breaker_type->value, fqn_suffix);
    xmlNode *breaker_dbobject = xmlCopyNode(dbobject, 1);
    DagNode *breaker;
    Dependency *dep;
    int i;
    xmlSetProp(breaker_dbobject, "type", breaker_type->value);
    xmlUnsetProp(breaker_dbobject, "cycle_breaker");
    xmlSetProp(breaker_dbobject, "fqn", new_fqn);
    xmlFree(old_fqn);
    skfree(new_fqn);

    breaker = dagnodeNew(breaker_dbobject, from_node->build_type);
    breaker->parent = from_node->parent;
    breaker->breaker_for = from_node;

    /* Copy dependencies of from_node to breaker.  We will eliminate
     * unwanted ones later. */
    EACH (from_node->dependencies, i) {
	dep = (Dependency *) ELEM(from_node->dependencies, i);
	addDep(breaker, dep->dependency, dep->condition);
    }
    return breaker;
}

static DagNode *
getBreakerFor(DagNode *node)
{
    String *breaker_type = nodeAttribute(node->dbobject, "cycle_breaker");
    DagNode *breaker = NULL;

    if (breaker_type) {
	breaker = makeBreakerNode(node, breaker_type);
	objectFree((Object *) breaker_type, TRUE);
    }
    return breaker;
}

/* Put breaker into the DAG
 */
static void
processBreaker(DagNode *node, DagNode *breaker)
{
    DagNode *node_in_cycle;
    Dependency *dep;
    DagNode *breaker_for;

    /* Remove the cycle node in breaker that we copied from the
     * original node (breaker_for). */
    dep = (Dependency *) CURDEP(node);
    breaker_for = dep->dependency;
    dep = (Dependency *) CURDEP(breaker_for);
    node_in_cycle = dep->dependency;
    rmDep(breaker, node_in_cycle);

    /* Add deps to the breaker from each node in the cycle. */
    node_in_cycle = breaker_for;
    while (node_in_cycle != node) {
	addDep(node_in_cycle, breaker, 0);

	dep = (Dependency *) CURDEP(node_in_cycle);
	node_in_cycle = dep->dependency;
    }

    /* Replace existing dependency on cycle_node from node, with a
     * dependency on breaker. */
    rmDep(node, breaker_for);
    addDep(node, breaker, 0);
}




static DagNode *
getFallbackNode(xmlNode *dep_node, Hash *nodes_by_fqn)
{
    String *fallback;
    DagNode *fallback_node = NULL;
    char *errmsg;
    if (isDependencySet(dep_node)) {
	fallback = nodeAttribute(dep_node, "fallback");
	if (fallback) {
	    fallback_node = (DagNode *) hashGet(nodes_by_fqn, 
						(Object *) fallback);
	    if (!fallback_node) {
		errmsg = newstr("Fallback node %s not found", 
				fallback->value);
		objectFree((Object *) fallback, TRUE);
		RAISE(TSORT_ERROR, errmsg);
	    }
	    objectFree((Object *) fallback, TRUE);
	}
    }
    return fallback_node;
}


static void
dropSubNode(DagNode *node)
{
    DagNode *parent = node->supernode;
    DagNode **nodeptr = &(parent->subnodes);

    while (*nodeptr) {
	if (*nodeptr == node) {
	    *nodeptr = node->subnodes;
	    node->supernode = NULL;  /* Without this the node will not
				      * be freed */
	    objectFree((Object *) node, TRUE);
	    break;
	}
	nodeptr = &((*nodeptr)->subnodes);
    }
}

static DagNode *
addSubNode(DagNode *node)
{
    DagNode *this;
    this = dagnodeNew(node->dbobject, OPTIONAL_NODE);
    this->subnodes = node->subnodes;
    this->supernode = node;
    node->subnodes = this;
    return this;
}

/* Record the dependencies for a single node. */
static void
addDepsForNode(
    DagNode *node, 
    Vector *nodes,
    Hash *nodes_by_fqn, 
    Hash *nodes_by_pqn)
{
    xmlNode *dep_node;
    DagNode *fallback_node;
    Vector *deps;
    DagNode *this;
    String *tmp;
    String *errmsg;

    assert(node, "addDepsForNode: no node provided");
    assert(node->dbobject, "addDepsForNode: node has no dbobject");

    for (dep_node = node->dbobject->children;
	 dep_node = nextDependency(dep_node);
	 dep_node = dep_node->next) 
    {
	fallback_node = getFallbackNode(dep_node, nodes_by_fqn);
	deps = explicitDepsForNode(dep_node, nodes_by_fqn, 
				   nodes_by_pqn, 0);

	if (fallback_node || (deps && (deps->elems > 1))) {
	    /* We have a set of optional dependencies.  Create a new subnode
	     * where they will be recorded. */
	    this = addSubNode(node);
	    this->fallback = fallback_node;
	}
	else {
	    if (!deps) {
		if (isDependencySet(dep_node)) {
		    RAISE(TSORT_ERROR, 
			  newstr("Unable to find any dependency for dependency "
				 "set in %s\n(Perhaps a fallback needs to be "
				 "defined by the skit template developer)", 
				 node->fqn->value));
		}
		tmp = nodeAttribute(dep_node, "fqn");
		errmsg = newstr("Unable to find dependency %s in %s", 
				tmp->value, node->fqn->value);
		objectFree((Object *) tmp, TRUE);
		RAISE(TSORT_ERROR, errmsg);
	    }

	    this = node;  /* this is the node to which we will add deps */
	}

	if (deps) {
	    addDepsVector(this, deps);
	    objectFree((Object *) deps, FALSE);
	}
    }
}


/* Create an initial dependency graph.
 */
Vector *
nodesFromDoc(Document *doc)
{
    Vector *volatile nodes = vectorNew(1000);
    Hash *volatile byfqn = NULL;
    Hash *volatile bypqn = NULL;
    DagNode *node;
    int i;

    BEGIN {
	(void) xmlTraverse(doc->doc->children, &addNodeToVector, 
			   (Object *) nodes);
	byfqn = hashByFqn(nodes);
	bypqn = hashByPqn(nodes);
	identifyParents(nodes, byfqn);
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    assert(node->type == OBJ_DAGNODE, "incorrect node type");
	    addDepsForNode(node, nodes, byfqn, bypqn);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) nodes, TRUE);
    }
    FINALLY {
	objectFree((Object *) byfqn, FALSE);
	objectFree((Object *) bypqn, TRUE);
    }
    END;
    return nodes;
}


/* Activate a fallback node.  
 * A fallback node is one that is used when no dependencies in a set can
 * be satisfied.  Typically this would be for privileges for the owner
 * of an object, and if no suitable privilege assignment can be found in
 * the set of dependencies, a superuser privilege would be assigned
 * temporarily.
 * Fallback nodes are handled as pairs of nodes, one to activate the
 * fallback (eg grant the temporary privilege), and one to deactivate
 * it (eg revoke it).
 */
static void
activateFallback(Vector *nodelist, DagNode *node)
{
    DagNode *fallback = node->fallback;
    DagNode *closer;
    Dependency *dep;
    int i;

    if (fallback->build_type != BUILD_NODE) {
	/* Promote this node from an exists node into a build node, and
	 * create the matching drop node for it. */
	fallback->build_type = BUILD_NODE;
	closer = dagnodeNew(fallback->dbobject, DROP_NODE);
	closer->is_fallback = TRUE;
	closer->parent = fallback->parent;
	fallback->mirror_node = closer;
	vectorPush(nodelist, (Object*) closer);

	EACH(fallback->dependencies, i) {
	    dep = (Dependency *) ELEM(fallback->dependencies, i);
	    addDep(closer, dep->dependency, 0);
	}
    }
    else {
	closer = fallback->mirror_node;
    }
    /* Add dependencies to and from the fallback and closer. */
    //node->fallback_build_type = BUILD_NODE;
    addDep(node->supernode, fallback, 0);
    addDep(closer, node->supernode, 0);

    /* Eliminate the optional deps that the fallback is for.  */
    dropSubNode(node);
}


static void tsort_node(Vector *nodelist, DagNode *node, Vector *results);

static boolean
tsort_deps(Vector *nodelist, DagNode *node, Vector *results)
{
    Vector *volatile deps;
    volatile int i;
    Dependency *dep;
    DagNode *depnode;
    boolean in_cycle;
    DagNode *cycle_node;
    DagNode *breaker;
    DagNode *supernode;
    char *errmsg;
    char *tmpmsg;

    if (deps = node->dependencies) {
	EACH(deps, i) {
	    node->dep_idx = i;
	    dep = (Dependency *) ELEM(deps, i);
	    depnode = dep->dependency;
	    BEGIN {
		tsort_node(nodelist, depnode, results);
		in_cycle = FALSE;
	    }
	    EXCEPTION(ex);
	    WHEN(TSORT_CYCLIC_DEPENDENCY) {
		in_cycle = TRUE;
		cycle_node = (DagNode *) ex->param;
		errmsg = newstr("%s", ex->text);
	    }
	    END;

	    if (in_cycle) {
		if (breaker = getBreakerFor(depnode)) {
		    /* We have a breaker for depnode.  Add it to our
		     * nodes vector. */
		    vectorPush(nodelist, (Object *) breaker);
		    processBreaker(node, breaker);

		    skfree(errmsg);

		    i--;        /* The current depnode will have been  */
		    continue;   /* replaced, so repeat this iteration. */
		}

		if (node->supernode) {
		    /* In a subnode, only a single dependency from the
		     * vector needs to succeed. */

		    if ((i + 1) < deps->elems) {
			/* Try the next dep in the list. */
			skfree(errmsg);
			continue;
		    }
		}

		if (node == cycle_node) {
		    /* We are at the start of the cyclic dependency.
		     * Set errmsg to describe this, and reset cycle_node
		     * for the RAISE below. */ 
		    tmpmsg = newstr("Cyclic dependency detected: %s->%s", 
				    node->fqn->value, errmsg);
		    cycle_node = NULL;
		}
		else if (cycle_node) {
		    /* We are somewhere in the cycle of deps.  Add the
		     * current node to the error message. */ 
		    tmpmsg = newstr("%s->%s", node->fqn->value, errmsg);
		}
		else {
		    /* We are outside of the cyclic deps.  Add this node
		     * to the error message so we can see how we got
		     * to this point.  */ 
		    tmpmsg = newstr("%s from %s", errmsg, node->fqn->value);
		}

		skfree(errmsg);
		errmsg = tmpmsg;
		RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg, cycle_node);
	    }

	    /* We are not in a dependency cycle.  If this is a subnode,
	     * we have successfully found a suitable dependency, which
	     * we have recorded in node->dep_idx.  If not, we process
	     * the rest of the dependencies. */
	    if (node->supernode) {
		return FALSE;
	    }
	}
    }
    if (node->supernode) {
	/* We get to this point if we are in a subnode and no dependency
	 * has been found. */
	if (node->fallback) {
	    activateFallback(nodelist, node);
	    /* Now we need to try the tsort again from the supernode */
	    return TRUE;
	}
	else {
	    RAISE(TSORT_ERROR, 
		  newstr("No dependency found in dependency set for %s", 
			 node->supernode->fqn->value));
	}
    }
    return FALSE;
}

/* Return a pointer to the node status.  If this is a subnode, the node
 * status is given by the supernode.
 */
static DagNodeStatus *
nodeStatusP(DagNode *node)
{
    if (node->supernode) {
	return &(node->supernode->status);
    }
    return &(node->status);
}

static boolean
tsort_subnodes(
    Vector *nodelist,
    DagNode *node,
    Vector *results)
{
    /* When we get here, we will have already processed the non-optional
     * dependencies of our supernode. */
    while (node) {
	if (tsort_deps(nodelist, node, results)) {
	    /* tsort_deps has indicated that a retry is required.  This
	     * will be because we have replaced optional dependencies
	     * with a fallback.  In such a case we must exit and signal
	     * the need for a retry to our caller. */
	    return TRUE;
	}
	node = node->subnodes;
    }
    return FALSE;
}

static void
tsort_node(
    Vector *nodelist,
    DagNode *node,
    Vector *results)
{
    DagNodeStatus *p_status = nodeStatusP(node);
    boolean retry;

    switch (*p_status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("%s", node->fqn->value), node);
    case UNVISITED: 
	BEGIN {
	    *p_status = VISITING;
	    do {
		retry = tsort_deps(nodelist, node, results);
		if (node->supernode) {
		    /* node is a sub-node.  Now handle the deps for the
		     * parent. */ 
		    RAISE(NOT_IMPLEMENTED_ERROR, 
			  newstr("TSORT_DEPS of supernode"));
		    //tsort_deps(nodes, node->supernode, results);
		}
		else if (node->subnodes) {
		    /* node is a parent-node.  Handle optional deps */
		    retry = tsort_subnodes(nodelist, node->subnodes, results);
		}
	    } while (retry);
	}
	EXCEPTION(ex) {
	    *p_status = UNVISITED;
	    RAISE();
	}
	END;
	*p_status = RESOLVED;
	vectorPush(results, (Object *) node);
	break;
    case RESOLVED: 
	break;
    default:
	RAISE(TSORT_ERROR,
		  newstr("Unexpected status for dagnode %s: %d",
			 node->fqn->value, node->status));
    }
}

/* Convert the dependency graph into a true DAG.  This is a variant on
 * the standard tsort algorithm with tweaks to allow optional
 * dependencies and breakable cyclic dependencies.  */
Vector *
resolving_tsort(Vector *nodelist)
{
    Vector *volatile results = vectorNew(nodelist->elems + 10);
    DagNode *node;
    int i;
    BEGIN {
	EACH(nodelist, i) {
	    node = (DagNode *) ELEM(nodelist, i);
	    tsort_node(nodelist, node, results);
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

static void
resolveOneSubNode(DagNode *node, DagNode *sub)
{
    Dependency *dep;
    DagNode *depnode;
    int i;

    EACH(sub->dependencies, i) {
	dep = (Dependency *) ELEM(sub->dependencies, i);
	depnode = dep->dependency;
	/* Remove the optional dependent. */
	rmFromDepVector(depnode->dependents, sub);

	if (i == sub->dep_idx) {
	    /* Add depnode as a non-optional dependency to node. */
	    addDep(node, depnode, dep->condition);
	}
    }
    objectFree((Object *) sub->dependencies, TRUE);
    sub->dependencies = NULL;
}

/* Make the optional deps into actual ones. */
static void
resolveSubNodes(Vector *nodes)
{
    int i;
    DagNode *this;
    DagNode *sub;
    DagNode *next;
    Dependency *dep;
    EACH(nodes, i) {
	this = (DagNode *) ELEM(nodes, i);
	sub = this->subnodes;
	while (sub) {
	    next = sub->subnodes;
	    resolveOneSubNode(this, sub);
	    sub = next;
	}
    }
}

void
prepareDagForBuild(Vector **p_nodes)
{
    int i;
    DagNode *node;
    Vector *sorted = resolving_tsort(*p_nodes);
    Vector *rebuild_nodes = NULL;
    objectFree((Object *) *p_nodes, FALSE);
    *p_nodes = sorted;

    //fprintf(stderr, "\n\n1\n\n");
    //showVectorDeps(sorted);
    resolveSubNodes(sorted);
    rebuild_nodes = identifyRebuildNodes(sorted);
    //fprintf(stderr, "\n\n3\n\n");
    //dbgSexp(rebuild_nodes);
    //showVectorDeps(sorted);
    duplicateRebuildNodes(sorted, rebuild_nodes);
    //fprintf(stderr, "\n\n4\n\n");
    //showVectorDeps(sorted);

    objectFree((Object *) rebuild_nodes, FALSE);
    redirectDependencies(sorted);
    //fprintf(stderr, "\n\n5\n\n");
    //showVectorDeps(sorted);
}
