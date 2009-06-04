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

    BEGIN {
	if (old = hashAdd(hash, (Object *) key, (Object *) dagnode)) {
	    objectFree(old, TRUE);
	    RAISE(GENERAL_ERROR, 
		  newstr("doAddNode: duplicate node \"%s\"", key->value));
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) dagnode, FALSE);
	objectFree((Object *) key, TRUE);
    }
    END;
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
addParentForNode(Object *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;
    xmlNode *xmlnode = node->dbobject;
    String *fqn;

    //printSexp(stderr, "CHECKING FOR PARENT OF: ", node);
    //printElem(xmlnode);
    while (xmlnode = (xmlNode *) xmlnode->parent) {
	if ((xmlnode->type == XML_ELEMENT_NODE) &&
	    streq(xmlnode->name, "dbobject")) {
	    break;
	}
    }

    if (xmlnode) {
	//fprintf(stderr, "FOUND PARENT\n");
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
addDependency(DagNode *node, DagNode *dep)
{
    ObjReference *refdep = objRefNew((Object *) dep);
    ObjReference *refnode = objRefNew((Object *) node);
    if (!(node->dependencies)) {
	node->dependencies = vectorNew(10);
    }
    vectorPush(node->dependencies, (Object *) refdep);
    if (!(dep->dependents)) {
	dep->dependents = vectorNew(10);
    }
    vectorPush(dep->dependents, (Object *) refnode);
}

static void
addDirectedDependency(DagNode *node, DagNode *dep)
{
    switch (node->build_type) {
    case BUILD_NODE: 
	addDependency(node, dep);
	break;
    case DROP_NODE: 
	addDependency(dep, node);
	break;
    default:
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("addDirectedDependency does not handle this: %d",
		     node->build_type));
    }
}

static void
processDependenciesForNode(DagNode *node, xmlNode *xmlnode, Hash *dagnodes)
{
    String *fqn;
    String *pqn;
    DagNode *found;
    char *prefix = nameForBuildType(node->build_type);
    char *tmpstr;
    
    if (fqn = getPrefixedAttribute(xmlnode, prefix, "fqn")) {
	found = (DagNode *) hashGet(dagnodes, (Object *) fqn);
	if (!found) {
	    tmpstr = newstr("processDependenciesForNode: no dependency "
			    "found for %s", fqn->value);
	    objectFree((Object *) fqn, TRUE);
	    RAISE(GENERAL_ERROR, tmpstr);
	}
	objectFree((Object *) fqn, TRUE);
	addDirectedDependency(node, found);
    }
    else if (pqn = getPrefixedAttribute(xmlnode, prefix, "pqn")) {
	// PQNs should be handled as a list of matches rather than a
	// single match.
	printSexp(stderr, "PQN: ", (Object *) pqn);
	objectFree((Object *) pqn, TRUE);
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("processDependenciesForNode does not yet handle pqns"));
    }
}

static void
processDependencies(DagNode *node, Hash *dagnodes)
{
    // Looks like we cannot use xpath here as we have no appropriate
    // context node.  Instead we should directly traverse to child nodes
    // going to <dependencies> and then <dependency>
    xmlNode *deps_node = node->dbobject->children;
    xmlNode *dep_node;

    for (; deps_node; deps_node = deps_node->next) {
	if ((deps_node->type == XML_ELEMENT_NODE) &&
	    streq(deps_node->name, "dependencies")) {
	    for (dep_node = deps_node->children; dep_node; 
		 dep_node = dep_node->next) {
		if ((dep_node->type == XML_ELEMENT_NODE) &&
		    streq(dep_node->name, "dependency")) {
		    processDependenciesForNode(node, dep_node, dagnodes);
		}
	    }
	    break;
	}
    }
}

static void
addDepsForBuildNode(DagNode *node, Hash *dagnodes)
{
    char *base_fqn = strchr(node->fqn->value, '.');
    char *depname = newstr("drop%s", base_fqn);
    String *depkey = stringNewByRef(depname);
    DagNode *drop_node = (DagNode *) hashGet(dagnodes, (Object *) depkey);

    BEGIN {
	/* Build nodes are dependent on the equivalent drop node if it
	 * exists (ie if we are doing a drop and build, the drop must
	 * happen first) */
	if (drop_node) {
	    addDependency(node, drop_node);
	}
	processDependencies(node, dagnodes);
	if (node->parent) {
	    addDependency(node, node->parent);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) depkey, TRUE);
    }
    END;
}

static void
addDepsForDropNode(DagNode *node, Hash *dagnodes)
{
    processDependencies(node, dagnodes);
    if (node->parent) {
	addDependency(node->parent, node);
    }
}

static Object *
addDepsForNode(Object *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;

    switch (node->build_type) {
    case BUILD_NODE: addDepsForBuildNode(node, (Hash *) dagnodes); break;
    case DROP_NODE:  addDepsForDropNode(node, (Hash *) dagnodes); break;
    default: RAISE(NOT_IMPLEMENTED_ERROR,
		   newstr("addDepsForNode of type %d is not implemented",
			  node->build_type));
    }
    return (Object *) node;
}

void
identifyDependencies(Document *doc,
		     Hash *dagnodes)
{
    hashEach(dagnodes, &addParentForNode, (Object *) dagnodes);
    hashEach(dagnodes, &addDepsForNode, (Object *) dagnodes);
}

static String root_name = {OBJ_STRING, ".."};

static Object *
addCandidateToBuild(Object *node_entry, Object *buildlist)
{
    DagNode *node = (DagNode *) ((Cons *) node_entry)->cdr;
    Vector *vector;
    String *parent_name;

    if ((node->status == DAGNODE_READY) &&
	(!node->dependencies)) {
	if (node->parent) {
	    parent_name = node->parent->fqn;
	}
	else {
	    parent_name = &root_name;
	}
	
	vector = (Vector *) hashGet((Hash *) buildlist, 
				    (Object *) parent_name);
	if (!vector) {
	    vector = vectorNew(20);
	    (void) hashAdd((Hash *) buildlist, 
			   (Object *) stringDup(parent_name),
			   (Object *) vector);
	}
	vectorPush(vector, (Object *) objRefNew((Object *) node));
	node->status = DAGNODE_SORTING;
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

static boolean
isDescendant(String *fqn, String *child_fqn)
{
    /* Eliminate the type prefix from fqns for both aname and dname */
    char *parent = strchr(strchr(fqn->value, '.'), '.');
    char *child = strchr(strchr(child_fqn->value, '.'), '.');

    /* dname is a descendant iff, it all characters match up to length
     * of aname */

    assert(parent, "isDescendant: Oops, parent is not defined");
    assert(child, "isDescendant: Oops, child is not defined");

    while (*parent++ == *child++) ;
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
    qsort(vector->vector, vector->elems, sizeof(Object *), &dagnodercmp);
}


/* Remove dependencies to and from node.
 */
static void
removeNodeFromDag(DagNode *node, Hash *allnodes)
{
    Vector *vec = node->dependents;
    DagNode *depnode;
    Object *obj;
    int i;

    assert(node->status == DAGNODE_SORTING,
	   "Incorrect status for node");
    if (vec) {
	for (i = vec->elems-1; i >= 0; i--) {
	    depnode = (DagNode *) dereference(vec->vector[i]);
	    if (depnode->dependencies->elems <= 1) {
		objectFree((Object *) depnode->dependencies, TRUE);
		depnode->dependencies = NULL;
	    }
	    else if (obj = vectorDel(depnode->dependencies, (Object *) node)) {
		objectFree(obj, TRUE);
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

static DagNode *
nextNavigationStep(DagNode *start, DagNode *target)
{
    DagNode *node = NULL;
    DagNode *prev = NULL;

    if (!start) {
	for (node = target; node; node = node->parent) {
	    prev = node;
	}
	return prev;
    }
    return node;
}


static void
addResultNode(Vector *results, DagNode *node)
{
    DagNode *prev = NULL;
    DagNode *navigation = NULL;
    if (results->elems) {
	prev = (DagNode *) results->vector[results->elems - 1];
    }
    while (navigation = nextNavigationStep(prev, node)) {
	prev = navigation;
	dbgSexp(navigation);
    }
    vectorPush(results, (Object *) node);
}

static void
buildInContext(String *context_fqn, Hash *buildlist,
	       Hash *allnodes, Vector *results)
{
    Vector *to_build = (Vector *) hashGet(buildlist, (Object *) context_fqn);
    Object *ref;
    Object *entry;
    DagNode *node;
    int i;
    String *child_context_fqn;

    while (to_build->elems) {
	while (to_build->elems) {
	    reverseSortBuildVector(to_build);
	    ref = vectorPop(to_build);
	    node = (DagNode *) dereference(ref);
	    objectFree((Object *) ref, FALSE);
	    removeNodeFromDag(node, allnodes);
	    addResultNode(results, node);

	    /* Building this node has established a new build context.
	     * Try building under that context. */
	    if (child_context_fqn = get_child_context_node(node, buildlist)) {
		buildInContext(child_context_fqn, buildlist, allnodes, results);
	    }
	}
	get_build_candidates(allnodes, buildlist);
    }
    /* Now remove this context from the buildlist */

    entry = hashDel(buildlist, (Object *) context_fqn);
    objectFree(entry, TRUE);
    return;
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
    get_build_candidates(allnodes, buildlist);

    while (hashElems(buildlist)) {
	context_fqn = get_context_node(buildlist);
	buildInContext(context_fqn, buildlist, allnodes, results);
	get_build_candidates(allnodes, buildlist);
    }
    objectFree((Object *) buildlist, TRUE);
    return results;
}

Vector *
gensort(Document *doc)
{
    Hash *dagnodes = NULL;
    Vector *sorted = NULL;
    BEGIN {
	dagnodes = dagnodesFromDoc(doc);
	identifyDependencies(doc, dagnodes);
	sorted = tsort(dagnodes);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) dagnodes, TRUE);
    }
    END;
    return sorted;
}
