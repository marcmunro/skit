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
 * Optional dependencies must be handled first.  We do this by creating
 * a special node (called an Xnode) for each dependency-set.  When we
 * traverse such nodes (using resolving_tsort()) only one of the
 * dependencies has to be satisfied.  If a cyclic dependency is found
 * while we are resolving an optional dependency, we will retry the
 * tsort with the next option.  Satisfying optional dependencies after
 * we have duplicated nodes (for drop and build, etc) is very difficult
 * as, the graph when reversed effectively gives us optional dependents
 * rather than optional dependencies and the standard tsort algorithms
 * give us no easy way to resolve such things.  One approach that was
 * considered and tried (and appears to work) is as follows:
 *
 * Given optional dependencies:
 *   A-->B or C
 *   B-->C or D
 *
 * We add Xnodes:
 *    A --> XA
 *   XA --> B
 *   XA --> C
 *    B --> XB
 *   XB --> C
 *   XB --> D
 *
 * We then add optional inverted dependencies (represented below as
 * ..>): 
 *    A --> XA
 *   XA --> B
 *   XA --> C
 *    B ..> A
 *    B --> XB
 *   XB --> C
 *   XB --> D
 *    C ..> A
 *    C ..> B
 *    D ..> C
 * 
 * To process this, our rule is that dependencies on Xnodes are
 * traversed first and optional dependencies are tried but may be
 * abandoned if a cyclic dependency is encountered.  It is not obvious
 * that this is a guranteed complete solution (in fact, I suspect it is
 * not) but it is the best I came up with.  
 * 
 * In light of this complexity, I chose to deal with dependency inversion
 * of optional dependencies by fixing those optional dependencies in
 * the forward direction first. That is, by identifying a path through
 * the graph involving only one of each of the optional dependencies in
 * the forward direction.  Once an appropriate set of options has been
 * identified in the forward (build) direction (eliminating the
 * optionality), the graph can be easily inverted for the drop
 * direction. 
 *
 * At the same time as handling optional dependencies, we also try to
 * deal with cyclic dependencies.  In some cases cyclic dependencies are
 * allowable, if the objects are built in stages.  Eg if A depends on B
 * and B depends on A, we can build a minimal A, then build Bm and then
 * build the full A.  For this to work, nodes have to be defined with
 * specific cycle-breaking mechanisms as part of their dbobject
 * definitions.  This is handled by the adddeps.xsl transform.
 * 
 * So, a cycle that involves a node that has a cycle breaker will result
 * in the cycle breaker being inserted into the graph, with all members
 * of the cycle depending on it.
 * 
 * Eg, given:
 *   A --> B
 *   B --> C
 *   C --> A
 *
 * We would pick one of the nodes in the cycle (A) and add a cycle
 * breaker (Ab) as follows:
 *
 *  A --> Ab
 *  A --> B
 *  B --> Ab
 *  B --> C
 *  C --> Ab
 * 
 * Ie, the dependency from C to A has been replaced with one from C to
 * Ab.  The other dependencies to Ab may be, strictly speaking,
 * unnecessary but they do no harm.  Inversion of this is slightly
 * tricky as we still want to use Ab as the breaker node but the
 * dependencies between the other nodes must still be inverted.  For
 * details on this take a look at the code comments for the specific
 * functions.
 *
 * I have a niggling suspicion that the inversion of cycle breakers
 * could, in pathological cases, fail.  I suspect that the correct way
 * of dealing with this is to defer the handling of cycle-breakers until
 * after we have duplicated and redirected the graph (for nodes that
 * have both build and drop actions).  However that would be a major
 * change to deal with an unlikely and currently only theoretical
 * issue.
 *
 * Node duplication, allowing for both build and drop actions within the
 * same DAG is performed by duplicateRebuildNodes().  This is then
 * followed by redirectDependencies() which inverts the initially
 * provided dependencies so that they reflect the actions being
 * performed at each node.
 */


/* Problems/issues to fix:
 * 1) Need to be able to add optional deps on owner being a superuser.
 *    This means that privs assigned to roles must be considered to be
 *    objects in their own right so that we can depend on them.
 * 2) Need the ability to have a fall-back position for depsets when
 *    no dep is found.  Using the fallback would result in something
 *    like switching the owner into superuser mode (grant superuser to
 *    owner, then do operation, then revoke).
 * 3) For inverted deps we need to be able to determine whether any of
 *    the inverted deps were satisfied and, if not, use the fall-back
 *    option. 
 * Test cases:
 * 1)  create objects owned by X as a superuser with no other rights.
 * 2 and 3) As above, then remove all rights from X.
 */

#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"



void
showDeps(DagNode *node)
{
    if (node) {
	printSexp(stderr, "NODE: ", (Object *) node);
	printSexp(stderr, "-->", (Object *) node->dependencies);
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

#ifdef wibble
/* Ensure that node has dep as a dependent */
static void 
checkDependent(DagNode *node, DagNode *dep)
{
    int i;
    DagNode *this;
    if (node->dependents) {
	EACH(node->dependents, i) {
	    this = (DagNode *) ELEM(node->dependents, i);
	    if (this == dep) {
		return;
	    }
	}
    }
    RAISE(TSORT_ERROR, 
	  newstr("Node %s is missing %s as a dependent.\n",
		 node->fqn->value, dep->fqn->value));
}

/* Ensure that node has dep as a dependency */
static void 
checkDependency(DagNode *node, DagNode *dep)
{
    int i;
    DagNode *this;
    if (node->dependencies) {
	EACH(node->dependencies, i) {
	    this = (DagNode *) ELEM(node->dependencies, i);
	    if (this == dep) {
		return;
	    }
	}
    }
    RAISE(TSORT_ERROR, 
	  newstr("Node %s is missing %s as a dependency.\n",
		 node->fqn->value, dep->fqn->value));
}

static void
checkDeps(DagNode *node)
{
    int i;
    DagNode *dep;
    if (node) {
	if (node->dependencies) {
	    EACH(node->dependencies, i) {
		dep = (DagNode *) ELEM(node->dependencies, i);
		checkDependent(dep, node);
	    }
	}
	if (node->dependents) {
	    EACH(node->dependents, i) {
		dep = (DagNode *) ELEM(node->dependents, i);
		checkDependency(dep, node);
	    }
	}
    }
}

void
checkVectorDeps(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	checkDeps(node);
    }
}
#endif


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
addDependency(DagNode *node, DagNode *dep)
{
    Vector *node_deps = makeVector(node->dependencies);
    setPush(node_deps, (Object *) dep);
}

void
addDependent(DagNode *node, DagNode *dep)
{
    Vector *node_deps = makeVector(node->dependents);
    setPush(node_deps, (Object *) dep);
}

void
addDep(DagNode *node, DagNode *dep)
{
    addDependency(node, dep);
    addDependent(dep, node);
}

static void
rmDep(DagNode *node, DagNode *dep)
{
    if (dep->dependents) {
	setDel(dep->dependents, (Object *) node);
    }
    if (node->dependencies) {
	setDel(node->dependencies, (Object *) dep);
    }
}

static void
addDepsVector(DagNode *node, Vector *deps)
{
    int i;
    EACH(deps, i) {
	addDep(node, (DagNode *) dereference(ELEM(deps, i)));
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

static Vector *
explicitDepsForNode(xmlNode *node, Hash *nodes_by_fqn, Hash *nodes_by_pqn)
{
    Vector *vector = NULL;
    Vector *next;
    Object *elem;
    xmlNode *dep_node;
    String *fqn;
    String *pqn;

    if (isDependencySet(node)) {
	for (dep_node = node->children;
	     dep_node = nextDependency(dep_node);
	     dep_node = dep_node->next) 
	{
	    if (next = explicitDepsForNode(dep_node, nodes_by_fqn, 
					    nodes_by_pqn)) {
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
	dep_node = node;
	if (fqn = nodeAttribute(dep_node, "fqn")) {
	    if (elem = hashGet(nodes_by_fqn, (Object *) fqn)) {
		vector = vectorNew(10);
		vectorPush(vector, elem);
	    }
	    objectFree((Object *) fqn, TRUE);
	}
	else if (pqn = nodeAttribute(dep_node, "pqn")) {
	    if (elem = hashGet(nodes_by_pqn, (Object *) pqn)) {
		vector = cons2Vector((Cons *) elem);
	    }
	    objectFree((Object *) pqn, TRUE);
	}
    }

    return vector;
}


/* Add depsets for all explicitly defined dependencies. */
static void
addExplicitDepsForNode(
    DagNode *node, 
    Vector *nodes,
    Hash *nodes_by_fqn, 
    Hash *nodes_by_pqn)
{
    xmlNode *dep_node;
    String *fallback;
    String *old;
    String *new;
    Vector *deps;
    String *xnode_key;
    DagNode *fallback_node;
    DagNode *this;
    char *errmsg;

    assert(node, "addExplicitDepsForNode: no node provided");
    if (node->xnode_for) {
	/* Deps have already been added */
	return;
    }
    assert(node->dbobject, "addExplicitDepsForNode: node has no dbobject");

    for (dep_node = node->dbobject->children;
	 dep_node = nextDependency(dep_node);
	 dep_node = dep_node->next) 
    {
	fallback_node = NULL;
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
	if (old = nodeAttribute(dep_node, "old")) {
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("old deps handling not implemented"));
	}
	if (new = nodeAttribute(dep_node, "new")) {
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("new deps handling not implemented"));
	}
	deps = explicitDepsForNode(dep_node, nodes_by_fqn, nodes_by_pqn);

	this = node;  /* this is the node to which we will add deps */

	if (fallback_node || (deps && (deps->elems > 1))) {
	    /* Create an xnode. */
	    this = xnodeNew(node);
	    xnode_key = stringNew(this->fqn->value);
	    hashAdd(nodes_by_fqn, (Object *) xnode_key, (Object *) this);
	    vectorPush(nodes, (Object *) this);
	    addDep(node, this);
	    
	    if (fallback_node) {
		this->fallback = fallback_node;
	    }
	}	
	else {
	    if (!deps) {
		RAISE(TSORT_ERROR, 
		      newstr("Unable to find any dependency for dependency "
			     "set in %s\n(Perhaps a fallback needs to be "
			     "defined by the skit template developer)", 
			     node->fqn->value));
	    }
	}

	if (deps) {
	    addDepsVector(this, deps);
	    objectFree((Object *) deps, FALSE);
	}
    }
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




/* A TraverserFn to identify dbobject nodes, adding them to our vector
 * our vector.
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

static void
identifyDeps(Vector *nodes, Hash *byfqn)
{
    DagNode *node;
    int i;
    Hash *volatile bypqn = hashByPqn(nodes);

    BEGIN {
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    if (node->parent) {
		addDep(node, node->parent);
	    }
	}
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    addExplicitDepsForNode(node, nodes, byfqn, bypqn);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) bypqn, TRUE);
    }
    END;
}

Vector *
nodesFromDoc(Document *doc)
{
    Vector *volatile nodes = vectorNew(1000);
    Hash *volatile byfqn = NULL;

    BEGIN {
	(void) xmlTraverse(doc->doc->children, &addNodeToVector, 
			   (Object *) nodes);
	byfqn = hashByFqn(nodes);
	identifyParents(nodes, byfqn);
	identifyDeps(nodes, byfqn);
    }
    EXCEPTION(ex) {
	objectFree((Object *) nodes, TRUE);
    }
    FINALLY {
	objectFree((Object *) byfqn, FALSE);
    }
    END;
    return nodes;
}

#define CURDEP(node) ELEM(node->dependencies, node->dep_idx)


static void
replaceXnodeRefs(DagNode *xnode)
{
    DagNode *node = xnode->xnode_for;
    DagNode *dependency;
    DagNode *dep;
    int i;

    if (!xnode->dependencies) {
	dbgSexp(xnode);
	RAISE(TSORT_ERROR, 
	      newstr("No realised dependency for Xnode %s", xnode->fqn->value));
    }
    dependency = (DagNode *) CURDEP(xnode);    

    /* Remove all references to our xnode */
    EACH(xnode->dependents, i) {
	dep = (DagNode *) ELEM(xnode->dependents, i);
	setDel(dep->dependencies, (Object *) xnode);
    }
    EACH(xnode->dependencies, i) {
	dep = (DagNode *) ELEM(xnode->dependencies, i);
	setDel(dep->dependents, (Object *) xnode);
    }

    /* Add a full dependency to node for the xnode's actual dependency. */
    addDep(node, dependency);
}

/* Remove xnodes from the sorted nodes vector, and bring the realised
 * dependency into the node from which they originate.
 */
static void
eliminateXnodes(Vector *nodes)
{
    DagNode *node;
    int read_idx, write_idx;
    for (read_idx = write_idx = 0; read_idx < nodes->elems; read_idx++) {
	node = (DagNode *) ELEM(nodes, read_idx);
	if (node->xnode_for) {
	    replaceXnodeRefs(node);
	    objectFree((Object *) node, TRUE);
	    ELEM(nodes, read_idx) = NULL;
	}
	else {
	    ELEM(nodes, write_idx) = (Object *) node;
	    write_idx++;
	}
    }
    nodes->elems = write_idx;
}

static void recordRebuildNode(DagNode *node, Vector *nodelist);

static void
findRebuildNodesInVector(
    Vector *vector, 
    BuildTypeBitSet looking_for,
    Vector *foundlist)
{
    int i;
    DagNode *node;
    if (vector) {
	EACH(vector, i) {
	    node = (DagNode *) ELEM(vector, i);
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
    findRebuildNodesInVector(node->dependents, BUILD_NODE_BIT + 
				  DIFF_NODE_BIT + EXISTS_NODE_BIT,
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
    DagNode *dep;
    boolean doing_dependencies = FALSE;
    int i;
    while (TRUE) {
	deps = doing_dependencies? primary->dependencies: primary->dependents;

	if (deps) {
	    EACH(deps, i) {
		dep = (DagNode *) ELEM(deps, i);
		if (dep->duplicate_node) {
		    dep = dep->duplicate_node;
		}
		if (doing_dependencies) {
		    addDep(dup, dep);
		}
		else {
		    addDep(dep, dup);
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
    DagNode *node_for_breaker;
    int i;

    EACH(rebuild_nodes, i) {
	primary = (DagNode *) ELEM(rebuild_nodes, i);
	dup = dagnodeNew(primary->dbobject, DROP_NODE);
	primary->build_type = BUILD_NODE;
	primary->duplicate_node = dup;
	vectorPush(nodes, (Object *) dup);
    }
    EACH(rebuild_nodes, i) {
	primary = (DagNode *) ELEM(rebuild_nodes, i);
	dup = primary->duplicate_node;
	duplicateDeps(primary, dup);
	addDep(primary, dup);
	if (primary->breaker_for) {
	    dup->breaker_for = primary->breaker_for->duplicate_node;
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

static RedirectAction 
getRedirectionAction(DagNode *node, DagNode *dep)
{
    RedirectAction result;
    static RedirectAction redirect_action[4][4] = {
	{REDIRECT_RETAIN, REDIRECT_RETAIN,
	 REDIRECT_UNIMPLEMENTED, REDIRECT_DROP},          /* BUILD_NODE */
	{REDIRECT_INVERT, REDIRECT_INVERT,
	 REDIRECT_UNIMPLEMENTED, REDIRECT_DROP},          /* DROP_NODE */
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
	if (node->fallback_build_type != UNSPECIFIED_NODE) {
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
	    return TRUE;
	}
    }
    return FALSE;
}

static void
eliminateDependency(DagNode *node, int i, DagNode *breaker)
{
    DagNode *dep = (DagNode *) ELEM(node->original_dependencies, i);
    
    assert(dep->type == OBJ_DAGNODE,
	   "Need to be able to handle REFERENCES");
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
    DagNode *this;
    Vector *deps;

    if (node == breaker->breaker_for) {
	EACH(node->original_dependencies, i) {
	    this = (DagNode *) ELEM(node->original_dependencies, i);
	    if (isCycleNode(this, breaker)) {
		/* First, eliminate the dependency on our cycle_node.  */
		eliminateDependency(node, i, breaker);

		/* Next, add a dependency on node from the last element
		 * of the cycle chain.  */
		this = findCycleEnd(this, breaker);
		
		/* Note that this dependency has to be inverted.  */
		setPush(node->dependencies, (Object *) this);
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
	/* Take a temporary copy of the original  dependencies, and then
	 * reset dependencies.  We will  re-populate it below. */
	node = (DagNode *) ELEM(nodes, i);
	elems = node->dependencies? node->dependencies->elems: 1;
	node->original_dependencies = node->dependencies;
	node->dependencies = vectorNew(elems);
    }
	
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (deps = node->original_dependencies) {
	    EACH(deps, j) {
		depnode = (DagNode *) ELEM(deps, j);
		action = getRedirectionAction(node, depnode);
		switch (action) {
		case REDIRECT_INVERT:
		    setPush(depnode->dependencies, (Object *) node);
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
		    setPush(node->dependencies, (Object *) depnode);
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
	objectFree((Object *) node->dependents, FALSE);
	objectFree((Object *) node->original_dependencies, FALSE);
	node->original_dependencies = NULL;
	node->dependents = NULL;
    }
}

static void
promoteFallback(Vector *nodes, DagNode *instigator, DagNodeBuildType build_type)
{
    int i;
    DagNode *dep;
    DagNode *closer;

    instigator->fallback_build_type = build_type;
    instigator->build_type = BUILD_NODE;
    closer = dagnodeNew(instigator->dbobject, DROP_NODE);
    closer->fallback_build_type = build_type;
    closer->parent = instigator->parent;
    vectorPush(nodes, (Object*) closer);
    instigator->duplicate_node = closer;

    EACH(instigator->dependencies, i) {
	dep = (DagNode *) ELEM(instigator->dependencies, i);
	addDep(closer, dep);
    }
}

static DagNode *
getFallbackInstigator(Vector *nodes, DagNode *xnode)
{
    DagNode *fallback = xnode->fallback;
    if (fallback->fallback_build_type == UNSPECIFIED_NODE) {
	/* We have not yet instantiated the fallback node. */
	promoteFallback(nodes, fallback, xnode->build_type);
	return fallback;
    }
    else {
	if (fallback->fallback_build_type == xnode->build_type) {
	    /* Our instigator is the fallback node */
	    return fallback;
	}
	else {
	    RAISE(NOT_IMPLEMENTED_ERROR, newstr("ARG"));
	}
    }
}


/* Activate a fallback node and add it as a dependency to the xnode.  
 * Note that fallback nodes come in pairs, one to instigate the fallback
 * and one to close it down.  The xnode will depend on the instigator,
 * and the closer will depend on the xnode.
 */
static void
activateFallback(Vector *nodes, DagNode *xnode)
{
    DagNode *instigator = getFallbackInstigator(nodes, xnode);
    DagNode *closer = instigator->duplicate_node;

    addDep(xnode, instigator);
    /* Since xnode can only be given one dependency, we place the closer
     * dependency on the node for which this is an xnode.   Note that
     * the normal inversion which would be performed on this
     * dependency will not happen as fallback nodes are handled
     * specially.  */
    addDep(closer, xnode->xnode_for);
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
    DagNode *dep;
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
	dep = (DagNode *) ELEM(from_node->dependencies, i);
	addDep(breaker, dep);
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
    DagNode *breaker_for;

    /* Remove the cycle node in breaker that we copied from the
     * original node (breaker_for). */
    breaker_for = (DagNode *) CURDEP(node);
    node_in_cycle = (DagNode *) CURDEP(breaker_for);
    rmDep(breaker, node_in_cycle);

    /* Add deps to the breaker from each node in the cycle. */
    node_in_cycle = breaker_for;
    while (node_in_cycle != node) {
	addDep(node_in_cycle, breaker);

	node_in_cycle = (DagNode *) CURDEP(node_in_cycle);
    }

    /* Replace existing dependency on cycle_node from node, with a
     * dependency on breaker. */
    rmDep(node, breaker_for);
    addDep(node, breaker);
}

static void
tsort_node(Vector *nodes, DagNode *node, Vector *results);

static void
tsort_deps(Vector *nodes, DagNode *node, Vector *results)
{
    Vector *volatile deps;
    volatile int i;
    char *errmsg;
    char *tmp;
    DagNode *depnode;
    Object *reference;
    DagNode *cycle_node;
    DagNode *breaker;

    if (deps = node->dependencies) {
	EACH(deps, i) {
	    node->dep_idx = i;
	    errmsg = NULL;
	    depnode = (DagNode *) ELEM(deps, i);
	    BEGIN {
		tsort_node(nodes, depnode, results);
	    }
	    EXCEPTION(ex);
	    WHEN(TSORT_CYCLIC_DEPENDENCY) {
		/* Keep the exception trapping code short and simple -
		 * we don't want to raise exceptions from in here! */
		cycle_node = (DagNode *) ex->param;
		errmsg = newstr("%s", ex->text);
	    }
	    END;
	    if (errmsg) {
		/* A cyclic dependency was encountered.  Let's see if we
		 * can do something to resolve it. */
		if (node->xnode_for) {
		    /* In an xnode, only a single dependency from the
		     * vector needs to succeed, and even if none
		     * succeed, there may be a fallback solution. */

		    if ((i + 1) < deps->elems) {
			/* Try the next dep in the list. */
			skfree(errmsg);
			continue;
		    }
		}

		if (breaker = getBreakerFor(depnode)) {
		    /* We have a breaker for depnode.  Add it to our
		     * nodes vector. */
		    vectorPush(nodes, (Object *) breaker);
		    processBreaker(node, breaker);

		    skfree(errmsg);
		    i--;        /* The current depnode will have been
				 * replaced, so repeat this iteration. 
				 */
		    continue;   
		}

		/* Nothing we could do to resolve the cycle, so we just
		 * report it! */
		if (node == cycle_node) {
		    /* We are at the start of the cyclic dependency.
		     * Set errmsg to describe this, and reset cycle_node
		     * for the RAISE below. */ 
		    tmp = newstr("Cyclic dependency detected: %s->%s", 
				 node->fqn->value, errmsg);
		    cycle_node = NULL;
		}
		else if (cycle_node) {
		    /* We are somewhere in the cycle of deps.  Add the current
		     * node to the error message. */ 
		    tmp = newstr("%s->%s", node->fqn->value, errmsg);
		}
		else {
		    /* We are outside of the cyclic deps.  Add this node
		     * to the error message so we can see how we got
		     * to this point.  */ 
		    tmp = newstr("%s from %s", errmsg, node->fqn->value);
		}
		skfree(errmsg);
		errmsg = tmp;
		RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg, cycle_node);
	    }
	    if (node->xnode_for) {
		/* We have successfully traversed a dependency - there
		 * is nothing more to do.  */
		return;
	    }
	}
    }
    if (node->xnode_for) {
	/* We only reach this point if this is an xnode and we have not
	 * found a suitable dependency.  */
	if (node->fallback) {
	    activateFallback(nodes, node);
	    /* And try one more time... */
	    tsort_deps(nodes, node, results);
	}
	else {
	    RAISE(TSORT_ERROR, 
		  newstr("No realised dependency for Xnode %s", 
			 node->fqn->value));
	}
    }
}


static void
tsort_node(Vector *nodes, DagNode *node, Vector *results)
{
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, newstr("%s", node->fqn->value), node);
    case UNVISITED: 
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
	node->status = RESOLVED;
	break;
    case RESOLVED: 
	break;
    default:
	RAISE(TSORT_ERROR,
		  newstr("Unexpected status for dagnode %s: %d",
			 node->fqn->value, node->status));
    }
}

/* Create an initial sort of our dagnodes.  This identifies which
 * optional dependencies and fallbacks should be followed, and which
 * cycle-breakers should be employed.  This has two purposes: it turns
 * our list of dagnodes into a true dag, and it gives us an ordering
 * from which to split multi-action nodes (splitting multi-action nodes
 * should be done in dependency order, ie dependent objects before the
 * objects on which they depend).
 */
Vector *
resolving_tsort(Vector *nodes)
{
    Vector *volatile results = vectorNew(nodes->elems + 10); /* Allow a little
								extra for
								expansion */
    DagNode *node;
    int i;
    BEGIN {
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
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




/* This takes a set of nodes comprising a dependency graph, and converts
 * it into a true DAG.  In doing so it resolves optional dependencies,
 * identifies when fallback nodes must be added (to resolve unsatisfied
 * dependencies), and when cycle breaking nodes should be added.
 * After creating a simple DAG, nodes that represent combined build and
 * drop actions are expanded into pairs of nodes.  Finally dependencies
 * are inverted as required.  At the end of this process we have a DAG
 * which is buildable and which can be processed with a tsort.
 * 
 * Note that this completely rewrites *p_nodes removing some
 * DagNodes and creating new ones.  Callers must not rely on the
 * previous state of this vector following the call. 
 */
void
prepareDagForBuild(Vector **p_nodes)
{
    Vector *sorted = resolving_tsort(*p_nodes);
    Vector *rebuild_nodes = NULL;
    objectFree((Object *) *p_nodes, FALSE);
    *p_nodes = sorted;

    //fprintf(stderr, "\n\n1\n\n");
    //showVectorDeps(sorted);
    eliminateXnodes(sorted);
    //fprintf(stderr, "\n\n2\n\n");
    //showVectorDeps(sorted);
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
