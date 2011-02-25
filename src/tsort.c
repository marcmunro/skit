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
#include <stdarg.h>
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
	//dbgNode(node->node);
	//dbgNode(node->node->parent);
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

static DagNode *
findMatchingParent(Hash *dagnodes, char *fqn, ...)
{
    va_list args;
    char *prefix;
    String *key;
    DagNode *result;

    va_start(args, fqn);
    while (prefix = va_arg(args, char *)) {
	key = stringNewByRef(newstr("%s.%s", prefix, fqn));
	result = hashGet(dagnodes, (Object *) key);
	objectFree((Object *) key, TRUE);
	if (result) {
	    break;
	}
    }
    va_end(args);
    return result;
}

static Object *
addParentForNode(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    xmlNode *xmlnode = findAncestor(node->dbobject, "dbobject");
    xmlChar *fqn = NULL;
    DagNode *parent;
    if (xmlnode) {
	BEGIN {
	    fqn  = xmlGetProp(xmlnode, "fqn");

	    switch (node->build_type) {
	    case BUILD_NODE:
	    case DIFF_NODE:
	    case EXISTS_NODE:
		parent = findMatchingParent((Hash *) dagnodes, fqn, 
					    "build", "exists", "diff", NULL);
		break;
	    case DROP_NODE:
		parent = findMatchingParent((Hash *) dagnodes, fqn, 
					    "drop", "exists", "diff", NULL);
		break;
	    default:
		RAISE(NOT_IMPLEMENTED_ERROR,
		      newstr("addParentForNode of type %d is not implemented",
			     node->build_type));
	    }
	    if (parent) {
		node->parent = parent;
	    }
	    else {
		RAISE(TSORT_ERROR,
		      newstr("addParentForNode: No parent found for %s",
			  node->fqn->value));
	    }
	}
	EXCEPTION(ex);
	FINALLY {
	    if (fqn) {
		xmlFree(fqn);
	    }
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
}

static Object *
addDependentsForNode(Cons *node_entry, Object *param)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Hash *allnodes = (Hash *) param;
    Vector *dependencies = node->dependencies;
    int i;
    Cons *deps;
    DagNode *dep;

    if (dependencies) {
	for (i = 0; i < dependencies->elems; i++) {
	    deps = (Cons *) dependencies->contents->vector[i];
	    while (deps) {
		addDependent(node, (DagNode *) dereference(deps->car));
		deps = (Cons *) deps->cdr;
	    }
	}
    }
    return (Object *) node;
}


static void
addAllDependents(Hash *allnodes)
{
    hashEach(allnodes, &addDependentsForNode, (Object *) allnodes);
}

/* We have not implemented the use of a list of alternate dependencies
 * for diffs.  It is not obvious to me that it is actually needed,
 * though I clearly thought so when implementing addDependency.  
 * TODO: Either add this or refactor addDependency to simplify it by
 * removing the use of lists. 
 */
static void
tsortdebug(char *x)
{
    fprintf(stderr, "DEBGUG %s\n", x);
}

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
	case EXISTS_NODE: 
	    addDependency(node, consNode(depnode));
	    break;
	default:
	    dbgSexp(node);
	    tsortdebug("zz");
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

static Cons *
removeDagnodeFromList(Cons *list, DagNode *node)
{
    Cons *prev = NULL;
    Cons *cur = list;
    Cons *result;

    while (cur) {
	if ((DagNode *) dereference(cur->car) == node) {
	    if (prev) {
		prev->cdr = cur->cdr;
		result = list;
	    }
	    else {
		result = (Cons *) cur->cdr;
	    }
	    objectFree(cur->car, FALSE);
	    objectFree((Object *) cur, FALSE);
	    return result;
	}
	prev = cur;
	cur = (Cons *) cur->cdr;
    }
    return list;
}

static void
removeDependency(DagNode *from, DagNode *dependency)
{
    int i;
    Cons *deplist;
    Object *obj;
    DagNode *dep;

    if (from->dependencies) {
	for (i = 0; i < from->dependencies->elems; i++) {
	    deplist = (Cons *) from->dependencies->contents->vector[i];
	    deplist = removeDagnodeFromList(deplist, dependency);
	    if (deplist) {
		from->dependencies->contents->vector[i] = (Object *) deplist;
	    }
	    else {
		(void) vectorRemove(from->dependencies, i);
	    }
	}
	if (!from->dependencies->elems) {
	    objectFree((Object *) from->dependencies, TRUE);
	    from->dependencies = NULL;
	}
    }
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
copyDeps(DagNode *target, DagNode *src)
{
    DagNode *ignore = src->cur_dep;
    Cons *src_cons;
    Cons *target_cons;
    DagNode *src_dagnode;
    int i;

    if (src->dependencies) {
	for (i = 0; i < src->dependencies->elems; i++) {
	    src_cons = (Cons *) src->dependencies->contents->vector[i];
	    target_cons = NULL;
	    while (src_cons) {
		src_dagnode = (DagNode *) dereference(src_cons->car);
		target_cons = consNew((Object *) objRefNew(
					  (Object *) src_dagnode), 
				      (Object *) target_cons); 
		src_cons = (Cons *) src_cons->cdr;
	    }

	    if (target_cons) {
		addDependency(target, target_cons);
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
	copyDeps(breaker, cycle_node);
	removeDependency(breaker, cur_node);
	xmlSetProp(cycle_node->dbobject, "cycle_breaker_fqn", 
		   breaker->fqn->value);
	objectFree((Object *) breaker_type, TRUE);
    }
    return breaker;
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
    Cons *depset, 
    Hash *allnodes,
    boolean break_allowed,
    Cons **p_cycle)
{
    DagNode *dep;
    Cons *cycle;
    String *newfqn;

    *p_cycle = NULL;
    while (depset) {
	dep = (DagNode *) dereference(depset->car);
	node->cur_dep = dep;
	if (cycle = visitNode(dep, allnodes)) {
	    /* We have a cyclic dependency. */
	    depset = (Cons *) depset->cdr;
	    if (break_allowed) {
		if (dep = attemptCycleBreak(node, dep)) {
		    newfqn = stringNew(dep->fqn->value);
		    hashAdd(allnodes, (Object *) newfqn, (Object *) dep);
		    objectFree((Object *) cycle, TRUE);
		    return dep;
		}

		*p_cycle = cycle;
		return NULL;
	    }
	    if (depset) {
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
	    return dep;
	}
    }
    return dep;
}

static Cons *
visitNode(DagNode *node, Hash *allnodes)
{
    int i;
    Cons *depset;
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
        for (i = 0; i < node->dependencies->elems; i++) {
            depset = (Cons *) node->dependencies->contents->vector[i];
	    dep = dependencyFromSet(node, depset, allnodes, FALSE, &result);
	    if (!dep) {
		/* We have an unhandled cyclic dependency.  Attempt a
		 * retry, allowing it to be handled. */
		objectFree((Object *) result, TRUE);
		dep = dependencyFromSet(node, depset, allnodes, TRUE, &result);
		if (!dep) {
		    result = consNew((Object *) objRefNew((Object *) node),
				     (Object *) result);
		    node->status = UNVISITED;
		    return result;
		}
	    }

	    /* Replace depset with the single dependency to
	     * which we have successfully traversed.  */
	    objectFree(node->dependencies->contents->vector[i], TRUE);
	    depset = consNew((Object *) objRefNew((Object *) dep),
			     NULL);
	    node->dependencies->contents->vector[i] = (Object *) depset;
        }
    }
    node->status = VISITED;
    return NULL;
}

static Object *
visitNodeInHash(Cons *entry, Object *allnodes)
{
    DagNode *node = (DagNode *) entry->cdr;
    Cons *result;
    char *deps;
    char *errmsg;
    
    if (result = visitNode(node, (Hash *) allnodes)) {
	deps = objectSexp((Object *) result);
	objectFree((Object *) result, TRUE);
	errmsg = newstr("Cyclic dependency in %s", deps);
	skfree(deps);
	RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg);
    }

    return (Object *) node;
}

static void
check_dag(Hash *allnodes)
{
    hashEach(allnodes, &visitNodeInHash, (Object *) allnodes);
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

    addAllDependents(allnodes);
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
 
    if (hashElems(allnodes)) {
	char *nodes = objectSexp((Object *) allnodes);
	char *errmsg = newstr("gensort: unsorted nodes remain:\n\"%s\"\n",
			      nodes);
	skfree(nodes);
	RAISE(GENERAL_ERROR, errmsg);
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
	//dbgSexp(doc);
	dagnodes = dagnodesFromDoc(doc);
	pqnhash = makePqnHash(doc);
	identifyDependencies(doc, dagnodes, pqnhash);
	check_dag(dagnodes);
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
	fprintf(stderr, "DEPARTURES ");
	dbgSexp(results);
	while (!nodeEq(current, target)) {
	    dbgSexp(current);
	    dbgSexp(target);
	    dbgSexp(target->parent);
	    current = nextNodeFrom(current, target);
	    //printSexp(stderr, "nextNodeFrom(current, target) is: ", current);
	    if ((current == target) &&
		(current->build_type == BUILD_NODE)) {
		/* We don't need to arrive at a build node as the build
		 * must perform the arrival for us. */
	    }
	    else {
		//printSexp(stderr, "Navigation to: ", current);
		if (navigation = arriveNode(current)) {
		    dbgSexp(navigation);
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

