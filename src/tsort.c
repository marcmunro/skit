/**
 * @file   tsort.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for performing a topological sort.
 */

/* Definitions
 * FQN: dependencies defined with fqn specify a fully qualified name
 * dependency.  These dependencies must be satisfied by an 
 * object with the same fqn.
 * PQN: dependencies specified with a pqn may be satisfied by *any*
 * object with a matching pqn.  Typically these are for grants, where
 * the necessary privilege to perform a grant may have been provided
 * in several ways.  We currently allow pqn definitions to fail to
 * match an object.  This allows us to have, for example: pqn dependencies
 * on usage privilege on schema x granted to role y, or to public.  We
 * do not need to care whether the grant to y, or the grant to public
 * exists, but if they do (and one of them must), there will be a
 * dependency on it.  This is a crass simplification and technically
 * wrong but is probably good enough.  I'll fix it when I discover it
 * needs to be fixed.
 */

/* Marc's traversal cost-reduced tsort algorithm
 * (by cost we mean the cost of switching contexts when building
 *  each node.  This algorithm aims to do as much as possible within
 *  one context before switching to another.  The context for the
 *  building of a node, is actually the parent node of that being
 *  built.  Keeping switches of build context down means that we switch
 *  from one database to another or one schema to another less
 *  frequently.  This makes the resulting script easier to understand
 *  and edit, and also reduces the number of context changing statements 
 *  that have to be issued - especially for changing databases):
 * A DagNode is a record containing a reference to an xml dbobject
 * element that represents a node in a DAG (Directed Acyclic Graph).
 * Each DagNode contains a list of dependencies and dependents (TODO:
 * CHECK THE VERACITY OF PREVIOUS STATEMENT)
 * The sort runs in a number of passes.  Each pass runs for 
 * given starting node (null for the first pass).
 * pass(current_node, nodelist)
 *   generate a list of nodes that have no dependencies
 *     (these are candidates for being bult)
 *   remove those nodes from nodelist
 *   sort the candidate list by parent, type and name
 *     (parent == current node sorts first)
 *   count = 0
 *   repeat
 *     kids = 0
 *     for each node in the list
 *       append node to results
 *       count++
 *       kids += pass(node, nodelist)
 *     end
 *     count += kids
 *   until kids = 0
 *   return count
 * end   
 *
 *
 * get_candidates(nodelist, buildlist)
 *   for node = each entry in nodelist
 *     if node has no dependencies
 *       parent = get parent for node
 *       append node name to buildlist[parent]
 *     end
 *   end
 * end 
 *
 * get_context_node(cur_context_node, buildlist)
 *   if buildlist[cur_context_node] is not empty
 *     return cur_context_node
 *   else
 *     get first non-empty buildlist key, sorting keys as follows:
 *       descendants of cur_context_node (deepest levels first)
 *         by name and type
 *       deepest nodes first, by name and type
 *   end
 * end 
 *
 * tsort(nodelist)
 *   resultlist = []
 *   buildlist = new hash
 *   context_node = NULL
 *   get_candidates(nodelist, buildlist)
 *   while buildlist is not empty
 *     context_node = get_context_node(context_node, buildlist)
 *     for node in sort nodes in buildlist by type, then name
 *       remove dependencies from and to node
 *       add node to resultlist
 *     end
 *     get_candidates(nodelist, buildlist)
 *   end
 *   if any nodes in nodelist still have dependencies
 *     raise a cyclic dependency error
 *   end
 * end
 *
 */

#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"

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

static boolean do_build = FALSE;
static boolean do_drop = FALSE;

static Object *
addDagNodeToHash(Object *node, Object *hash)
{
    // If this dbobject describes a diff, we will use that to figure out
    // what DagNodes to create, otherwise we figure it out from do_build
    // and do_drop.

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
Hash *
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

static void
addDependent(DagNode *node, DagNode *dep)
{
    ObjReference *refnode = objRefNew(dereference((Object *) node));

    assert(node->type == OBJ_DAGNODE,
	"addDependent: Cannot handle non-dagnode nodes");
    assert(dep->type == OBJ_DAGNODE,
	"addDependent: Cannot handle non-dagnode dependent");
    
    if (!(dep->dependents)) {
	dep->dependents = vectorNew(10);
    }
    vectorPush(dep->dependents, (Object *) refnode);
}

static void
addDependency(DagNode *node, Cons *deps)
{
    assert(node->type == OBJ_DAGNODE,
	"addDependency: Cannot handle non-dagnode nodes");
    if (!(node->dependencies)) {
	node->dependencies = vectorNew(10);
    }
    vectorPush(node->dependencies, (Object *) deps);

    while (deps) {
	addDependent(node, (DagNode *) dereference(deps->car));
	deps = (Cons *) deps->cdr;
    }
}

static Cons *
consNode(DagNode *node)
{
    ObjReference *ref = objRefNew(dereference((Object *) node));
    Cons *result = consNew((Object *) ref, NULL);
    return result;
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

void
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

static String root_name = {OBJ_STRING, ".."};

static void
addToBuildList(Hash *buildlist, DagNode *node)
{
    Vector *vector;
    String *parent_name;
    if (node->parent) {
	parent_name = node->parent->fqn;
    }
    else {
	parent_name = &root_name;
    }
    
    vector = (Vector *) hashGet(buildlist, (Object *) parent_name);
    if (!vector) {
	vector = vectorNew(20);
	(void) hashAdd(buildlist, (Object *) stringDup(parent_name),
		       (Object *) vector);
    }
    vectorPush(vector, (Object *) objRefNew((Object *) node));
    node->status = DAGNODE_SORTING;
}

static Object *
addCandidateToBuild(Object *node_entry, Object *buildlist)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;
    Vector *vector;
    String *parent_name;

    assert(node->type == OBJ_DAGNODE, "Node is not a dagnode");
    if ((node->status == DAGNODE_READY) && (!node->dependencies)) {
	addToBuildList((Hash *) buildlist, node);
    }
    return (Object *) node;
}

static void
get_build_candidates(Hash *nodelist, Hash *buildlist)
{
    hashEach(nodelist, &addCandidateToBuild, (Object *) buildlist);
    return;
}

static int
fqncmp(String *fqn1, String *fqn2)
{
    char *str1 = fqn1->value;
    char *str2 = fqn2->value;

    while (str1 && str2) {
	str1 = strchr(str1+1, '.');
	str2 = strchr(str2+1, '.');
    }
    if (str2) {
	/* str1 has fewer dot characters, so it is the smaller */
	return -1;
    }
    if (str1) {
	return 1;
    }
    return strcmp(fqn1->value, fqn2->value);
}

/* Return the heridity part of the fqn.  FQNs have the form:
 * action.type.ancestor_heridity.name or action.type in the case of the
 * cluster. */
static char *
fqnHeridity(char *fqn)
{
    char *notype = strchr(fqn, '.');
    char *result;
    if (notype) {
	notype++;
	if (result = strchr(notype, '.')) {
	    return result + 1;
	}
    }
    return notype;
}

static boolean
isDescendant(String *fqn, String *child_fqn)
{
    /* Eliminate the type prefix from both fqns */
    char *parent = fqnHeridity(fqn->value);
    char *child = fqnHeridity(child_fqn->value);

    /* dname is a descendant iff, all characters match up to length
     * of the parent fqn */

    assert(parent, "isDescendant: Oops, parent is not defined");
    assert(child, "isDescendant: Oops, child is not defined");

    while ((*parent != '\0') && (*parent == *child)) {
	parent++;
	child++;
    }
    return *parent == '\0';
}

static Object *
getPreferredContext(Object *node_entry, Object *cons)
{
    String *fqn = (String *) ((Cons *) node_entry)->car;
    Object *result = ((Cons *) node_entry)->cdr;
    String *prev = (String *) ((Cons *)cons)->cdr;
    String *context = (String *) ((Cons *)cons)->car;

    if (context) {
	if (!isDescendant(context, fqn)) {
	    return result;
	}
    }

    if (!prev) {
	((Cons *)cons)->cdr = (Object *) fqn;
    }
    else {
	if (fqncmp(fqn, prev) < 0) {
	    ((Cons *)cons)->cdr = (Object *) fqn;
	}
    }
    return result;
}

/* Get the most appropriate context_node for the set of nodes to be
 * built next.  We sort on number of dots in fqn and then fqn
 */
static String *
get_context_node(Hash *buildlist)
{
    Cons cons = {OBJ_CONS, NULL, NULL};
    hashEach(buildlist, &getPreferredContext, (Object *) &cons);
    return (String *) cons.cdr;
}


/* Find the most appropriate context_node from buildlist that is a child
 * of the current context_node.
 */
static String *
get_child_context_node(DagNode *cur_node, Hash *buildlist)
{
    Cons cons = {OBJ_CONS, (Object *) cur_node->fqn, NULL};
    hashEach(buildlist, &getPreferredContext, (Object *) &cons);
    return (String *) cons.cdr;
}

/* Compare two DagNodes with the result providing a reverse ordering */
static int
dagnodercmp(const void *node1, const void *node2)
{
    Object *obj1 = (Object *) (*(void **) node1);
    Object *obj2 = (Object *) (*(void **) node2);
    DagNode *dnode1;
    DagNode *dnode2;
    int result;

    assert(obj1->type == OBJ_OBJ_REFERENCE, 
	   newstr("cmpBuildNodes: invalid obj1 type(%d) %p", 
		  obj1->type, obj1));
    assert(obj2->type == OBJ_OBJ_REFERENCE, 
	   newstr("cmpBuildNodes: invalid obj2 type(%d) %p", 
		  obj2->type, obj2));
    dnode1 = (DagNode *) dereference(obj1);
    dnode2 = (DagNode *) dereference(obj2);
    assert(dnode1->type == OBJ_DAGNODE, 
	   newstr("cmpBuildNodes: invalid dnode1 type(%d) %p", 
		  dnode1->type, dnode1));
    assert(dnode2->type == OBJ_DAGNODE, 
	   newstr("cmpBuildNodes: invalid dnode2 type(%d) %p", 
		  dnode2->type, dnode2));

    return strcmp(dnode2->fqn->value, dnode1->fqn->value);
}


static void
reverseSortBuildVector(Vector *vector)
{
    qsort(vector->contents->vector, vector->elems, 
	  sizeof(Object *), &dagnodercmp);
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

/* Remove dependencies to and from node, prior to adding node to the
 * results list.  If this results in nodes having no dependencies, they
 * will be added to buildlist.  */
static void
removeNodeFromDag(DagNode *node, Hash *allnodes, Hash *buildlist)
{
    Vector *vec = node->dependents;
    DagNode *depnode;
    Object *obj;
    int i;

    assert(node->status == DAGNODE_SORTING,
	   "Incorrect status for node");
    if (vec) {
	for (i = vec->elems-1; i >= 0; i--) {
	    depnode = (DagNode *) dereference(vec->contents->vector[i]);
	    assert(depnode->type == OBJ_DAGNODE,
		   "removeNodeFromDag: Cannot handle non-dagnode depnodes");
	    if (depnode->dependencies->elems <= 1) {
		objectFree((Object *) depnode->dependencies, TRUE);
		depnode->dependencies = NULL;
		addToBuildList(buildlist, depnode);
	    }
	    else {
		removeDependency(depnode, node);
	    }
	    node->status = DAGNODE_SORTED;
	}
    }
    /* All dependencies have been removed, now we free the dependents
     * vector */
    objectFree((Object *) vec, TRUE);
    node->dependents = NULL;

    /* And since nothing now references node, we can remove it from the
     * allnodes hash.  Note that we don't free the object as it will
     * be in the results vector and will be freed from there. */

    obj = hashDel(allnodes, (Object *) node->fqn);
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
showAllDeps(Hash *buildlist)
{
    hashEach(buildlist, &showDeps, (Object *) buildlist);
}

static void
buildInContext(String *context_fqn, Hash *buildlist,
	       Hash *allnodes, Vector *results)
{
    Vector *to_build;
    Object *ref;
    Object *entry;
    DagNode *node;
    int i;
    String *child_context_fqn;

    while ((to_build = (Vector *) hashGet(buildlist, (Object *) context_fqn)) &&
	   to_build->elems) {
	reverseSortBuildVector(to_build);
	//printSexp(stderr, "CONTEXT: ", context_fqn);
	//printSexp(stderr, "BUILDLIST: ", buildlist);
	ref = vectorPop(to_build);
	node = (DagNode *) dereference(ref);
	//printSexp(stderr, "BUILDING: ", node);
	objectFree((Object *) ref, FALSE);
	removeNodeFromDag(node, allnodes, buildlist);
	//showAllDeps(allnodes);
	vectorPush(results, (Object *) node);

	/* Building this node has established a new build context.
	 * Try building under that context. */
	get_build_candidates(allnodes, buildlist);
	while (child_context_fqn = get_child_context_node(node, buildlist)) {
	    buildInContext(child_context_fqn, buildlist, allnodes, results);
	}
    }
    /* Now remove this context from the buildlist */

    //printSexp(stderr, "CONTEXT: ", context_fqn);
    //printSexp(stderr, "BUILDLIST: ", buildlist);
    entry = hashDel(buildlist, (Object *) context_fqn);
    objectFree(entry, TRUE);
    //fprintf(stderr, "END\n");
}

static Vector *
tsort(Hash *allnodes)
{
    Vector *results = NULL;
    Hash   *buildlist = hashNew(TRUE);
    int     elems;
    String *context_fqn = NULL;

    elems = hashElems(allnodes);
    results = vectorNew(elems);
    //dbgSexp(allnodes);
    get_build_candidates(allnodes, buildlist);

    while (hashElems(buildlist)) {
	context_fqn = get_context_node(buildlist);
	buildInContext(context_fqn, buildlist, allnodes, results);
    }
    objectFree((Object *) buildlist, TRUE);
    return results;
}


/* Reset the DAGNODE status of each DagNode */
static Object *
resetDagNodeEntry(Object *node_entry, Object *ignore)
{
    DagNode *node;
    assert(isCons((Cons *) node_entry),
	   "resetDagNodeEntry: parameter is not a cons cell");
    node = (DagNode *) ((Cons *) node_entry)->cdr;
    assert((node->type == OBJ_DAGNODE),
	   "resetDagNodeEntry: entry is not a dagnode");
    fprintf(stderr, "RESETTING: %s\n", node->fqn->value);
    node->status = DAGNODE_READY;
    return (Object *) node;
}


/* Return any entry from the hash */
static Object *
anyEntry(Object *node_entry, Object *p_result)
{
    Object *entry;
    Object **result_ptr = (Object **) p_result;

    assert(isCons((Cons *) node_entry),
	   "anyEntry: parameter is not a cons cell");
    entry = ((Cons *) node_entry)->cdr;
    *result_ptr = entry;
    return entry;
}

/* This implements the node traversal of a traditional tsort.  It visits
 * each dependency in turn setting the status to SORTING.  If it finds 
 * a node where the status is already SORTING, then a cyclic dependency
 * has been encountered.  Note that this only deals with fqns for now.
 * I hope there will be no need to deal properly with pqns for this,
 * which is only here for better error reporting.
 */
static void
traverse(DagNode *this, DagNode *from)
{
    int i;
    DagNode *next;
    Cons *cons;

    if (this->status == DAGNODE_SORTING) {
	RAISE(GENERAL_ERROR, 
	      newstr("cyclic dependency detected from %s to %s",
		     from->fqn->value, this->fqn->value));
    }
    if (this->status == DAGNODE_READY) {
	this->status = DAGNODE_SORTING;
	fprintf(stderr, "TRAVERSED TO NEW NODE\n");
	showNodeDeps(this);
	if (this->dependencies) {
	    for (i = 0; i < this->dependencies->elems; i++) {
		cons = (Cons *) this->dependencies->contents->vector[i];
		next = (DagNode *) dereference(cons->car);

		fprintf(stderr, "Traversing to dependency %s\n", 
			next->fqn->value);
		dbgSexp(next);
		traverse(next, this);
		fprintf(stderr, "back\n");
	    }
	}
	if (this->parent) {
	    next = (DagNode *) dereference((Object *) this->parent);
	    fprintf(stderr, "Traversing to parent %s\n", next->fqn->value);
	    traverse(next, this);
	    fprintf(stderr, "back\n");
	}
	this->status = DAGNODE_SORTED;
    }
}

static void
describeDAGBreakage(Hash *dagnodes)
{
    DagNode *start = NULL;

    hashEach(dagnodes, &resetDagNodeEntry, NULL);
    hashEach(dagnodes, &anyEntry, (Object *) &start);
    traverse(start, NULL);
    return;
}

Vector *
gensort(Document *doc)
{
    Hash *dagnodes = NULL;
    Hash *pqnhash = NULL;
    Vector *sorted = NULL;
    BEGIN {
	dagnodes = dagnodesFromDoc(doc);
	pqnhash = makePqnHash(doc);
	identifyDependencies(doc, dagnodes, pqnhash);
	//showAllDeps(dagnodes);
	sorted = tsort(dagnodes);
	//dbgSexp(sorted);
	if (hashElems(dagnodes)) {
	    /* If we get here something bad has happened: there are
	       unbuilt dagnodes in the dagnodes hash, which means that
	       for some reason we have not been able to find a single build
	       candidate for the remaining dagnodes.  This means our DAG
	       is effectively broken (not acyclic).  */
	    objectFree((Object *) sorted, TRUE);
	    describeDAGBreakage(dagnodes);  /* This will raise an
					     * exception if it detects
					     * the problem. */
					     
	    RAISE(GENERAL_ERROR, newstr("NO BUILD CANDIDATES!"));
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
    }
    EXCEPTION(ex) {
	objectFree((Object *) results, TRUE);
    }
    END;
    return results;
}

