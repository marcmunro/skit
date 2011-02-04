/**
 * @file   tsort.c
 * \code
 *     Copyright (c) 2010 Marc Munro
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

static boolean do_build = FALSE;
static boolean do_drop = FALSE;

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

/* Identify dbobject nodes, adding them as Dagnodes to our hash
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

static Object *
pqnsToHash(Object *this, Object *pqnhash)
{
    Node *node = (Node *) this;
    xmlChar *pqn;
    xmlChar *fqn;
    String *key;
    String *value;
    Cons *list;
    Cons *prev;
    if (streq(node->node->name, "dbobject")) {
	if (pqn  = xmlGetProp(node->node, "pqn")) {
	    fqn = xmlGetProp(((Node *)node)->node, "fqn");
	    key = stringNew(pqn);
	    value = stringNew(fqn);
	    xmlFree(pqn);
	    xmlFree(fqn);
	    list = consNew((Object *) value, NULL);
	    if (prev = (Cons *) hashAdd((Hash *) pqnhash, 
					(Object *) key, (Object *) list)) {
		/* Append previous hash contents to list */
		list->cdr = (Object *) prev;
	    }
	}
    }
    return NULL;
}

static Hash *
makePqnHash(Document *doc)
{
    Hash *pqnhash = hashNew(TRUE);
    
    BEGIN {
	(void) xmlTraverse(doc->doc->children, &pqnsToHash, 
			   (Object *) pqnhash);
    }
    EXCEPTION(ex) {
	objectFree((Object *) pqnhash, TRUE);
    }
    END;
    return pqnhash;
}

static boolean
xmlnodeMatch(xmlNode *node, char *name)
{
    return node && (node->type == XML_ELEMENT_NODE) && streq(node->name, name);
}

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

static xmlNode *
findFirstChild(xmlNode *parent, char *name)
{
    if (xmlnodeMatch(parent->children, name)) {
	return parent->children;
    }
    return findNextSibling(parent->children, name);
}

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

static Object *
addParentForNode(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    xmlNode *xmlnode = findAncestor(node->dbobject, "dbobject");
    String *fqn;

    if (xmlnode) {
	BEGIN {
	    switch (node->build_type) {
	    case BUILD_NODE:
		fqn = getPrefixedAttribute(xmlnode, "build", "fqn");
		break;
	    case DROP_NODE:
		fqn = getPrefixedAttribute(xmlnode, "drop", "fqn");
		break;
	    default:
		RAISE(NOT_IMPLEMENTED_ERROR,
		      newstr("addParentForNode of type %d is not implemented",
			     node->build_type));
	    }
	    node->parent = (DagNode *) hashGet((Hash *) dagnodes, 
					       (Object *) fqn);
	}
	EXCEPTION(ex);
	FINALLY {
	    objectFree((Object *) fqn, TRUE);
	}
	END;
    }
    return (Object *) node;
}

static Cons *
consNode(DagNode *node)
{
    ObjReference *ref = objRefNew(dereference((Object *) node));
    Cons *result = consNew((Object *) ref, NULL);
    return result;
}

static void
freeConsNode(Cons *node)
{
    objectFree(node->car, FALSE);
    objectFree((Object *) node, FALSE);
}

static void
addDependent(DagNode *node, DagNode *dep)
{
    assert(node->type == OBJ_DAGNODE,
	"addDependent: Cannot handle non-dagnode nodes");
    assert(dep->type == OBJ_DAGNODE,
	"addDependent: Cannot handle non-dagnode dependent");
    
    if (!(dep->dependents)) {
	dep->dependents = vectorNew(10);
    }
    setPush(dep->dependents, (Object *) objRefNew((Object *) node));
}

static void
addDependency(DagNode *node, Cons *deps)
{
    assert(node->type == OBJ_DAGNODE,
	"addDependency: node must be a dagnode");
    if (!(node->dependencies)) {
	node->dependencies = vectorNew(10);
    }

    if (! setPush(node->dependencies, (Object *) deps)) {
	/* The dependency was already present, so just free up the cons
	 * we were passed. */
	freeConsNode(deps);
	return;
    }

    while (deps) {
	addDependent(node, (DagNode *) dereference(deps->car));
	deps = (Cons *) deps->cdr;
    }
}

/* We have not implemented the use of a list of alternate dependencies
 * for diffs.  It is not obvious to me that it is actually needed,
 * though I clearly thought so when implementing addDependency.  
 * TODO: Either add this or refactor addDependency to simplify it by
 * removing the use of lists. 
 */
static void
addDiffDependency(DagNode *node, DagNode *depnode, boolean old)
{
    if (old) {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("addDiffDependency not implemented for old deps"));
    }
    else {
	switch (depnode->build_type) {
	case BUILD_NODE: 
	case DIFF_NODE: 
	    addDependency(node, consNode(depnode));
	    break;
	default:
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("addDiffDependency not implemented for all types"));
	}
    }
}

/* Like addDependency but handles dependencies in the opposite direction
 * (eg for drops) */
static void
addInvertedDependencies(DagNode *node, Cons *deps)
{
    Cons *prev;
    DagNode *dep;
    while (deps) {
	dep = (DagNode *) dereference(deps->car);
	addDependency(dep, consNode(node));
	prev = deps;
	deps = (Cons *) deps->cdr;
	objectFree(prev->car, FALSE);
	objectFree((Object *) prev, FALSE);
    }
}

/* Must use or free the deps object! */
static void
addDirectedDependency(DagNode *node, Cons *deps)
{
    switch (node->build_type) {
    case BUILD_NODE: 
	addDependency(node, deps);
	break;
    case DROP_NODE: 
	addInvertedDependencies(node, deps);
	break;
    default:
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("addDirectedDependency does not handle this: %d",
		     node->build_type));
    }
}

static Cons *
dagnodeListFromFqnList(Cons *fqnlist, char *prefix, Hash *dagnodes)
{
    Cons *result = NULL;
    Cons *prev;
    String *base_fqn;
    String *key;
    Object *node;

    while (fqnlist) {
	base_fqn = (String *) fqnlist->car;
	key = stringNewByRef(newstr("%s.%s", prefix, base_fqn->value));
	node = dereference(hashGet(dagnodes, (Object *) key));
	objectFree((Object *) key, TRUE);
	prev = result;
	result = consNew((Object *) objRefNew(node), (Object *) prev);
	fqnlist = (Cons *) fqnlist->cdr;
    }
    return result;
}

static void
addXmlnodeDependencies(DagNode *node, xmlNode *xmlnode, Cons *hashes)
{
    Hash *dagnodes = (Hash *) hashes->car;
    String *fqn = NULL;
    String *pqn = NULL;
    DagNode *found;
    char *prefix = nameForBuildType(node->build_type);
    char *tmpstr;
    Hash *pqnlist = (Hash *) hashes->cdr;
    Cons *fqnlist;
    Cons *dagnodelist = NULL;

    BEGIN {
	if (fqn = getPrefixedAttribute(xmlnode, prefix, "fqn")) {
	    found = (DagNode *) hashGet(dagnodes, (Object *) fqn);
	    if (!found) {
		tmpstr = newstr("addXmlnodeDependencies: no dependency "
				"found for %s in %s", fqn->value,
		                node->fqn->value);
		RAISE(GENERAL_ERROR, tmpstr);
	    }
	    /* addDirectedDependency must use or free the consNode
	     * created below. */
	    addDirectedDependency(node, consNode(found));
	}
	else if (pqn = nodeAttribute(xmlnode, "pqn")) {
	    fqnlist = (Cons *) hashGet(pqnlist, (Object *) pqn);
	    objectFree((Object *) pqn, TRUE);
	    pqn = NULL;
	    /* fqnlist is nil, is the item given by the pqn does not exist. */
	    if (fqnlist) {
		dagnodelist = dagnodeListFromFqnList(fqnlist, prefix, dagnodes);
		addDirectedDependency(node, dagnodelist);
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) fqn, TRUE);
	objectFree((Object *) pqn, TRUE);
    }
    END;
}

/* Find all of the dependency elements and add their dependencies to the
 * DAG */
static void
processDependencies(DagNode *node, Cons *hashes)
{
    /* We cannot use xpath here as we have no appropriate context node.
     * Instead we should directly traverse to child nodes going to
     * <dependencies> and then <dependency> */
    xmlNode *deps_node = node->dbobject->children;
    xmlNode *dep_node;

    for (deps_node = findFirstChild(node->dbobject, "dependencies");
	 deps_node;
	 deps_node = findNextSibling(deps_node, "dependencies")) {
	for (dep_node = findFirstChild(deps_node, "dependency");
	     dep_node;
	     dep_node = findNextSibling(dep_node, "dependency")) {
	    addXmlnodeDependencies(node, dep_node, hashes);
	}
	break;
    }
}

static void
addDepsForBuildNode(DagNode *node, Cons *hashes)
{
    char *base_fqn = strchr(node->fqn->value, '.');
    char *depname = newstr("drop%s", base_fqn);
    String *depkey = stringNewByRef(depname);
    Hash *dagnodes = (Hash *) hashes->car;
    DagNode *drop_node = (DagNode *) hashGet(dagnodes, (Object *) depkey);

    BEGIN {
	/* Build nodes are dependent on the equivalent drop node if it
	 * exists (ie if we are doing a drop and build, the drop must
	 * happen first) */
	if (drop_node) {
	    addDependency(node, consNode(drop_node));
	}
	processDependencies(node, hashes);
	if (node->parent) {
	    addDependency(node, consNode(node->parent));
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) depkey, TRUE);
    }
    END;
}

static DagNode *
getDepForDiff(xmlNode *dep, Cons *hashes, boolean is_old)
{
    String *fqn;
    Object *result = NULL;
    String *key;

    if (fqn = nodeAttribute(dep, "fqn")) {
	/* Now get an appropriate dependency record */
	key = stringNewByRef(newstr("exists.%s", fqn->value));
	if (result = hashGet((Hash *) hashes->car, (Object *) key)) {
	    objectFree((Object *) key, TRUE);
	    objectFree((Object *) fqn, TRUE);
	    /* No need to record a dependency against an exists node */
	    return NULL;
	}
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("getDepForDiff non-exists nodes not implemented"));
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("getDepForDiff pqn handling not implemented"));
    }
    return (DagNode *) result;
}

/* These are the rules for diff dependencies:     
 * - for standard dependencies:
 *   - if there is an exists node, all is well and there is no
 *     dependency
 *   - if there is s build node we depend on that
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
static void
addDepsForDiffNode(DagNode *node, Cons *hashes)
{
    xmlNode *deps_node;
    xmlNode *dep_node;
    String *old;
    boolean is_old;
    DagNode *dependency;
    
    if (node->parent) {
	addDiffDependency(node, node->parent, FALSE);
    }

    for (deps_node = findFirstChild(node->dbobject, "dependencies");
	 deps_node;
	 deps_node = findNextSibling(deps_node, "dependencies")) {
	for (dep_node = findFirstChild(deps_node, "dependency");
	     dep_node;
	     dep_node = findNextSibling(dep_node, "dependency")) 
	{
	    old = nodeAttribute(dep_node, "old");
	    if (is_old = (old && TRUE)) {
		objectFree((Object *) old, TRUE);
	    }

	    if (dependency = getDepForDiff(dep_node, hashes, is_old)) {
		addDiffDependency(node, dependency, is_old);
	    }
	}
	break;
    }
}

static void
addDepsForDropNode(DagNode *node, Cons *hashes)
{
    processDependencies(node, hashes);
    if (node->parent) {
	addDependency(node->parent, consNode(node));
    }
}

static Object *
addDepsForNode(Cons *node_entry, Object *hashes)
{
    DagNode *node = (DagNode *) node_entry->cdr;

    switch (node->build_type) {
    case BUILD_NODE: addDepsForBuildNode(node, (Cons *) hashes); break;
    case DIFF_NODE:  addDepsForDiffNode(node, (Cons *) hashes); break;
    case DROP_NODE:  addDepsForDropNode(node, (Cons *) hashes); break;
    case EXISTS_NODE: 
	/* No deps for this node as it already exists before we apply
	 * diffs and continues to exist after. */
	break;
    default: RAISE(NOT_IMPLEMENTED_ERROR,
		   newstr("addDepsForNode of type %d is not implemented",
			  node->build_type));
    }
    return (Object *) node;
}

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

static boolean
inList(Cons *list, Object *obj)
{
    while (list) {
	if (dereference(list->car) == obj) {
	    return TRUE;
	}
	list = (Cons *) list->cdr;
    }
    return FALSE;
}

static void
removeDependency(DagNode *from, DagNode *dependency)
{
    int i;
    Cons *deplist;
    Object *obj;

    if (from->dependencies) {
	for (i = 0; i < from->dependencies->elems; i++) {
	    deplist = (Cons *) from->dependencies->contents->vector[i];
	    if (inList(deplist, (Object *) dependency)) {
		obj = vectorRemove(from->dependencies, i);
		objectFree(obj, TRUE);
	    }
	}
	if (!from->dependencies->elems) {
	    objectFree((Object *) from->dependencies, TRUE);
	    from->dependencies = NULL;
	}
    }
}

static Vector *
simple_tsort_visit(DagNode *visit_node, Hash *candidates)
{
    int elems = hashElems(candidates);
    Vector *deps;
    Vector *results = NULL;
    Vector *kid_results = NULL;
    DagNode *next;
    Object *ref;

    results = vectorNew(elems);
    
    vectorPush(results, (Object *) visit_node);
    
    if (deps = visit_node->dependents) {
	while (ref = vectorPop(deps)) {
	    next = (DagNode *) dereference(ref);
	    removeDependency(next, visit_node);
	    if (!next->dependencies) {
		kid_results = simple_tsort_visit(next, candidates);
		vectorAppend(results, kid_results);
		objectFree((Object *) kid_results, FALSE);
		kid_results = NULL;
	    }
	    objectFree(ref, FALSE);
	}
    }

    /* Finally, we remove visit_node from our candidates hash. */
    (void) hashDel(candidates, (Object *) visit_node->fqn);
    return results;
}

/* This is not the standard tsort algorithm.  Instead it is a very
 * naive algorithm, which happens to deal well with PQN (partially
 * qualified name)-based dependencies.  A dependency based on a PQN
 * offers a set of alternate dependencies.  A PQN-based dependency is
 * satisfied if any of the dependencies in the set is satisfied.  The
 * standard tsort algorithm is very efficient but is driven by
 * dependencies, so if one dependency of a PQN cannot be satisfied a
 * cyclic dependency will be found.  In this case, we would have to 
 * backtrack and try the next alternate dependency.  Backtracking in a
 * recursive algorithm is unpleasant.
 * This algorithm works in the opposite direction.  It starts by
 * building a list of all leaf nodes (those without dependencies).  Then
 * it detaches these from the original list, revoking the dependencies
 * as it goes.  What this means is that the algorithm is driven from
 * dependents, and so as soon as any dependent is dealt with for a
 * PQN-based dependency, the dependency is satisfied.  This obviates the
 * need for backtracking at the expense of a less efficient algorithm.
 *
 * API notes: allnodes should be empty when we are done!
 */
static Vector *
simple_tsort(Hash *allnodes)
{
    Vector *results = NULL;
    Vector *kid_results = NULL;
    Vector *buildable = NULL;
    DagNode *node = NULL;
    int elems;
    int prev = 0;
    BEGIN {
	elems = hashElems(allnodes);

	while (TRUE) {
	    buildable = get_build_candidates(allnodes);
	    while (node = (DagNode *) dereference(vectorPop(buildable))) {
		kid_results = simple_tsort_visit(node, allnodes);
		if (results) {
		    vectorAppend(results, kid_results);
		    objectFree((Object *) kid_results, FALSE);
		}
		else {
		    results = kid_results;
		}
	    }
	    objectFree((Object *) buildable, TRUE);
	    if (results->elems == elems) {
		/* We are done! */
		break;
	    }
	    if (results->elems <= prev) {
		showAllDeps(allnodes);
		RAISE(TSORT_CYCLIC_DEPENDENCY,
		      newstr("No progress - cyclic dependency?"));
	    }
	    prev = results->elems;
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) results, TRUE);
    }
    END;
    return results;
}


/* This function checks that the set of dependencies in allnodes, makes
 * A DAG.  Specifically, it resolves optional dependencies picking a
 * specific dependency from each set of candidates, and attempts to
 * resolve cyclic dependencies by checking for SOMETHING.
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
 */
static Cons *
visitNode(DagNode *node)
{
    int i;
    Cons *depset;
    DagNode *dep;
    Cons *result;
    boolean found = FALSE;

    switch (node->status) {
    case VISITED:
	return NULL;
    case VISITING:
	return consNew((Object *) objRefNew((Object *) node), NULL);
    }
    node->status = VISITING;
    for (i = 0; i < node->dependencies->elems; i++) {
	depset = (Cons *) node->dependencies->contents->vector[i];
	while (depset && !found) {
	    dep = (DagNode *) dereference(depset->car);
	    if (result = visitNode(dep)) {
		depset = (Cons *) depset->cdr;
		fprintf(stderr, "TRYING AGAIN\n");
		dbgSexp(depset);
	    }
	    else {
		found = TRUE;
		// FIXME: REPLACE depset with single DEP
	    }
	}
	if (!found) {
	    RAISE(TSORT_CYCLIC_DEPENDENCY, newstr("ARG"));
	    return result;
	}
    }
    node->status = VISITED;
    return NULL;
}

static Object *
visitNodeInHash(Cons *entry, Object *ignore)
{
    DagNode *node = (DagNode *) entry->cdr;
    Cons *result;
    char *deps;
    char *errmsg;
    if (result = visitNode(node)) {
	deps = objectSexp((Object *) result);
	errmsg = newstr("Cyclic dependency: %s", deps);
	skfree(deps);
	RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg);
    }

    return (Object *) node;
}

static void
check_dag(Hash *allnodes)
{
    fprintf(stderr, "CHECKING_DAG\n");
    hashEach(allnodes, &visitNodeInHash, NULL);
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

/* Create a sorted tree, reflecting the hierarchy of DagNodes.  At each
 * level of the tree, the nodes are sorted in fqn order.
 */
static DagNode *
sortedTree(Hash *allnodes)
{
    Vector *nodelist = nodeList(allnodes);
    DagNode *root = NULL;
    DagNode *node;
    int i;

    /* First we sort the vector by fqn. */
    qsort((void *) nodelist->contents->vector,
	  nodelist->elems, sizeof(Object *), fqnCmp);

    /* Now add each node into it's rightful place in the tree */
    for (i = 0; i < nodelist->elems; i++) {
	node = (DagNode *) nodelist->contents->vector[i];
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

/* Mark this node as buildable, and update the counts of buildable_kids
 * in all ancestors. */
static void
markAsBuildable(DagNode *node)
{
    DagNode *up = node->parent;
    node->is_buildable = TRUE;
    while (up) {
	up->buildable_kids++;
	up = up->parent;
    }
}

/* Remove node as a build candidate (after it has been selected for
 * building), taking care of its ancestors' counts of buildable_kids */
static void
unmarkBuildable(DagNode *node)
{
    DagNode *up = node->parent;
    node->is_buildable = FALSE;
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
    if (node->is_buildable) {
	return node;
    }
    if (node->buildable_kids) {
	return nextBuildable(node->kids);
    }
    if (sibling = node->next) {
	while (sibling != node) {
	    if (sibling->is_buildable) {
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
 *           --   boolean: is_buildable, integer: buildable_kids
 *           -- initialised to false and 0 respectively
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
    DagNode *root = sortedTree(allnodes);
    DagNode *next;
    int i;
    Vector *buildable = get_build_candidates(allnodes);
    Vector *results = vectorNew(hashElems(allnodes));

    //showAllDeps(allnodes);
    markAllBuildable(buildable);

    next = nextBuildable(root);
    while (next) {
	(void) vectorPush(results, (Object *) next);
	unmarkBuildable(next);
	buildable = removeNodeGetNewCandidates(next, allnodes);
	markAllBuildable(buildable);
	next = nextBuildable(next);
    }
 
    return results;
}

static boolean handling_context = FALSE;

Vector *
gensort(Document *doc)
{
    Hash *dagnodes = NULL;
    Hash *pqnhash = NULL;
    Vector *sorted = NULL;
    Symbol *ignore_contexts = symbolGet("ignore-contexts");
    Symbol *simple_sort = symbolGet("simple-sort");
    
    handling_context = (ignore_contexts == NULL);

    BEGIN {
	dagnodes = dagnodesFromDoc(doc);
	pqnhash = makePqnHash(doc);
	identifyDependencies(doc, dagnodes, pqnhash);
	check_dag(dagnodes);

	if (simple_sort) {
	    sorted = simple_tsort(dagnodes);
	}
	else {
	    sorted = smart_tsort(dagnodes);
	}
	if (hashElems(dagnodes)) {
	    char *nodes = objectSexp((Object *) dagnodes);
	    char *errmsg = newstr("gensort: unsorted nodes remain:\n\"%s\"\n",
				  nodes);
	    skfree(nodes);
	    RAISE(GENERAL_ERROR, errmsg);
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
    return results;
}

