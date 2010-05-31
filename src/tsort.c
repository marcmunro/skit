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

static Object *
addDagNodeToHash(Object *node, Object *hash)
{
    /* If this dbobject describes a diff, we will use that to figure out
     * what DagNodes to create, otherwise we figure it out from do_build
     * and do_drop. */

    if (do_build) {
	doAddNode((Hash *) hash, (Node *) node, BUILD_NODE);
    }

    if (do_drop) {
	doAddNode((Hash *) hash, (Node *) node, DROP_NODE);
    }

    return hash;
}

/* Build a hash of dagnodes from the provided document.  The hash
 * may contain one build node and one drop node per database object,
 * depending on which build and drop options have been selected.
 */
static Hash *
dagnodesFromDoc(Document *doc)
{
    Hash *daghash = hashNew(TRUE);
    String *xpath_expr = stringNew("//dbobject");

    do_build = dereference(symbolGetValue("build")) && TRUE;
    do_drop = dereference(symbolGetValue("drop")) && TRUE;

    BEGIN {
	(void) xpathEach(doc, xpath_expr, &addDagNodeToHash, 
			 (Object *) daghash);
    }
    EXCEPTION(ex) {
	objectFree((Object *) daghash, TRUE);
    }
    FINALLY {
	objectFree((Object *) xpath_expr, TRUE);
    }
    END;
    return daghash;
}

static Object *
addPqnEntry(Object *node, Object *hash)
{
    xmlChar *pqn = xmlGetProp(((Node *)node)->node, "pqn");
    xmlChar *fqn;
    String *key;
    String *value;
    Cons *list;
    Cons *prev;
    if (pqn) {
	fqn = xmlGetProp(((Node *)node)->node, "fqn");
	key = stringNew(pqn);
	value = stringNew(fqn);
	xmlFree(pqn);
	xmlFree(fqn);
	list = consNew((Object *) value, NULL);
	if (prev = (Cons *) hashAdd((Hash *) hash, 
				    (Object *) key, (Object *) list)) {
	    /* Append previous hash contents to list */
	    list->cdr = (Object *) prev;
	}
    }

    return hash;
}

static Hash *
makePqnHash(Document *doc)
{
    Hash *pqnhash = hashNew(TRUE);
    String *xpath_expr = stringNew("//dbobject");
    
    BEGIN {
	(void) xpathEach(doc, xpath_expr, &addPqnEntry, 
			 (Object *) pqnhash);
    }
    EXCEPTION(ex) {
	objectFree((Object *) pqnhash, TRUE);
    }
    FINALLY {
	objectFree((Object *) xpath_expr, TRUE);
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
addParentForNode(Object *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;
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
    setPush(dep->dependents, (Object *) node);
}

static void
addDependency(DagNode *node, Cons *deps)
{
    assert(node->type == OBJ_DAGNODE,
	"addDependency: Cannot handle non-dagnode nodes");
    if (!(node->dependencies)) {
	node->dependencies = vectorNew(10);
    }
    if (! setPush(node->dependencies, (Object *) deps)) {
	/* The dependency was already present, so just free up the cons
	 * we were passed. */
	freeConsNode(deps);
	return;
    }
    //dbgSexp(node->dependencies);
    //dbgSexp(deps);

    while (deps) {
	addDependent(node, (DagNode *) dereference(deps->car));
	deps = (Cons *) deps->cdr;
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
    String *fqn;
    String *pqn;
    DagNode *found;
    char *prefix = nameForBuildType(node->build_type);
    char *tmpstr;
    Hash *pqnlist = (Hash *) hashes->cdr;
    Cons *fqnlist;
    Cons *dagnodelist;

    if (fqn = getPrefixedAttribute(xmlnode, prefix, "fqn")) {
	found = (DagNode *) hashGet(dagnodes, (Object *) fqn);
	if (!found) {
	    tmpstr = newstr("processDependenciesForNode: no dependency "
			    "found for %s", fqn->value);
	    objectFree((Object *) fqn, TRUE);
	    RAISE(GENERAL_ERROR, tmpstr);
	}
	objectFree((Object *) fqn, TRUE);
	addDirectedDependency(node, consNode(found));
    }
    else if (pqn = nodeAttribute(xmlnode, "pqn")) {
	fqnlist = (Cons *) hashGet(pqnlist, (Object *) pqn);
	objectFree((Object *) pqn, TRUE);
	/* fqnlist is nil, is the item given by the pqn does not exist. */
	if (fqnlist) {
	    dagnodelist = dagnodeListFromFqnList(fqnlist, prefix, dagnodes);
	    addDirectedDependency(node, dagnodelist);
	}
    }
}

/* Find all of the dependency elements and add their dependencies to the
 * DAG */
static void
processDependencies(DagNode *node, Cons *hashes)
{
    // Looks like we cannot use xpath here as we have no appropriate
    // context node.  Instead we should directly traverse to child nodes
    // going to <dependencies> and then <dependency>
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

static void
addDepsForDropNode(DagNode *node, Cons *hashes)
{
    processDependencies(node, hashes);
    if (node->parent) {
	addDependency(node->parent, consNode(node));
    }
}

static Object *
addDepsForNode(Object *node_entry, Object *hashes)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;

    switch (node->build_type) {
    case BUILD_NODE: addDepsForBuildNode(node, (Cons *) hashes); break;
    case DROP_NODE:  addDepsForDropNode(node, (Cons *) hashes); break;
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
showDeps(Object *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;
    showAllNodeDeps(node);
    return (Object *) node;
}

static void
showAllDeps(Hash *nodes)
{
    hashEach(nodes, &showDeps, (Object *) nodes);
}

static Object *
addCandidateToBuild(Object *node_entry, Object *results)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;
    Vector *vector = (Vector *) results;
    String *parent_name;

    assert(node->type == OBJ_DAGNODE, "Node is not a dagnode");
    if ((node->status == DAGNODE_READY) && (!node->dependencies)) {
	vectorPush(vector, (Object *) node);
    }
    return (Object *) node;
}


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

    results = vectorNew(elems);
    
    vectorPush(results, (Object *) visit_node);
    
    if (deps = visit_node->dependents) {
	while (next = (DagNode *) dereference(vectorPop(deps))) {
	    removeDependency(next, visit_node);
	    if (!next->dependencies) {
		kid_results = simple_tsort_visit(next, candidates);
		vectorAppend(results, kid_results);
		objectFree((Object *) kid_results, FALSE);
		kid_results = NULL;
	    }
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
 * backtrack and try the next alternate dependency.  Backtracking in a a
 * recursive algorithm is unpleasant.
 * This algorithm works in the opposite direction.  It starts by
 * building a list of all leaf nodes (those without dependencies).  Then
 * it detaches these from the original list, revoking the dependencies
 * as it goes.  What this means is that the algorithm is driven from
 * dependents, and so as soon as any dependent is dealt with for a
 * PQN-based dependency, the dependency is satisfied.  This obviates the
 * need for backtracking at the expense of a less efficient algorithm.
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

static boolean handling_context = FALSE;

Vector *
gensort(Document *doc)
{
    Hash *dagnodes = NULL;
    Hash *pqnhash = NULL;
    Vector *sorted = NULL;
    Symbol *ignore_contexts = symbolGet("ignore-contexts");

    handling_context = (ignore_contexts == NULL);

    BEGIN {
	dagnodes = dagnodesFromDoc(doc);
	pqnhash = makePqnHash(doc);
	identifyDependencies(doc, dagnodes, pqnhash);
	sorted = simple_tsort(dagnodes);
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

    if (node) {
	for (context_node = findFirstChild(node->dbobject, "context");
	     context_node;
	     context_node = findNextSibling(context_node, "context")) {
	    name = nodeAttribute(context_node, "name");
	    value = nodeAttribute(context_node, "value");
	    cell = consNew((Object *) name, (Object *) value);
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
setContextNode(String *name, String *value)
{
    Node dbobject = {OBJ_XMLNODE, dbobjectNode(name->value, value->value)};
    return dagnodeNew(&dbobject, ARRIVE_NODE);
}

static DagNode *
resetContextNode(String *name, String *value)
{
    Node dbobject = {OBJ_XMLNODE, dbobjectNode(name->value, value->value)};
    return dagnodeNew(&dbobject, DEPART_NODE);
}

static void
addContextNavigation(Vector *vec, DagNode *from, DagNode *target)
{
    Cons *from_contexts;
    Cons *target_contexts;
    Cons *this;
    Cons *match;
    String *name;
    DagNode *context_node;
    from_contexts = getContexts(from);
    target_contexts = getContexts(target);
    while (target_contexts && (this = (Cons *) consPop(&target_contexts))) {
	name = (String *) this->car;
	if (from_contexts &&
	    (match = (Cons *) alistExtract(&from_contexts, 
					   (Object *) name))) {
	    /* We have the same context for both dagnodes. */
	    if (objectCmp(this->cdr, match->cdr) != 0) {
		context_node = resetContextNode(name, (String *) match->cdr);
		vectorPush(vec, (Object *) context_node);
		context_node = setContextNode(name, (String *) this->cdr);
		vectorPush(vec, (Object *) context_node);
		//fprintf(stderr, "RESET CONTEXT %s(%s)\n", name->value,
		//	((String *) match->cdr)->value);
		//fprintf(stderr, "SET CONTEXT %s(%s)\n", name->value,
		//	((String *) this->cdr)->value);
	    }
	    objectFree((Object *) match, TRUE);
	}
	else {
	    /* This is a new context */
	    context_node = setContextNode(name, (String *) this->cdr);
	    vectorPush(vec, (Object *) context_node);
	    //fprintf(stderr, "SET CONTEXT %s(%s)\n", name->value,
	    //	    ((String *) this->cdr)->value);
	}
	objectFree((Object *) this, TRUE);
    }
    while (from_contexts && (this = (Cons *) consPop(&from_contexts))) {
	name = (String *) this->car;
	context_node = resetContextNode(name, (String *) this->cdr);
	vectorPush(vec, (Object *) context_node);
	//fprintf(stderr, "RESET CONTEXT %s(%s)\n", name->value,
	//	((String *) this->cdr)->value);
	objectFree((Object *) this, TRUE);
    }
}

/* Return a vector of DagNodes containing the navigation to get from
 * from to target */
Vector *
navigationToNode(DagNode *from, DagNode *target)
{
    Vector *results = vectorNew(10);
    DagNode *current = NULL;
    DagNode *next = NULL;
    DagNode *common_root = NULL;
    DagNode *navigation = NULL;

    BEGIN {
	if (from) {
	    common_root = getCommonRoot(from, target);
	    current = from;
	    while (!nodeEq(current, common_root)) {
		if ((current == from) &&
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
	if (handling_context) {
	    addContextNavigation(results, from, target);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) results, TRUE);
    }
    END;
    return results;
}

