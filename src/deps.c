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




#define CURDEP(node) ELEM(node->dependencies, node->dep_idx)




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

/* Deresolve any elements that were resolved as part of a cyclic
 * dependency loop.  We will re-resolve them later if we can.
 */

/*

 COMMENT NEEDED HERE DESCRIBING THE LATEST ALGORITHM FOR GENERATING A
 DAG FROM OUR SOMEWHAT UNSTRUCTURED SET OF DEPENDENCIES

*/







void
showDeps(DagNode *node)
{
    DagNode *sub;
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
buildTypeForDagNode(Node *node)
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
	    errmsg = newstr("buildTypeForDagnode: unexpected diff "
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
addDagNodeToVector(Object *this, Object *vector)
{
    Node *node = (Node *) this;
    DagNodeBuildType build_type;
    DagNode *dagnode;

    if (streq(node->node->name, "dbobject")) {
	dagnode = dagNodeNew(node->node, UNSPECIFIED_NODE);
	dagnode->build_type = buildTypeForDagNode(node);
	vectorPush((Vector *) vector, (Object *) dagnode);
    }

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

/* Return a Hash of Dagnodes keyed by fqn.
 */
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

/* Return a Hash of lists of DagNodes keyed by pqn.
 */
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
		   "identifyParents2: parent of %s (%s) not found",
		   node->fqn->value, parent_fqn->value);
	    objectFree((Object *) parent_fqn, TRUE);
	}
    }
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
		if ((dep->type == OBJ_DAGNODE) &&
		    (((DagNode *) dep)->build_type == EXISTS_NODE)) {
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
addDepToVector(Vector **p_vector, DagNode *dep)
{
    if (!*p_vector) {
	*p_vector = vectorNew(10);
    }
    setPush(*p_vector, (Object *) dep);
}

/* Add a dependency to a DagNode in whatever directions are appropriate.
 *
 */
static void
addDep(DagNode *node, DagNode *dep, DependencyApplication applies)
{
    if ((applies == FORWARDS) || (applies == BOTH)) {
	addDepToVector(&(node->forward_deps), dep);
    }
    if ((applies == BACKWARDS) || (applies == BOTH)) {
	addDepToVector(&(node->backward_deps), dep);
    }
}


static DagNode *
newSubNode(DagNode *node, DependencyApplication applies)
{
    DagNode *new = dagNodeNew(node->dbobject, OPTIONAL_NODE);
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
    Vector *deps;
    int i;
    boolean forwards;
    xmlSetProp(breaker_dbobject, "type", breaker_type->value);
    xmlUnsetProp(breaker_dbobject, "cycle_breaker");
    xmlSetProp(breaker_dbobject, "fqn", new_fqn);
    xmlFree(old_fqn);
    skfree(new_fqn);

    breaker = dagNodeNew(breaker_dbobject, from_node->build_type);
    breaker->parent = from_node->parent;
    breaker->breaker_for = from_node;

    /* Copy dependencies of from_node to breaker.  We will eliminate
     * unwanted ones later. */
    
    for (forwards = 0; forwards <= 1; forwards++) {
	deps = forwards? from_node->forward_deps: from_node->backward_deps;
	EACH (deps, i) {
	    dep = (DagNode *) ELEM(deps, i);
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

static DagNode *
getBreakerFor(DagNode *node, Vector *nodes)
{
    String *breaker_type;

    if (!node->breaker) {
	if (breaker_type = nodeAttribute(node->dbobject, "cycle_breaker")) {
	    node->breaker = makeBreakerNode(node, breaker_type);
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
processBreaker(
    DagNode *curnode, 
    DagNode *depnode, 
    DagNode *breaker,
    boolean forwards)
{
    Vector *deps;
    DagNode *thru = (DagNode *) ELEM(depnode->forward_deps, depnode->dep_idx);
    DagNode *this;
    Vector *newdeps;
    int i;

    /* Eliminiate from breaker any dependency on thru */
    deps = forwards? breaker->forward_deps: breaker->backward_deps;
    this = (DagNode *) vectorDel(deps, (Object *) thru);
    if (this != thru) {
	RAISE(TSORT_ERROR, 
	      newstr("Unable to eliminate dependency %s -> %s",
		     breaker->fqn->value, this->fqn->value));
    }

    /* Replace the dependency from curnode to depnode with one from
     * curnode to breaker. */
    deps = forwards? curnode->forward_deps: curnode->backward_deps;
    EACH(deps, i) {
	this = (DagNode *) ELEM(deps, i);
	if (this == depnode) {
	    ELEM(deps, i) = (Object *) breaker;
	}
    }
}



/* Add both forward and back dependencies to a DagNode from the source
 * xml objects.
 */
static void
addDepsForNode(DagNode *node, Vector *allnodes, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode;
    Object *deps;
    DagNode *fallback_node;
    DagNode *sub;
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
	fallback_node = getFallbackNode(depnode, byfqn);
	deps = getDepSets(depnode, byfqn, bypqn);

	if (deps && (deps->type == OBJ_DAGNODE) && 
	    (((DagNode *) deps)->build_type == EXISTS_NODE))
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
	    addDep(node, (DagNode *) deps, applies);
	}
    }
    //showDeps(node);
}

/* Determine the declared set of dependencies for the nodes, creating
 * both forward and backward dependencies.
 */
static void
addDependencies(Vector *nodes, Hash *byfqn, Hash *bypqn)
{
   DagNode *volatile this = NULL;
   int volatile i;
   EACH(nodes, i) {
       this = (DagNode *) ELEM(nodes, i);
       assert(this->type == OBJ_DAGNODE, "incorrect node type");
       addDepsForNode(this, nodes, byfqn, bypqn);
   }
}

static void
resolveNode(DagNode *node, DagNode *from, boolean forwards, Vector *nodes);

static void
resolveDeps(DagNode *node, DagNode *from, boolean forwards, Vector *nodes)
{
    Vector *volatile deps = forwards? node->forward_deps: node->backward_deps;
    int volatile i;
    boolean volatile cyclic_exception;
    DagNode *volatile dep;
    DagNode *breaker;
    DagNode *cycle_node;
    char *tmpmsg;
    char *errmsg;

    if (deps) {
	EACH(deps, i) {
	    dep = (DagNode *) ELEM(deps, i);
	    node->dep_idx = i;
	    BEGIN {
		cyclic_exception = FALSE;
		resolveNode(dep, node, forwards, nodes);
	    }
	    EXCEPTION(ex);
	    WHEN(TSORT_CYCLIC_DEPENDENCY) {
		cyclic_exception = TRUE;
		cycle_node = (DagNode *) ex->param;
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
		
		if (breaker = getBreakerFor(dep, nodes)) {
		    /* Replace the current dependency on dep with
		     * one instead on breaker.  Breaker has been created
		     * with the same dependencies as depnode, so we must
		     * remove from it the dependency that causes the
		     * cycle.  Then we replace the current dependency on
		     * depnode with one instead on breaker and finally
		     * we retry processing this, modified, dependency. */
		    
		    skfree(errmsg);
		    processBreaker(node, dep, breaker, forwards);
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
resolveSubNodes(
    DagNode *node, 
    DagNode *from, 
    boolean forwards,
    Vector *nodes)
{
    DagNode *sub = node;
    Vector *deps;
    DagNode *dep;
    while (sub = forwards? sub->forward_subnodes: sub->backward_subnodes) {
	resolveNode(sub, from, forwards, nodes);
    }
}

static void
resolveNode(DagNode *node, DagNode *from, boolean forwards, Vector *nodes)
{
    assert(node && node->type == OBJ_DAGNODE, 
	   "resolveNode: node is not a DagNode");
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
	    resolveSubNodes(node, from, forwards, nodes);
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
activateFallback(DagNode *node, DagNode *fallback, Vector *nodes)
{
    DagNode *closer;
    DagNode *dep;
    int i;
    if (fallback->build_type != FALLBACK_NODE) {
	/* Promote this node from an exists node into a build node, and
	 * create the matching drop node for it. */
	fallback->build_type = FALLBACK_NODE;
	closer = dagNodeNew(fallback->dbobject, ENDFALLBACK_NODE);
	fallback->mirror_node = closer;
	setPush(nodes, (Object *) closer);

	/* Copy deps from build node to drop node.  For fallback nodes,
	 * all dependendencies wodk in the forward direction as we are
	 * just going to grant a privilege and then revoke it and the
	 * role to which the grant and revoke is applied should be the
	 * only initial dependency. */
	EACH(fallback->forward_deps, i) {
	    dep = (DagNode *) ELEM(fallback->forward_deps, i);
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
    DagNode *node;
    DagNode *sub;
    DagNode *dep;
    int i;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNode(node, NULL, TRUE, nodes);
    }

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNode(node, NULL, FALSE, nodes);
    }

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	sub = node;
	while (sub = sub->forward_subnodes) {
	    if (sub->dep_idx < 0) {
		activateFallback(node, sub->fallback_node, nodes);
	    }
	    else {
		dep = (DagNode *) ELEM(sub->forward_deps, sub->dep_idx);
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
		activateFallback(node, sub->fallback_node, nodes);
	    }
	    else {
		dep = (DagNode *) ELEM(sub->backward_deps, sub->dep_idx);
		addDepToVector(&(node->backward_deps), dep);
	    }
	    //printSexp(stderr, "Promoting :", (Object *) dep);
	    //printSexp(stderr, " in :", (Object *) node);
	}
    }
}

static DagNode *
makeMirror(DagNode *node, DagNodeBuildType type)
{
    DagNode *mirror = dagNodeNew(node->dbobject, type);
    node->mirror_node = mirror;
    mirror->mirror_node = node;
    mirror->parent = node->parent;
    addDepToVector(&(node->forward_deps), mirror);
    return mirror;
}

static void
expandDagNodes(Vector *nodes)
{
    int i;
    DagNode *node;
    DagNode *mirror;
    
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);

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
	    mirror = makeMirror(node, DIFFPREP_NODE);
	    break;
	case REBUILD_NODE:
	    node->build_type = BUILD_NODE;
	    mirror = makeMirror(node, DROP_NODE);
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
    DagNode *node;
    
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
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
    DagNode *node;
    DagNode *depnode;
    Vector *deps;

    clearUnneededDeps(nodes);

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);

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
		    depnode = (DagNode *) ELEM(deps, j);
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
   Vector *volatile nodes = dagNodesFromDoc(doc);
   Hash *volatile byfqn = hashByFqn(nodes);
   Hash *volatile bypqn = NULL;
   DagNode *volatile this = NULL;
   int volatile i;
   
   BEGIN {
       identifyParents(nodes, byfqn);
       bypqn = hashByPqn(nodes);
       addDependencies(nodes, byfqn, bypqn);
       resolveGraphs(nodes);
       expandDagNodes(nodes);
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
