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
 * Provides functions for performing topological sorts.
 */

#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"

#define DEPENDENCIES_STR "dependencies"
#define DEPENDENCY_STR "dependency"
#define DEPENDENCY_SET_STR "dependency-set"

/* Return a statically allocated list containing:
 * ("depnode" "depnode-set")
 */
static Cons *
depnode_list()
{
    static String dependency_str     = {OBJ_STRING, DEPENDENCY_STR};
    static String dependency_set_str = {OBJ_STRING, DEPENDENCY_SET_STR};
    static Cons dependency_set_cons = {
	OBJ_CONS, (Object *) &dependency_set_str, NULL
    };
    static Cons depnode_cons = {
	OBJ_CONS, (Object *) &dependency_str, (Object *) &dependency_set_cons
    };
    return &depnode_cons;
}

#define BUILD_STR "build"
#define DROP_STR "drop"
#define EXISTS_STR "exists"
#define DIFF_STR "diff"

/* Return a list of the prefices to be applied when searching for a
 * dependency of a given build_type.  This is part of the implementation
 * of the dependencies truth table.  More documentation needed.
 * The results are:
 *   build: ("build" "diff" "exists")
 *   drop:  ("drop" "diff" "exists")
 *   diff:  ("build" "drop" "diff" "exists")
 *   diff(old):  ("drop" "build" "diff" "exists")
 */
static Cons *
preficesForBuildType(DagNodeBuildType build_type, boolean is_old)
{
    static String build_string  = {OBJ_STRING, BUILD_STR};
    static String drop_string   = {OBJ_STRING, DROP_STR};
    static String diff_string   = {OBJ_STRING, DIFF_STR};
    static String exists_string = {OBJ_STRING, EXISTS_STR};
    static Cons exists_cons = {
	OBJ_CONS, (Object *) &exists_string, NULL};
    static Cons diff_cons = {
	OBJ_CONS, (Object *) &diff_string, (Object *) &exists_cons};
    static Cons for_build = {
	OBJ_CONS, (Object *) &build_string, (Object *) &diff_cons};
    static Cons for_drop = {
	OBJ_CONS, (Object *) &drop_string, (Object *) &diff_cons};
    static Cons for_diff = {
	OBJ_CONS, (Object *) &build_string, (Object *) &for_drop};
    static Cons for_old_diff = {
	OBJ_CONS, (Object *) &drop_string, (Object *) &for_build};

    switch (build_type) {
    case BUILD_NODE: return &for_build;
    case DROP_NODE: return &for_drop;
    case DIFF_NODE: 
	if (is_old) {
	    return &for_old_diff;
	}
	else {
	    return &for_diff;
	}
    }
    return NULL;
}


/* Predicate identifying whether a node is of a specific type.
 */
static boolean
xmlnodeMatch(xmlNode *node, char *name)
{
    return node && (node->type == XML_ELEMENT_NODE) && streq(node->name, name);
}

static boolean
xmlnodeMatchAny(xmlNode *node, Cons *list)
{
    Cons *next = list;
    if (node && (node->type == XML_ELEMENT_NODE))
    {
	while (next) {
	    if (streq(node->name, ((String *) next->car)->value)) {
		return TRUE;
	    }
	    next = (Cons *) next->cdr;
	}
    }
    return FALSE;
}

/* Find the next (xml document) sibling of the given node type. */
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

/* Find the next (xml document) sibling of the given node type. */
static xmlNode *
findNextMatchingSibling(xmlNode *start, Cons *list)
{
    xmlNode *result = start;
    if (result) {
	while (result = (xmlNode *) result->next) {
	    if (xmlnodeMatchAny(result, list)) {
		return result;
	    }
	}
    }
    return NULL;
}

/* Find the first (xml document) child of the given node type. */
static xmlNode *
findFirstMatchingChild(xmlNode *parent, Cons *list)
{
    if (xmlnodeMatchAny(parent->children, list)) {
	return parent->children;
    }
    return findNextMatchingSibling(parent->children, list);
}

/* Find the first (xml document) child of the given node type. */
static xmlNode *
findFirstChild(xmlNode *parent, char *name)
{
    if (xmlnodeMatch(parent->children, name)) {
	return parent->children;
    }
    return findNextSibling(parent->children, name);
}


/* Concatenate each vararg in turn with fqn, and return the first node
 * from allnodes that has that key.  Eg:
 * findNodeFromOptions(hash, "cluster", "build", "exists", "diff", NULL)
 * will look first for "build.cluster", then "exists.cluster", etc.
 */
static Object *
findNodeFromOptions(Hash *allnodes, String *fqn, ...)
{
    va_list args;
    char *prefix;
    String *key;
    Object *result = NULL;

    va_start(args, fqn);
    while (prefix = va_arg(args, char *)) {
	key = stringNewByRef(newstr("%s.%s", prefix, fqn->value));
	result = hashGet(allnodes, (Object *) key);
	objectFree((Object *) key, TRUE);
	if (result) {
	    break;
	}
    }
    va_end(args);
    return result;
}

/* Identify a matching node for fqn, based on the build_type.  This is
 * not trivial as, if we are building from a diff, we may be creating
 * a child of a new object, or one that has changed, or one that is
 * unchanged, so the parent node may have the prefix, "build",
 * "exists", or "diff". */
static Object *
nodeForBuildType(Hash *hash, String *fqn, DagNodeBuildType build_type)
{
    Object *result;
    switch (build_type) {
    case BUILD_NODE:
    case DIFF_NODE:
    case EXISTS_NODE:
	result = findNodeFromOptions(hash, fqn, 
				     "build", "exists", "diff", NULL);
	break;
    case DROP_NODE:
	result = findNodeFromOptions(hash, fqn, 
				     "drop", "exists", "diff", NULL);
	break;
    default:
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("nodeForBuildType for build type %d is not implemented",
		     build_type));
    }
    if (result) {
	result = (Object *) dereference(((Cons *) result)->car);
    }
    return result;
}

static Cons *
makeCons(Object *obj)
{
    Cons *next;
    Cons *result = NULL;
    ObjReference *ref;
    if (obj->type == OBJ_CONS) {
	next = (Cons *) obj;
	while (next) {
	    ref = objRefNew(dereference(next->car));
	    result = consNew((Object *) ref, (Object *) result);
	    next = (Cons *) next->cdr;
	}
    }
    else {
	ref = objRefNew(dereference(obj));
	result = consNew((Object *) ref, NULL);
    }
    return result;
}

/* Identify a matching node for fqn, based on the build_type.  This is
 * not trivial as, if we are building from a diff, we may be creating
 * a child of a new object, or one that has changed, or one that is
 * unchanged, so the parent node may have the prefix, "build",
 * "exists", or "diff". TODO: improve this comment, mentioning our
 * truth table */
static Cons *
typedNodesFromHash(Hash *hash, String *qn, Cons *prefices)
{
    Object *result;
    Cons *next = prefices;
    String *prefix;
    String *key;
    while (next) {
	prefix = (String *) prefices->car;
	key = stringNewByRef(newstr("%s.%s", prefix->value, qn->value));
	result = hashGet(hash, (Object *) key);
	objectFree((Object *) key, TRUE);
	if (result) {
	    break;
	}
	next = (Cons *) next->cdr;
    }
    
    return makeCons(result);
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


/* A HashEachFn, to record the parent for a dagnode.  This is required
 * primarily for figuring out navigation to and from nodes during a
 * build.  Note that this parentage is quite distinct from any
 * dependencies on parentage - the dependencies will be added later.
 */
static Object *
addParentForNode(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    xmlNode *parent = findAncestor(node->dbobject, "dbobject");
    String *parent_fqn = NULL;
    if (parent) {
	BEGIN {
	    parent_fqn  = nodeAttribute(parent, "fqn");
	    node->parent = (DagNode *) nodeForBuildType((Hash *) dagnodes, 
							parent_fqn, 
							node->build_type);
	}
	EXCEPTION(ex);
	FINALLY {
	    objectFree((Object *) parent_fqn, TRUE);
	}
	END;
    }
    return (Object *) node;
}


/* Record the parentage for each node in dagnodes, by figuring it out
 * from doc.
 */
void
recordParentage(Document *doc, Hash *dagnodes)
{
    //TODO: Rewrite this
    hashEach(dagnodes, &addParentForNode, (Object *) dagnodes);
}

#ifdef nowt
Truth table for deps based on build type (leftmost column) against 
depnodes (top header).  Old dependencies are for diffs where the object
previously depended on something but no longer does.  In that case, if
there is a drop node for the dependency, we create an inverted dependency.
If there is no drop node we do no need to worry.  In general there will 
only be one type of node for each object but, when there are diffs, we
sometimes have to implement the diff as a drop and rebuild.  So, if
there is a diff dependency on the node, we have to convert the diff node
into a drop and rebuild.

		build	drop	diff	exists

build		n       n       n       g
drop    	f       i       i	g
diff    	*       *       n	g
exists		g	g	g	g
old	        f	i	g	g


n - normal dependency
i - inverted
f - fail (should not be possible)
g - ignore (do not make a dep)
* - convert the diff node to a drop and rebuild.
? - builds depend on drops of the same node, but for explicit deps
    a build cannot depend on a drop


The * condition is a problem, because it completely changes the deps tree.
If such a condition is encountered, the whole deps tree will have to be 
dropped, the diff node converted into a drop and rebuild, and then we will
have to try to build the dependencies again.  Nasty.

#endif

static xmlNode *
nextDependencyInSet(xmlNode *node, xmlNode *cur)
{
    if (!cur) {
	return findFirstChild(node, DEPENDENCY_STR);
    }
    else {
	return findNextSibling(cur, DEPENDENCY_STR);
    }
    return NULL;
}

/* Return each dependency or dependency-set of node in turn.  This
 * allows constructions like this to be used:
 * while (dep_node = nextDepnode(node, dep_node))
 */
static xmlNode *
nextDepnode(DagNode *node, xmlNode *cur)
{
    if (!cur) {
	xmlNode *deps_node = findFirstChild(node->dbobject, DEPENDENCIES_STR);
	if (deps_node) {
	    return findFirstMatchingChild(deps_node, depnode_list());
	}
    }
    else {
	return findNextMatchingSibling(cur, depnode_list());
    }
    return NULL;
}

static Depset *
depsetNew(DagNode *owner, xmlNode *dep_node)
{
    Depset *depset = skalloc(sizeof(Depset));
    Cons *list = NULL;
    String *fqn;
    String *pqn;
    xmlNode *next = NULL;

    depset->type = OBJ_DEPSET;
    depset->is_optional = FALSE;
    depset->is_set = FALSE;
    depset->satisfies_depset = FALSE;
    depset->depset_origin = NULL;
    depset->deps = NULL;

    //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    // This function needs to be called to create the depsets only after
    // we have done the hard workd of tracking down the deps.

    if (streq(dep_node->name, DEPENDENCY_STR)) {
	depset->deps = consNew(NULL, NULL);
    }
    else {
	depset->is_set = TRUE;
	while (next = nextDependencyInSet(dep_node, next)) {
	    dbgNode(next);
	    fqn = nodeAttribute(next, "fqn");	
	    pqn = nodeAttribute(next, "pqn");	
	    dbgSexp(fqn);
	    dbgSexp(pqn);
	    objectFree((Object *) fqn, TRUE);
	    objectFree((Object *) pqn, TRUE);
	}
    }

    return depset;
}

/* This deals only with simple (single, not collections of)
 * dependencies. */ 
static Cons *
identifySimpleDeps(DagNode *owner, xmlNode *dep_node, Cons *hashes)
{
    Cons *result = NULL;
    String *fqn = nodeAttribute(dep_node, "fqn");	
    String *pqn = nodeAttribute(dep_node, "pqn");	
    String *old = nodeAttribute(dep_node, "old");
    Cons *prefices = preficesForBuildType(owner->build_type, old && TRUE);

    if (fqn) {
	result = typedNodesFromHash((Hash *) hashes->car, fqn, prefices);
	if (!result) {
	    RAISE(TSORT_ERROR, 
		  newstr("unable to find fqn dependency %s for node %s",
			 owner->fqn->value, fqn));
	}
    }
    else if (pqn) {
	result = typedNodesFromHash((Hash *) hashes->cdr, pqn, prefices);
	if (!result) {
	    RAISE(TSORT_ERROR, 
		  newstr("unable to find pqn dependency %s for node %s",
			 owner->fqn->value, pqn));
	}
    }

    objectFree((Object *) old, TRUE);
    objectFree((Object *) fqn, TRUE);
    objectFree((Object *) pqn, TRUE);
    return result;
}

/* This can deal with a dependency or a dependency-set */
static Cons *
doIdentifyDeps(DagNode *owner, xmlNode *dep_node, Cons *hashes)
{
    Cons *result = NULL;
    Cons *this;
    xmlNode *next = NULL;

    if (streq(dep_node->name, DEPENDENCY_SET_STR)) {
	/* Call recursively to deal with contents of depset */
	while (next = nextDependencyInSet(dep_node, next)) {
	    this = doIdentifyDeps(owner, next, hashes);
	    if (!consIn(result, dereference(this->car))) {
		this->cdr = (Object *) result;
		result = this;
	    }
	    else {
		/* We have a duplicate entry, so ignore it. */
		objectFree((Object *) this, TRUE);
	    }
	}
    }
    else {
	return identifySimpleDeps(owner, dep_node, hashes);
    }
    return result;
}

static Cons *
identifyDeps(DagNode *owner, xmlNode *dep_node, Cons *hashes)
{
    Cons *result = doIdentifyDeps(owner, dep_node, hashes);
    String *old = nodeAttribute(dep_node, "old");
    dbgSexp(old);
    return result;
}

typedef enum {
    DEP_IGNORE = 7,
    DEP_NORMAL,
    DEP_INVERT,
    DEP_BUILD_AND_DROP,
    DEP_ERROR
} DependencyType;

// WORKING ON THIS.  BUT I HAVE FORGOTTEN THAT I HAVE TO BE ABLE TO
// IDENTIFY OLD DEPS, IN MY DEPS LIST.  NEED TO FIX THAT!
/*
static DependencyType depActions[5][5] = {
    {DEP_NORMAL, DEP_NORMAL, DEP_NORMAL, DEP_IGNORE, DEP_ERROR}, 
    {DEP_ERROR, DEP_INVERT, DEP_INVERT, DEP_IGNORE, DEP_ERROR}
};

static DependencyType
getDepType(DagNode *source_node, DagNode *dep_node)
{
TODO: this
}
*/
static void
doAddDeps(DagNode *node, Cons *deps)
{
//TODO: this
    dbgSexp(node);
    dbgSexp(deps);
}

/* Add depsets for all explicitly defined dependencies. */
static void
addExplicitDepsForNode(DagNode *node, Cons *hashes)
{
    xmlNode *dep_node = NULL;
    Depset *depset;
    Cons *deps;

    while (dep_node = nextDepnode(node, dep_node)) {
	deps = identifyDeps(node, dep_node, hashes);
	doAddDeps(node, deps);

	objectFree((Object *) deps, TRUE);
	//depset = depsetNew(node, dep_node);
	//dbgSexp(depset);
	//objectFree((Object *) depset, TRUE);
    }
}


/* A HashEachFn, that identifies and adds dependencies to our DagNode.
 */
static Object *
addDepsForNode(Cons *node_entry, Object *hashes)
{
    DagNode *node = (DagNode *) node_entry->cdr;

    if (node->build_type != EXISTS_NODE) {
	//TODO
	//addImplicitDepsForNode(node);
	addExplicitDepsForNode(node, (Cons *) hashes);
    }
    return (Object *) node;
}


/* Identify all dependencies for our DagNodes, creating and filling in
 * the dependencies dagnode->dependencies and dagnode->dependents
 * vectors as we go.
 */
void
recordDependencies(Document *doc, Hash *dagnodes, Hash *pqnhash)
{
    Cons *hashes = consNew((Object *) dagnodes, (Object *) pqnhash);
    BEGIN {
	hashEach(dagnodes, &addDepsForNode, (Object *) hashes);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) hashes, FALSE);
    }
    END;
}



