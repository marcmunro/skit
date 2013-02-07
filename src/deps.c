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


/*
void
showDeps(DagNode *node)
{
    DagNode *sub = node->subnodes;
    if (node) {
	printSexp(stderr, "NODE: ", (Object *) node);
	printSexp(stderr, "-->", (Object *) node->dependencies);
	while (sub) {
	    if (sub->dependencies || sub->fallback) {
		printSexp(stderr, "optional->", (Object *) sub->dependencies);
		if (sub->fallback) {
		    printSexp(stderr, "flbk->", (Object *) sub->fallback);
		}
	    }
	    sub = sub->subnodes;
	}
	printSexp(stderr, "<--", (Object *) node->dependents);
    }
}
*/

 /*
static Object *
hashEachShowDeps(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    showDeps(node);
    return (Object *) node;
}
*/
  /*
void
showHashDeps(Hash *nodes)
{
    hashEach(nodes, &hashEachShowDeps, (Object *) nodes);
}
*/

   /*
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
   */

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

#ifdef wibble
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
#endif

#ifdef wibble
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
#endif

#ifdef wibble
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
#endif

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

#ifdef wibble
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
#endif


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
/*
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
*/
 /*
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
    //printSexp(stderr, "FQN HASH: ", (Object *) hash);
    return hash;
}
 */

  /*
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
    //printSexp(stderr, "PQN HASH: ", (Object *) hash);
    return hash;
}
  */

/*
 * For each DagNode in the nodes Vector, identify the parent node and
 * record it in the child node's parent field.
 */
#ifdef wibble
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
#endif
#define CURDEP(node) ELEM(node->dependencies, node->dep_idx)

#ifdef wibble
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
	    //dbgSexp(dep);
	    node = dep->dependency;
	    if (node->status == RESOLVED) {
		//fprintf(stderr, "Looking for %d in %d\n", node->build_type, looking_for);
		if (inBuildTypeBitSet(looking_for, node->build_type)) {
		    //printSexp(stderr, "FOUND: ", node);
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
    //dbgSexp(node);
    //dbgSexp(node->dependents);
    //dbgSexp(node->dependencies);
    findRebuildNodesInVector(node->dependents, EXISTS_NODE_BIT + DIFF_NODE_BIT,
				  nodelist);
    // I THINK THE FOLLOWING IS JUST PLAIN WRONG 20121130
    //findRebuildNodesInVector(node->dependencies, DROP_NODE_BIT,
    //				  nodelist);
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
#endif

/* Return a vector of nodes that must be dropped and built.  Such nodes
 * will either have been directly designated as such, or will have been
 * promoted due to their dependencies on other such nodes. 
 */
#ifdef wibble
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
#endif
/* We duplicate both the dependencies and dependents of each primary to
 * the dup node.  Although in principle we only need to do one or the
 * other, since addDup creates both dependencies and dependents, it is
 * not possible to use either just the dependencies or just the
 * dependents as source data.  This is because when we propagated our
 * rebuild_nodes some will have been done in the dependency
 * direction and some in the dependent direction.
 */
#ifdef wibble
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
#endif
/* For each node designated to be dropped and rebuilt, duplicate the
 * node including its dependencies etc, add it to our vector of nodes
 * and set the build_type for each node of the pair.
 */
#ifdef wibble
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
	dup->mirror_node = primary;
	dup->parent = primary->parent;
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
	    /* Duplicate the dbobject so that it can be safely freed in
	     * dgnodeFree() */
	    dup->dbobject = xmlCopyNode(primary->dbobject, 1);
	}
    }
    return;
}
#endif

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
#ifdef wibble
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
#endif

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
#ifdef wibble
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

/* Replace the current dependency on depnode with one instead on
* breaker.  Note that breaker has been created with the same
* dependencies as depnode, so we must remove from it the dependency that
* causes the cycle.  Then we replace the current dependency on depnode
* with one instead on breaker.
*/
static void 
processBreaker2(DagNode *curnode, DagNode *depnode, DagNode *breaker)
{
    Dependency *dep = (Dependency *) CURDEP(depnode);
    DagNode *to_eliminate = dep->dependency;

    rmDep(breaker, to_eliminate);
    dep = (Dependency *) CURDEP(curnode);
    dep->dependency = breaker;
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
		if (!(tmp = nodeAttribute(dep_node, "fqn"))) {
		    if (!(tmp = nodeAttribute(dep_node, "pqn"))) {
			tmp = stringNew("unknown");
		    }
		}
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
#endif


/* Create an initial dependency graph from a source xml document.
 * Return the graph as a Vector of DagNodes.
 */
#ifdef wibble
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
#endif


/* 
 * Dependency handling in skit has 2 features that prevent the standard
 * topological sort from being directly used:
 * 1) We allow optional dependencies
 * 2) Cyclic dependencies can happen
 *
 * This means that the dependency graph as derived directly from the
 * input stream does not form a Directed Acyclic Graph (DAG) which is
 * required by the standard tsort algorithms.  Skit deals with this by
 * using a resolving process to convert the dependency graph into a true
 * DAG.  The dependency resolver is based upon the standard tsort
 * algorithm.
 *
 * The basic unit of the dependency graph is th DagNode.  Each dbobject
 * in the source document is initially converted into 1 or 2 DagNodes,
 * one for each action.  If a generate run consists of both build and
 * drop, one DagNode will be created for each of Build and Drop.  In the
 * case of diffs, a diff is converted into 2 DagNodes: DiffPrep and
 * DiffComplete.  For dbobjects that are left unchanges in a diff
 * stream, a single Exists node is created.
 *
 * Each set of optional dependencies is handled by creating a subnode of
 * a DagNode.  Only one dependency (or dependent) of a subnode needs to
 * be satisfied.  If no dependencies are satisfied, a fallback node may
 * be provided.  Typically a fallback node is used when granting
 * privileges on objects.  The dependency-set (set of optional
 * dependencies) in such a case might be on the grantor having been
 * assigned the privilege, or public having been assigned the privilege,
 * or the grantor having been assigned superuser privilege.  If none of
 * these are satisifed the fallback would be to use a superuser to
 * assign the privilege.
 *
 * Cyclic dependencies are handled by allowing cycle-breaker nodes to be
 * added to the graph, these resolve the cyclic dependency by creating
 * an intermediate version of an object that is not dependent on other
 * objects in the cycle.
 *
 * The algorithm for the resolver is as follows:
 * TODO: Rewrite this based upon a successful implementation
 * 
 * fn visit_node(node, from, list)
 *   node->status := VISITING;
 *   for dep = each dependency(node)
 *     begin
 *       resolve_node(dep, dep, list)
 *     exception when cyclic_exception
 *       if is_sub_node(node) then
 *         if dep is last dep of node then
 * 	  if has_fallback then
 * 	    deploy fallback to supernode
 * 	  else
 * 	    raise
 * 	  end if
 * 	end if;
 *       else
 *         if has_breaker(node) then
 * 	  deploy_breaker
 * 	else
 * 	  raise
 * 	end if
 *       end if
 *     end
 *   end loop
 *   node->status := VISITED
 *   if is_sub_node(node) then
 *     record the dep from which we were traversed to
 *   else
 *     append node to list
 *   end if
 * end fn
 * 
 * fn resolve_node(node, from list)
 *   case node->status
 *   VISITED: break
 *   VISITING: 
 *     if is_sub_node(node) then
 *       if there are unvisited dependents then
 *         -- We can continue in the hope that one of the others will apply
 * 	return
 *       else
 *         if there is a fallback then
 * 	  deploy fallback
 * 	else
 * 	  raise cyclic_exception
 * 	end if
 *       end if
 *     else
 *       raise cyclic_exception
 *     end if
 *   UNVISITED: visit_node(node, from, list)
 *   end case
 * end fn
 * 
 * fn resolver(vector)
 *   for node = each node in vector loop
 *     resolve_node(node, null, vector)
 *   end loop
 * 
 *   for node = each node in vector loop
 *     if the node is not VISITED then
 *       raise an exception
 *     end if
 *     if the node is a subnode then
 *       move the selected dependency (or dependent) to the supernode
 *       remove the subnode and associated dependencies and dependents
 *     end if
 *   end loop
 * end fn
 * 
 * 
 */

/* Read the source document, creating a single Dagnode for each
 * dbobject.
 */
/*
static Vector *
dagNodesFromDoc(Document *doc)
{
    Vector *volatile nodes = vectorNew(1000);

    BEGIN {
	(void) xmlTraverse(doc->doc->children, &addNodeToVector, 
			   (Object *) nodes);
    }
    EXCEPTION(ex) {
	objectFree((Object *) nodes, TRUE);
    }
    END;
    return nodes;

}
*/
/* 
 * Create a mirror node for node.  A mirror node is one which is
 * repsonsible for a mirror operation on the node.  In the case of a
 * rebuild operation, the mirror of a build node is a drop node.  For a
 * diff, the operations are prepare and complete. 
 */
/*
static DagNode *
makeMirror(DagNode *node, DagNodeBuildType type)
{
    DagNode *mirror = dagnodeNew(node->dbobject, type);
    node->mirror_node = mirror;
    mirror->mirror_node = node;
    mirror->parent = node->parent;
    addDep(node, mirror, 0);
    return mirror;
}
*/
/*
 * For each DagNode in the nodes Vector, create mirror nodes as needed.
 * Nodes with build_types of REBUILD_NODE become a pair of DROP_NODE and
 * BUILD_NODE, and nodes with build_types of DIFF_NODE become a pair of
 * DIFFCOMPLETE_NODE and DIFFPREP_NODE.
 * When nodes are converted into pairs, an appropriate dependency is
 * added between the nodes.
 */
#ifdef wibble
static Vector *
expandDa3gNodes(Vector *nodes)
{
    Vector *result = vectorNew(nodes->elems * 2);
    DagNode *this;
    DagNode *mirror;
    int i;

    EACH(nodes, i) {
	this = (DagNode *) ELEM(nodes, i);
	switch (this->build_type) {
	case BUILD_NODE:
	case DROP_NODE:
	case EXISTS_NODE:
	    /* Nothing to do - all is well */
	    mirror = NULL;
	    break;
	case DIFF_NODE:
	    this->build_type = DIFFCOMPLETE_NODE;
	    mirror = makeMirror(this, DIFFPREP_NODE);
	    break;
	case REBUILD_NODE:
	    this->build_type = BUILD_NODE;
	    mirror = makeMirror(this, DROP_NODE);
	    break;
	default:
	    objectFree((Object *) result, FALSE);
	    RAISE(TSORT_ERROR, 
		  newstr("Unexpected build_type: %d", this->build_type));
	}
	vectorPush(result, (Object *) this); 
	if (mirror) {
	    vectorPush(result, (Object *) mirror); 
	}
    }

    objectFree((Object *) nodes, FALSE);
    return result;
}
#endif

static Object *
getDepSet(xmlNode *node, boolean inverted, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode;
    Object *depsobj = NULL;
    Object *elem;
    Vector *depsvec = NULL;
    String *restrict = nodeAttribute(node, "restrict");
    String *qn;
    boolean dep_applies;

    if (restrict) {
	dep_applies = (streq(restrict->value, "old") && inverted) ||
	    (streq(restrict->value, "new") && !inverted);
	objectFree((Object *) restrict, TRUE);
    }
    else {
	dep_applies = TRUE;
    }
    
    if (dep_applies) {
	if (isDependencySet(node)) {
	    for (depnode = node->children;
		 depnode = nextDependency(depnode);
		 depnode = depnode->next) 
	    {
		if (depsobj = getDepSet(depnode, inverted, byfqn, bypqn)) {
		    if (depsvec) {
			if (isVector(depsobj)) {
			    vectorAppend(depsvec, (Vector *) depsobj);
			}
			else {
			    vectorPush(depsvec, depsobj);
			}
		    }
		    else {
			if (isVector(depsobj)) {
			    depsvec = (Vector *) depsobj;
			}
			else {
			    depsvec = vectorNew(10);
			    vectorPush(depsvec, depsobj);
			}
		    }
		}
	    }
	    depsobj = (Object *) depsvec;
	}
	else {
	    if (qn = nodeAttribute(node, "fqn")) {
		if (elem = hashGet(byfqn, (Object *) qn)) {
		    depsobj = (Object *) elem;
		}
	    }
	    else if (qn = nodeAttribute(node, "pqn")) {
		if (elem = hashGet(bypqn, (Object *) qn)) {
		    if (((Cons *) elem)->cdr) {
			dbgNode(node);
			dbgSexp(elem);
			RAISE(NOT_IMPLEMENTED_ERROR, 
			      newstr("pqn multiple elements unhandled"));
		    }
		    else {
			depsobj = dereference(((Cons *) elem)->car);
		    }
		}
	    }
	    objectFree((Object *) qn, TRUE);
	}
    }

    return depsobj;
}

#ifdef wibble
static DagNode *
addSubNode2(DagNode *node, boolean invert)
{
    DagNode *subnode;
    char *orig_fqn;
    int subnode_count = 1;
    DagNode *this = node;
    while (this = this->subnodes) {
	subnode_count++;
    }
    subnode= dagnodeNew(node->dbobject, UNSPECIFIED_NODE);
    orig_fqn = subnode->fqn->value;
    subnode->fqn->value = newstr("%s.%s.sub%d", 
				 nameForBuildType(node->build_type), 
				 orig_fqn, subnode_count);
    skfree(orig_fqn);
    subnode->subnodes = node->subnodes;
    subnode->supernode = node;
    node->subnodes = subnode;
    if (invert) {
	addDep(subnode, node, 0);
    }
    else {
	addDep(node, subnode, 0);
    }
    return subnode;
}


static void
addDepsetToNode(
    DagNode *node, 
    Object *depset, 
    boolean invert, 
    boolean is_conditional)
{
    DagNode *subnode;
    DagNode *depnode;
    Object *dep;
    int i;
    boolean dep_is_ignorable;
    if (isVector(depset)) {
	subnode = addSubNode2(node, invert);
	EACH(((Vector *) depset), i) {
	    dep = ELEM(((Vector *) depset), i);
	    addDepsetToNode(subnode, dep, invert, TRUE);
	}
	objectFree(depset, FALSE);
    }
    else {
	dep_is_ignorable = (node->build_type == EXISTS_NODE) ||
	    (((DagNode *) depset)->build_type == EXISTS_NODE);

	if (is_conditional || !dep_is_ignorable) {
	    /* Unconditional dependencies involving an EXISTS_NODE need
	     * not be recorded. */
	    if (invert) {
		depnode = (DagNode *) depset;
		if (depnode->mirror_node) {
		    depnode = depnode->mirror_node;
		}
		addDep(depnode, node, 0);
	    }
	    else {
		addDep(node, (DagNode *) depset, 0);
	    }
	}
    }
}
#endif

/*
static void
addDepsForNode2(DagNode *node, Vector *allnodes, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode;
    boolean invert_deps;
    Object *deps;
    
    assert(node, "addDepsForNode: no node provided");
    assert(node->dbobject, "addDepsForNode: node has no dbobject");

    invert_deps = (node->build_type == DROP_NODE) ||
	(node->build_type == DIFFPREP_NODE);

    for (depnode = node->dbobject->children;
	 depnode = nextDependency(depnode);
	 depnode = depnode->next) 
    {
	if (deps = getDepSet(depnode, invert_deps, byfqn, bypqn)) {
	    addDepsetToNode(node, deps, invert_deps, FALSE);
	}
    }
}
*/
 /*
static boolean
hasUnvisitedDependents(DagNode *node)
{
    DagNode *dep;
    int i;
    if (node->dependents) {
	EACH(node->dependents, i) {
	    dep = (DagNode *) ELEM(node->dependents, i);
	    if (dep->status == UNVISITED) {
		return TRUE;
	    }
	}
    }
    return FALSE;
}
*/
/* Deresolve any elements that were resolved as part of a cyclic
 * dependency loop.  We will re-resolve them later if we can.
 */
#ifdef wibble
static void
resetResolvedList(Vector *resolved, int reset_to)
{
    DagNode *node;
    int i;
    for (i = reset_to; i < resolved->elems; i++) {
	node = (DagNode *) ELEM(resolved, i);
	node->status = UNVISITED;
    }
    resolved->elems = reset_to;
}

static boolean
subnodeHasDependencies(DagNode *node)
{
    Dependency *dep;
    if (node->dependencies->elems > 1) {
	return TRUE;
    }

    /* One dependency.  If it is our parent then this node has no real
     * dependencies and so must represent a set of dependents rather
     * than a set of dependencies (eg this might be on the drop side of
     * the dependency graph).
     */
    dep = (Dependency *) ELEM(node->dependencies, 0);
    return dep->dependency != node->supernode;
}

static boolean
isDependencySubnode(DagNode *node)
{
    return isSubnode(node) && subnodeHasDependencies(node);
}

static boolean
isDependentSubnode(DagNode *node)
{
    return isSubnode(node) && !subnodeHasDependencies(node);
}


#ifdef wibble
static void resolveNode(DagNode *node, DagNode *from, Vector *resolved);

// TODO: I think we can remove the resolved Vector.  There is no need to
// put resolved nodes into a different container as fas as I can see.
static void 
resolveDeps(DagNode *node, DagNode *from, Vector *resolved)
{
    Dependency *dep;
    DagNode *volatile depnode;
    DagNode *volatile subnode;
    int volatile resolved_elems;
    Vector *volatile rescopy = resolved;
    int volatile i;
    boolean volatile in_cycle;
    DagNode *cycle_node;
    DagNode *breaker;
    char *errmsg ;
    char *tmpmsg;

    assert(isVector(resolved), "resolveDeps: resolved is not a vector");

     if (node->dependencies) {
	EACH(node->dependencies, i) {
	    node->dep_idx = i;
	    dep = (Dependency *) ELEM(node->dependencies, i);
	    depnode = dep->dependency;
	    //printSexp(stderr, "Checking: ", (Object *) depnode);
	    //printSexp(stderr, "  from: ", (Object *) node);
	    BEGIN {
		in_cycle = FALSE;
		resolved_elems = resolved->elems;
		resolveNode(depnode, node, resolved);
	    }
	    EXCEPTION(ex);
	    WHEN(TSORT_CYCLIC_DEPENDENCY) {
		in_cycle = TRUE;
		cycle_node = (DagNode *) ex->param;
		errmsg = newstr("%s", ex->text);
	    }
	    END;

	    if (in_cycle) {
		//printSexp(stderr, "Cycling at: ", (Object *) depnode);
		//printSexp(stderr, "  from: ", (Object *) node);
		/* We do this processing outside of the exception
		 * handler as I don't have confidence that the exception
		 * handler can deal properly with exceptions raised 
		 * inside. */
		resetResolvedList(rescopy, resolved_elems);

		if (isDependentSubnode(depnode)) {
		    /* We were traversing to an optional Dependent.
		     * Since that failed, we will remove this
		     * dependency.  We will be ensuring that at least
		     * one dependent of the subnode was satisfied later.
		     */
		    //printSexp(stderr, "ELIMINATING: ", (Object *) depnode);
		    rmDep(node, depnode);
		    skfree(errmsg);
		    i--;
		    continue;
		}

		if (isDependencySubnode(node)) {
		    skfree(errmsg);
		    if (isLastElem(node->dependencies, i)) {
			RAISE(NOT_IMPLEMENTED_ERROR, 
			      newstr("handle fallback"));
		    }
		    else {
			/* We only have to successfully traverse one
			 * dependency, so try the next one. */
			continue;
		    }
		}

		if (breaker = getBreakerFor(depnode)) {
		    /* Replace the current dependency on depnode with
		     * one instead on breaker.  Breaker has been created
		     * with the same dependencies as depnode, so we must
		     * remove from it the dependency that causes the
		     * cycle.  Then we replace the current dependency on
		     * depnode with one instead on breaker and finally
		     * we retry processing this, modified, dependency. */

		    processBreaker2(node, depnode, breaker);
		    i--;
		    skfree(errmsg);
		    continue;
		    RAISE(NOT_IMPLEMENTED_ERROR, 
			  newstr("cycle breaker"));
		    
		}

		/* We were unable to resolve the cyclic dependency.
 		 * Update the errmsg and re-raise it - maybe one of our
 		 * callers will be able to resolve it.
		 */
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
		RAISE(TSORT_CYCLIC_DEPENDENCY, tmpmsg, cycle_node);
	    }
	    else {
		//printSexp(stderr, "Resolved: ", (Object *) depnode);
		//printSexp(stderr, "  from: ", (Object *) node);
	    }

	    if (isSubnode(node)) {
		/* We only have to process one of our dependencies.
		 * The one we have matched is recorded in node->dep_idx */
		break;
	    }
	}
    }    
}

static boolean
findNodeinDepVector(Vector *deps, DagNode *node, int *idx)
{
    Dependency *dep;
    EACH(deps, *idx) {
	dep = (Dependency *) ELEM(deps, *idx);
	if (dep->dependency == node) {
	    return TRUE;
	}
    }
    return FALSE;
}

static void 
resolveNode(
    DagNode *volatile node, 
    DagNode *volatile from, 
    Vector *volatile resolved)
{
    assert(node && node->type == OBJ_DAGNODE, 
	   "resolveNode: node is not a DagNode");

    switch (node->status) {
    case VISITING:
	if (isSubnode(node)) {
	    if (hasUnvisitedDependents(node)) {
		/* We continue in the hope that one of the others
		 * will apply.  If not, we will be back and this
		 * condition will no longer be true. */
		break;
	    }
	    else {
		if (node->fallback) {
		    RAISE(NOT_IMPLEMENTED_ERROR, 
			  newstr("Fallback in resolver"));
		}
	    }
	}
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("%s", node->fqn->value), node);
    case UNVISITED: 
	node->status = VISITING;
	BEGIN {
	    resolveDeps(node, from, resolved);
	}
	EXCEPTION(ex) {
	    node->status = UNVISITED;
	    RAISE();
	}
	END;
	node->status = VISITED;
	if (!node->supernode) {
	    /* Do not record subnodes in the final list. */
	    vectorPush(resolved, (Object *) node);
	}
    case VISITED:
	break;
    default: 
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected build_type: %d", node->build_type));
    }
}

static Vector *
resolveGraph(Vector *nodes)
{
    Vector *volatile resolved = vectorNew(nodes->elems + 10);
    Vector *vtmp;
    DagNode *node;
    DagNode *subnode;
    Dependency *dep;
    int i, j;

    BEGIN {
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    resolveNode(node, NULL, resolved);
	}

	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    if (node->status != VISITED) {
		RAISE(TSORT_ERROR, 
		      newstr("Node %s is unresolved in resolveGraph", 
			     node->fqn->value));
	    }
	    subnode = node->subnodes; 
	    while (subnode) {
		/* Check each subnode, identifying the selected dependency
		 * for it, and transferring that dependency to the
		 * supernode. */
		
		if (subnodeHasDependencies(subnode)) {
		    dep = (Dependency *) ELEM(subnode->dependencies, 
					      subnode->dep_idx);
		    rmDep(node, subnode);
		    rmDep(subnode, dep->dependency);
		    addDependency(node, dep->dependency, 0);
		}
		else {
		    /* We need to remove all of the dependencies to this
		     * optional node, and replace with the resolved single
		     * dependency to the supernode. */
		    vtmp = vectorCopy(subnode->dependents);
		    EACH(vtmp, j) {
			dep = (Dependency *) ELEM(vtmp, j);
			if (j == subnode->dep_idx) {
			    addDependency(dep->dependency, node, 0);
			}
			rmDep(dep->dependency, subnode);
		    }
		    rmDep(subnode, node);
		    objectFree((Object *) vtmp, FALSE);
		}
		subnode = subnode->subnodes;
	    }
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) resolved, FALSE);
	RAISE();
    }
    END;

    return resolved;
}
#endif

Vector *
dagFromDoc(Document *doc)
{
    Vector *volatile nodes = dagNodesFromDoc(doc);
    Vector *volatile resolved_nodes = NULL;
    Hash *volatile byfqn = hashByFqn(nodes);
    Hash *volatile bypqn = NULL;
    DagNode *volatile this = NULL;
    int volatile i;
    BEGIN {
	bypqn = hashByPqn(nodes);
	identifyParents(nodes, byfqn);
	nodes = expandDagNodes(nodes);
	EACH(nodes, i) {
	    this = (DagNode *) ELEM(nodes, i);
	    assert(this->type == OBJ_DAGNODE, "incorrect node type");
	    addDepsForNode2(this, nodes, byfqn, bypqn);
	}
	//showVectorDeps(nodes);
	//fprintf(stderr, "==========================\n");
	resolved_nodes = resolveGraph(nodes);
	//showVectorDeps(resolved_nodes);
    }
    EXCEPTION(ex) {
	objectFree((Object *) nodes, TRUE);
    }
    FINALLY {
	objectFree((Object *) byfqn, FALSE);
	objectFree((Object *) bypqn, TRUE);
    }
    END;
    objectFree((Object *) nodes, FALSE);
    return resolved_nodes;
}
#endif

/*

 COMMENT NEEDED HERE DESCRIBING THE LATEST ALGORITHM FOR GENERATING A
 DAG FROM OUR SOMEWHAT UNSTRUCTURED SET OF DEPENDENCIES

*/







void
showDeps2(DogNode *node)
{
    DogNode *sub;
    if (node) {
	printSexp(stderr, "NODE: ", (Object *) node);
	printSexp(stderr, "-->", (Object *) node->forward_deps);
	sub = node->forward_subnodes;
	while (sub) {
	    if (sub->forward_deps || sub->fallback_node) {
		printSexp(stderr, "optional->", (Object *) sub->forward_deps);
		if (sub->fallback_node) {
		    printSexp(stderr, "flbk->", (Object *) sub->fallback_node);
		}
	    }
	    sub = sub->forward_subnodes;
	}
	printSexp(stderr, "<==", (Object *) node->backward_deps);
	sub = node->backward_subnodes;
	while (sub) {
	    if (sub->backward_deps || sub->fallback_node) {
		printSexp(stderr, "optional<=", (Object *) sub->backward_deps);
		if (sub->fallback_node) {
		    printSexp(stderr, "flbk<=", (Object *) sub->fallback_node);
		}
	    }
	    sub = sub->backward_subnodes;
	}
    }
}

/*
 * Identify the type of build action expected for the supplied dbobject
 * node.
 */
static DagNodeBuildType
buildTypeForDogNode(Node *node)
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
	    errmsg = newstr("buildTypeForDogNode: unexpected diff "
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
	    errmsg = newstr("buildTypeForDogNode: unexpected action "
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
		errmsg = newstr("buildTypeForDogNode: cannot identify "
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
addDogNodeToVector(Object *this, Object *vector)
{
    Node *node = (Node *) this;
    DagNodeBuildType build_type;
    DogNode *dognode;

    if (streq(node->node->name, "dbobject")) {
	dognode = dognodeNew(node->node, UNSPECIFIED_NODE);
	dognode->build_type = buildTypeForDogNode(node);
	vectorPush((Vector *) vector, (Object *) dognode);
    }

    return NULL;
}

/* Read the source document, creating a single Dognode for each
 * dbobject.
 */
static Vector *
dogNodesFromDoc(Document *doc)
{
    Vector *volatile nodes = vectorNew(1000);

    BEGIN {
	(void) xmlTraverse(doc->doc->children, &addDogNodeToVector, 
			   (Object *) nodes);
    }
    EXCEPTION(ex) {
	objectFree((Object *) nodes, TRUE);
    }
    END;
    return nodes;

}

/* Return a Hash of DogNodes keyed by fqn.
 */
Hash *
hashByFqn2(Vector *vector)
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

	if (old = hashAdd(hash, (Object *) key, (Object *) node)) {
	    RAISE(GENERAL_ERROR, 
		  newstr("hashbyFqn: duplicate node \"%s\"", key->value));
	}
    }
    //printSexp(stderr, "FQN HASH: ", (Object *) hash);
    return hash;
}

/* Return a Hash of lists of DogNodes keyed by pqn.
 */
static Hash *
hashByPqn2(Vector *vector)
{
    int i;
    DogNode *node;
    String *key;
    Cons *entry ;
    Hash *hash = hashNew(TRUE);
    Object *new;
    xmlChar *pqn;

    EACH(vector, i) {
	node = (DogNode *) ELEM(vector, i);
	assert(node->type == OBJ_DOGNODE, "Incorrect node type");
	pqn = xmlGetProp(node->dbobject, "pqn");
	if (pqn) {
	    key = stringNew(pqn);
	    xmlFree(pqn);
	    new = (Object *) objRefNew((Object *) node);
	    if (entry = (Cons *) hashGet(hash, (Object *) key)) {
       RAISE(NOT_IMPLEMENTED_ERROR, 
			  newstr("CRAP"));
		consAppend(entry, new);
	    }
	    else {
		entry = consNew(new, NULL);
		hashAdd(hash, (Object *) key, (Object *) entry);
	    }
	}
    }
    return hash;
}
/*
 * For each DagNode in the nodes Vector, identify the parent node and
 * record it in the child node's parent field.
 */
static void
identifyParents2(Vector *nodes, Hash *nodes_by_fqn)
{
    int i;
    DogNode *node;
    xmlNode *parent;
    String *parent_fqn;

    EACH(nodes, i) {
	node = (DogNode *) ELEM(nodes, i);
	assert(node->type == OBJ_DOGNODE, "incorrect node type");
	assert(node->dbobject, "identifyParents: no dbobject node");
	parent = findAncestor(node->dbobject, "dbobject");
	parent_fqn  = nodeAttribute(parent, "fqn");
	if (parent_fqn) {
	    node->parent = (DogNode *) hashGet(nodes_by_fqn, 
					       (Object *) parent_fqn);
	    assert(node->parent, 
		   "identifyParents2: parent of %s (%s) not found",
		   node->fqn->value, parent_fqn->value);
	    objectFree((Object *) parent_fqn, TRUE);
	}
    }
}

static DogNode *
getFallbackNode2(xmlNode *dep_node, Hash *nodes_by_fqn)
{
    String *fallback;
    DogNode *fallback_node = NULL;
    char *errmsg;
    if (isDependencySet(dep_node)) {
	fallback = nodeAttribute(dep_node, "fallback");
	if (fallback) {
	    fallback_node = (DogNode *) hashGet(nodes_by_fqn, 
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

/* 
 * Identify to which sides of the DAG, the current dependency or
 * dependency-set applies.
 */
static DependencyApplication 
applicationForDep(xmlNode *node)
{
    String *condition_str = nodeAttribute(node, "condition");
    String separators = {OBJ_STRING, " ()"};
    Cons *contents;
    Cons *elem;
    char *head;
    DependencyApplication result;
    boolean inverted = FALSE;

    if (condition_str) {
	stringLowerOld(condition_str);
	elem = contents = stringSplit(condition_str, &separators);
	while (elem) {
	    head = ((String *) elem->car)->value;
	    if (streq(head, "build")) {
		result = inverted? BACKWARDS: FORWARDS;
	    }
	    else if (streq(head, "drop")) {
		result = inverted? FORWARDS: BACKWARDS;
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
	return result;
    }
    return BOTH;
}

/* 
 * Append dependencies, returning a vector if there are multiple
 * dependencies, otherwise returning a single DagNode.
 */
static Object *
appendDep(Object *cur, Object *new)
{
    Vector *vec;
    if (cur) {
	if (new) {
	    if (cur->type != OBJ_VECTOR) {
		vec = vectorNew(10);
		vectorPush(vec, cur);
		cur = (Object *) vec;
	    }

	    if (new->type == OBJ_VECTOR) {
		vectorAppend((Vector *) cur, (Vector *) new);
	    }
	    else {
		vectorPush((Vector *) cur, new);
	    }
	}
	return cur;
    }
    return new;
}
    
/* 
 *
 * TODO: rename to getDepSet after refactoring away the old version
 */
static Object *
getDepSets(xmlNode *node, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode;
    Object *dep;
    Cons *cons;
    Object *result = NULL;
    String *qn = NULL;

    if (isDependencySet(node)) {
	for (depnode = node->children;
	     depnode = nextDependency(depnode);
	     depnode = depnode->next) 
	{
	    dep = getDepSets(depnode, byfqn, bypqn);
	    if (dep) {
		if ((dep->type == OBJ_DOGNODE) &&
		    (((DogNode *) dep)->build_type == EXISTS_NODE)) {
		    /* This dependency set is automatically satisfied.
		     * This means we do not need to return a set but
		     * just this simple dependency.  So we clean up and
		     * exit.
		     */
		    objectFree(result, FALSE);
		    return dep;
		}
		else {
		    result = appendDep(result, dep);
		}
	    }
	}
    }
    else {
	if (qn = nodeAttribute(node, "fqn")) {
	    dep = hashGet(byfqn, (Object *) qn);
	    result = appendDep(result, dep);
	}
	else if (qn = nodeAttribute(node, "pqn")) {
	    if (cons = (Cons *) hashGet(bypqn, (Object *) qn)) {
		assert(cons->type == OBJ_CONS, "DEP BY PQN IS NOT A CONS CELL");
		if (!cons->cdr) {
		    /* List has only a single element */
		    dep = dereference(cons->car);
		}
		else {
		    dep = (Object *) toVector(cons);
		}
		result = appendDep(result, dep);
	    }
	}
	objectFree((Object *) qn, TRUE);
    }
    return result;
}

static void
addDepToVector(Vector **p_vector, DogNode *dep)
{
    if (!*p_vector) {
	*p_vector = vectorNew(10);
    }
    setPush(*p_vector, (Object *) dep);
}

/* Add a dependency to a DogNode in whatever directions are appropriate.
 *
 */
static void
addDep2(DogNode *node, DogNode *dep, DependencyApplication applies)
{
    if ((applies == FORWARDS) || (applies == BOTH)) {
	addDepToVector(&(node->forward_deps), dep);
    }
    if ((applies == BACKWARDS) || (applies == BOTH)) {
	addDepToVector(&(node->backward_deps), dep);
    }
}


static DogNode *
newSubNode(DogNode *node, DependencyApplication applies)
{
    DogNode *new = dognodeNew(node->dbobject, OPTIONAL_NODE);
    new->supernode = node;
    if ((applies == FORWARDS) || (applies == BOTH)) {
	new->forward_subnodes = node->forward_subnodes;
	node->forward_subnodes = new;
    }
    if ((applies == BACKWARDS) || (applies == BOTH)) {
	new->backward_subnodes = node->backward_subnodes;
	node->backward_subnodes = new;
    }
    return new;
}

static DogNode *
makeBreakerNode2(DogNode *from_node, String *breaker_type)
{
    xmlNode *dbobject = from_node->dbobject;
    xmlChar *old_fqn = xmlGetProp(dbobject, "fqn");
    char *fqn_suffix = strstr(old_fqn, ".");
    char *new_fqn = newstr("%s%s", breaker_type->value, fqn_suffix);
    xmlNode *breaker_dbobject = xmlCopyNode(dbobject, 1);
    DogNode *breaker;
    DogNode *dep;
    Vector *deps;
    int i;
    boolean forwards;
    xmlSetProp(breaker_dbobject, "type", breaker_type->value);
    xmlUnsetProp(breaker_dbobject, "cycle_breaker");
    xmlSetProp(breaker_dbobject, "fqn", new_fqn);
    xmlFree(old_fqn);
    skfree(new_fqn);

    breaker = dognodeNew(breaker_dbobject, from_node->build_type);
    breaker->parent = from_node->parent;
    breaker->breaker_for = from_node;

    /* Copy dependencies of from_node to breaker.  We will eliminate
     * unwanted ones later. */
    
    for (forwards = 0; forwards <= 1; forwards++) {
	deps = forwards? from_node->forward_deps: from_node->backward_deps;
	EACH (deps, i) {
	    dep = (DogNode *) ELEM(deps, i);
	    if (forwards) {
		addDepToVector(&(breaker->forward_deps), dep);
	    }
	    else {
		addDepToVector(&(breaker->backward_deps), dep);
	    }
	}
    }
    return breaker;
}

static DogNode *
getBreakerFor2(DogNode *node, Vector *nodes)
{
    String *breaker_type;

    if (!node->breaker) {
	if (breaker_type = nodeAttribute(node->dbobject, "cycle_breaker")) {
	    node->breaker = makeBreakerNode2(node, breaker_type);
	    objectFree((Object *) breaker_type, TRUE);
	    setPush(nodes, (Object *) node->breaker);
	}
    }
    return node->breaker;
}

/* Given a cycle breaking node (breaker), the node for which it is a
 * replacement (depnode), and the node that has a dependency on it
 * (curnode), update the dependency graph to break the current
 * dependency cycle.
 */
static void 
processBreaker3(
    DogNode *curnode, 
    DogNode *depnode, 
    DogNode *breaker,
    boolean forwards)
{
    Vector *deps;
    DogNode *thru = (DogNode *) ELEM(depnode->forward_deps, depnode->dep_idx);
    DogNode *this;
    Vector *newdeps;
    int i;

    /* Eliminiate from breaker any dependency on thru */
    deps = forwards? breaker->forward_deps: breaker->backward_deps;
    this = (DogNode *) vectorDel(deps, (Object *) thru);
    if (this != thru) {
	RAISE(TSORT_ERROR, 
	      newstr("Unable to eliminate dependency %s -> %s",
		     breaker->fqn->value, this->fqn->value));
    }

    /* Replace the dependency from curnode to depnode with one from
     * curnode to breaker. */
    deps = forwards? curnode->forward_deps: curnode->backward_deps;
    EACH(deps, i) {
	this = (DogNode *) ELEM(deps, i);
	if (this == depnode) {
	    ELEM(deps, i) = (Object *) breaker;
	}
    }
}



/* Add both forward and back dependencies to a DogNode from the source
 * xml objects.
 */
static void
addDepsForNode2(DogNode *node, Vector *allnodes, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode;
    Object *deps;
    DogNode *fallback_node;
    DogNode *sub;
    DependencyApplication applies;
    Object *tmp;

    assert(node, "addDepsForNode: no node provided");
    assert(node->dbobject, "addDepsForNode: node has no dbobject");

    for (depnode = node->dbobject->children;
	 depnode = nextDependency(depnode);
	 depnode = depnode->next) 
    {
	/* depnode is either a dependency or a dependency-set node and
	 * so there may be a single dependency or a set of dependencies
	 * for this depnode.  If there is a set, only one of the
	 * dependencies from the set needs to be resolved.  We deal with
	 * that by creating a subnode for dependency sets.
	 */
	applies = applicationForDep(depnode);
	fallback_node = getFallbackNode2(depnode, byfqn);
	deps = getDepSets(depnode, byfqn, bypqn);

	if (deps && (deps->type == OBJ_DOGNODE) && 
	    (((DogNode *) deps)->build_type == EXISTS_NODE))
	{
	    /* There is no need to record a dependency against this dep,
	     * as it is an EXISTS_NODE.
	     */
	    continue;
	}

	if (fallback_node || (deps && (deps->type == OBJ_VECTOR))) {

	    if ((!deps) || ((deps->type != OBJ_VECTOR))) {
		tmp = deps;
		deps = (Object *) vectorNew(10);
		if (tmp) {
		    setPush((Vector *) deps, tmp);
		}
	    }
	    if ((applies == FORWARDS) || (applies == BOTH)) {

		// No failure before here
		sub = newSubNode(node, FORWARDS);
		sub->forward_deps = (Vector *) deps;
		sub->fallback_node = fallback_node;
	    }

	    if ((applies == BACKWARDS) || (applies == BOTH)) {
		sub = newSubNode(node, BACKWARDS);
		if (deps && (applies == BOTH)) {
		    sub->backward_deps = vectorCopy((Vector *) deps);
		}
		else {
		    sub->backward_deps = (Vector *) deps;
		}
		sub->fallback_node = fallback_node;
	    }
	}
	else if (deps) {
	    addDep2(node, (DogNode *) deps, applies);
	}
    }
    //showDeps2(node);
}

/* Determine the declared set of dependencies for the nodes, creating
 * both forward and backward dependencies.
 */
static void
addDependencies(Vector *nodes, Hash *byfqn, Hash *bypqn)
{
   DogNode *volatile this = NULL;
   int volatile i;
   EACH(nodes, i) {
       this = (DogNode *) ELEM(nodes, i);
       assert(this->type == OBJ_DOGNODE, "incorrect node type");
       addDepsForNode2(this, nodes, byfqn, bypqn);
   }
}

static void
resolveNode(DogNode *node, DogNode *from, boolean forwards, Vector *nodes);

static void
resolveDeps(DogNode *node, DogNode *from, boolean forwards, Vector *nodes)
{
    Vector *volatile deps = forwards? node->forward_deps: node->backward_deps;
    int volatile i;
    boolean volatile cyclic_exception;
    DogNode *volatile dep;
    DogNode *breaker;
    DogNode *cycle_node;
    char *tmpmsg;
    char *errmsg;

    if (deps) {
	EACH(deps, i) {
	    dep = (DogNode *) ELEM(deps, i);
	    node->dep_idx = i;
	    BEGIN {
		cyclic_exception = FALSE;
		resolveNode(dep, node, forwards, nodes);
	    }
	    EXCEPTION(ex);
	    WHEN(TSORT_CYCLIC_DEPENDENCY) {
		cyclic_exception = TRUE;
		cycle_node = (DogNode *) ex->param;
		errmsg = newstr("%s", ex->text);
	    }
	    END;
	    if (cyclic_exception) {
		//printSexp(stderr, "Cycling at: ", (Object *) dep);
		//printSexp(stderr, "  from: ", (Object *) node);

		if (node->supernode) {
		    /* We are in a subnode, so this is an optional
		     * dependency.  Unless this is the last dependency
		     * in the list, we can simply try the next one. */
		    skfree(errmsg);
		    if (isLastElem(deps, i)) {
			RAISE(NOT_IMPLEMENTED_ERROR, 
			      newstr("Unimplemented: fallback handling"));
		    }
		    else {
			continue;
		    }
		}
		
		if (breaker = getBreakerFor2(dep, nodes)) {
		    /* Replace the current dependency on dep with
		     * one instead on breaker.  Breaker has been created
		     * with the same dependencies as depnode, so we must
		     * remove from it the dependency that causes the
		     * cycle.  Then we replace the current dependency on
		     * depnode with one instead on breaker and finally
		     * we retry processing this, modified, dependency. */
		    
		    skfree(errmsg);
		    processBreaker3(node, dep, breaker, forwards);
		    i--;
		    continue;
		}

		/* We were unable to resolve the cyclic dependency in
 		 * this node.  Update the errmsg and re-raise it - maybe
 		 * one of our callers will be able to resolve it.
		 */
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
		node->dep_idx = -1;
		RAISE(TSORT_CYCLIC_DEPENDENCY, tmpmsg, cycle_node);
	    }
	    //else {
	    //	printSexp(stderr, "Resolved: ", (Object *) dep);
	    //	printSexp(stderr, "  from: ", (Object *) node);
	    //}

	    if (node->supernode) {
		/* We are in a subnode and have successfully traversed a
		 * dependency.  Since we only need one dependency from
		 * our set, we are done. */
		break;
	    }
	}
    }
}

static void
resolveSubNodes2(
    DogNode *node, 
    DogNode *from, 
    boolean forwards,
    Vector *nodes)
{
    DogNode *sub = node;
    Vector *deps;
    DogNode *dep;
    while (sub = forwards? sub->forward_subnodes: sub->backward_subnodes) {
	resolveNode(sub, from, forwards, nodes);
    }
}

static void
resolveNode(DogNode *node, DogNode *from, boolean forwards, Vector *nodes)
{
    assert(node && node->type == OBJ_DOGNODE, 
	   "resolveNode: node is not a DogNode");
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("%s", node->fqn->value), node);
    case RESOLVED_F:
	if (forwards) {
	    break;
	}
    case UNVISITED: 
	node->status = VISITING;
	BEGIN {
	    resolveDeps(node, from, forwards, nodes);
	    resolveSubNodes2(node, from, forwards, nodes);
	}
	EXCEPTION(ex) {
	    node->status = UNVISITED;
	    RAISE();
	}
	END;
	node->status = forwards? RESOLVED_F: RESOLVED;
	break;
    case RESOLVED:
	if (!forwards) {
	    break;
	}
    default: 
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected node status: %d", node->status));
    }
}

static void
activateFallback2(DogNode *node, DogNode *fallback, Vector *nodes)
{
    DogNode *closer;
    DogNode *dep;
    int i;
    if (fallback->build_type != FALLBACK_NODE) {
	/* Promote this node from an exists node into a build node, and
	 * create the matching drop node for it. */
	fallback->build_type = FALLBACK_NODE;
	closer = dognodeNew(fallback->dbobject, ENDFALLBACK_NODE);
	fallback->mirror_node = closer;
	setPush(nodes, (Object *) closer);

	/* Copy deps from build node to drop node.  For fallback nodes,
	 * all dependendencies wodk in the forward direction as we are
	 * just going to grant a privilege and then revoke it and the
	 * role to which the grant and revoke is applied should be the
	 * only initial dependency. */
	EACH(fallback->forward_deps, i) {
	    dep = (DogNode *) ELEM(fallback->forward_deps, i);
	    addDepToVector(&(closer->forward_deps), dep);
	}
    }
    else {
	closer = fallback->mirror_node;
    }

    /* Add dependencies from closer to node, and from node to fallback. */
    addDepToVector(&(closer->forward_deps), node);
    addDepToVector(&(node->forward_deps), fallback);
}

/* 
 *
 */
static void
resolveGraphs(Vector *nodes)
{
    DogNode *node;
    DogNode *sub;
    DogNode *dep;
    int i;

    EACH(nodes, i) {
	node = (DogNode *) ELEM(nodes, i);
	resolveNode(node, NULL, TRUE, nodes);
    }

    EACH(nodes, i) {
	node = (DogNode *) ELEM(nodes, i);
	resolveNode(node, NULL, FALSE, nodes);
    }

    EACH(nodes, i) {
	node = (DogNode *) ELEM(nodes, i);
	sub = node;
	while (sub = sub->forward_subnodes) {
	    if (sub->dep_idx < 0) {
		activateFallback2(node, sub->fallback_node, nodes);
	    }
	    else {
		dep = (DogNode *) ELEM(sub->forward_deps, sub->dep_idx);
		addDepToVector(&(node->forward_deps), dep);
	    }
	    //printSexp(stderr, "Promoting :", (Object *) dep);
	    //printSexp(stderr, " in :", (Object *) node);
	}

	sub = node;
	while (sub = sub->backward_subnodes) {
	    //dbgSexp(sub);
	    //dbgSexp(sub->forward_subnodes);
	    //dbgSexp(sub->backward_subnodes);
	    if (sub->dep_idx < 0) {
		activateFallback2(node, sub->fallback_node, nodes);
	    }
	    else {
		dep = (DogNode *) ELEM(sub->backward_deps, sub->dep_idx);
		addDepToVector(&(node->backward_deps), dep);
	    }
	    //printSexp(stderr, "Promoting :", (Object *) dep);
	    //printSexp(stderr, " in :", (Object *) node);
	}
    }
}

static DogNode *
makeMirror2(DogNode *node, DagNodeBuildType type)
{
    DogNode *mirror = dognodeNew(node->dbobject, type);
    node->mirror_node = mirror;
    mirror->mirror_node = node;
    mirror->parent = node->parent;
    addDepToVector(&(node->forward_deps), mirror);
    return mirror;
}

static void
expandDogNodes(Vector *nodes)
{
    int i;
    DogNode *node;
    DogNode *mirror;
    
    EACH(nodes, i) {
	node = (DogNode *) ELEM(nodes, i);

	switch (node->build_type) {
	case BUILD_NODE:
	case DROP_NODE:
	case EXISTS_NODE:
	case FALLBACK_NODE:
	case ENDFALLBACK_NODE:
	    /* Nothing to do - all is well */
	    mirror = NULL;
	    break;
	case DIFF_NODE:
	    node->build_type = DIFFCOMPLETE_NODE;
	    mirror = makeMirror2(node, DIFFPREP_NODE);
	    break;
	case REBUILD_NODE:
	    node->build_type = BUILD_NODE;
	    mirror = makeMirror2(node, DROP_NODE);
	    break;
	default:
	    RAISE(TSORT_ERROR, 
		  newstr("Unexpected build_type: %d", node->build_type));
	}
	if (mirror) {
	    setPush(nodes, (Object *) mirror); 
	}
    }
}

static void
clearUnneededDeps(Vector *nodes)
{
    int i;
    DogNode *node;
    
    EACH(nodes, i) {
	node = (DogNode *) ELEM(nodes, i);
	if ((node->build_type == DIFFPREP_NODE) ||
	    (node->build_type == DROP_NODE))
	{
	    /* This node is on the backwards side of the dependency
	     * graph.  This means that its forward_deps must be derived
	     * from other nodes backward_deps.  Before we can do that,
	     * if the node already has forward_deps, they must be
	     * cleared out. */ 
	    if (node->forward_deps) {
		objectFree((Object *) node->forward_deps, FALSE);
		node->forward_deps = NULL;
	    }
	}
    }
}

static void
redirectBackwardDeps(Vector *nodes)
{
    int i;
    int j;
    DogNode *node;
    DogNode *depnode;
    Vector *deps;

    clearUnneededDeps(nodes);

    EACH(nodes, i) {
	node = (DogNode *) ELEM(nodes, i);

	if ((node->build_type == DIFFPREP_NODE) ||
	    (node->build_type == DROP_NODE))
	{
	    /* Derive other nodes' forward_deps from this node (or its
	     * mirror's backward_deps. */
	    if (!(deps = node->backward_deps)) {
		if (node->mirror_node) {
		    deps = node->mirror_node->backward_deps;
		}
	    }
	    if (deps) {
		EACH(deps, j) {
		    depnode = (DogNode *) ELEM(deps, j);
		    if (depnode->mirror_node) {
			depnode = depnode->mirror_node;
		    }
		    addDepToVector(&(depnode->forward_deps), node);
		}
	    }
	}
    }
}

/* Create a Dag from the supplied doc, returning it as a vector of DocNodes.
 *
 */
Vector *
dagFromDoc(Document *doc)
{
   Vector *volatile nodes = dogNodesFromDoc(doc);
   Hash *volatile byfqn = hashByFqn2(nodes);
   Hash *volatile bypqn = NULL;
   DogNode *volatile this = NULL;
   int volatile i;
   
   BEGIN {
       identifyParents2(nodes, byfqn);
       bypqn = hashByPqn2(nodes);
       addDependencies(nodes, byfqn, bypqn);
       resolveGraphs(nodes);
       expandDogNodes(nodes);
       redirectBackwardDeps(nodes);
   }
   EXCEPTION(ex) {
       objectFree((Object *) nodes, TRUE);
       objectFree((Object *) bypqn, TRUE);
       objectFree((Object *) byfqn, FALSE);
   }
   END;
   objectFree((Object *) bypqn, TRUE);
   objectFree((Object *) byfqn, FALSE);

   return nodes;
}
