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


void
showDeps(DagNode *node)
{
    printSexp(stderr, "NODE: ", (Object *) node);
    printSexp(stderr, "-->", (Object *) node->deps);
}

static Object *
hashEachShowDeps(Cons *node_entry, Object *dagnodes)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    showDeps(node);
    return (Object *) node;
}

void
showAllDeps(Hash *nodes)
{
    hashEach(nodes, &hashEachShowDeps, (Object *) nodes);
}


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

/* Find the first (xml document) child of the given node type. */
static xmlNode *
findFirstChild(xmlNode *parent, char *name)
{
    if (xmlnodeMatch(parent->children, name)) {
	return parent->children;
    }
    return findNextSibling(parent->children, name);
}

static Cons *
makeCons(Object *obj)
{
    Cons *next;
    Cons *result = NULL;
    ObjReference *ref;
    if (obj) {
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
    }
    return result;
}

/* Identify a matching node for fqn, based on the build_type.  This is
 * not trivial as, if we are building from a diff, we may be creating
 * a child of a new object, or one that has changed, or one that is
 * unchanged, so the parent node may have the prefix, "build",
 * "exists", or "diff". TODO: improve this comment, mentioning our
 * truth table.
 * Note that the result may not be freed as it is the direct object
 * from whichever hash we have been passed. */
static Object *
typedNodesFromHash(Hash *hash, String *qn, Cons *prefices)
{
    Object *result;
    Cons *next = prefices;
    String *prefix;
    String *key;
    while (next) {
	prefix = (String *) next->car;
	key = stringNewByRef(newstr("%s.%s", prefix->value, qn->value));
	result = hashGet(hash, (Object *) key);
	objectFree((Object *) key, TRUE);
	if (result) {
	    break;
	}
	next = (Cons *) next->cdr;
    }
    
    return result;
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
    String *parent_fqn;
    Cons *prefices;
    if (parent) {
	BEGIN {
	    prefices = preficesForBuildType(node->build_type, FALSE);
	    parent_fqn  = nodeAttribute(parent, "fqn");
	    node->parent = (DagNode *) typedNodesFromHash((Hash *) dagnodes, 
							  parent_fqn, prefices);
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
    hashEach(dagnodes, &addParentForNode, (Object *) dagnodes);
}


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

Depset *
depsetNew()
{
    Depset *depset = skalloc(sizeof(Depset));
    Cons *list = NULL;
    String *fqn;
    String *pqn;
    xmlNode *next = NULL;

    depset->type = OBJ_DEPSET;
    depset->is_set = FALSE;
    depset->is_optional = FALSE;
    depset->deps = NULL;
    depset->actual = NULL;

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
    char *errmsg;

    if (fqn) {
	result = makeCons(typedNodesFromHash((Hash *) hashes->car, 
					     fqn, prefices));
	if (!result) {
	    errmsg = newstr("unable to find fqn dependency %s for node %s",
			    fqn->value, owner->fqn->value);
	    objectFree((Object *) fqn, TRUE);
	    RAISE(TSORT_ERROR, errmsg);
	}
    }
    else if (pqn) {
	/* Note that pqns may legitimately not match anything.  This
	 * will happen in dependency sets, where we have "guessed" at
	 * what grants may have been necessary.  Only one of the pqns in
	 * such a dependency set needs to match. */
	result = makeCons(typedNodesFromHash((Hash *) hashes->cdr, 
					     pqn, prefices));
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
    boolean found = FALSE;
    char *errmsg;

    if (streq(dep_node->name, DEPENDENCY_SET_STR)) {
	/* Call recursively to deal with contents of depset */
	while (next = nextDependencyInSet(dep_node, next)) {
	    this = doIdentifyDeps(owner, next, hashes);
	    if (this) {
		found = TRUE;
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
	if (!found) {
	    objectFree((Object *) result, TRUE);
	    errmsg = newstr("No dependencies found for dependency set in %s",
			    owner->fqn->value);
	    RAISE(TSORT_ERROR, errmsg);
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
    if (old) {
	result = consNew((Object *) symbolGetValue("t"), (Object *) result);
    }
    else {
	result = consNew(NULL, (Object *) result);
    }
    objectFree((Object *) old, TRUE);
    return result;
}

/* 
 * Truth table for deps based on build type (leftmost column) against
 * depnodes (top header).  Old dependencies are for diffs where the
 * object previously depended on something but no longer does.  In that
 * case, if there is a drop node for the dependency, we create an
 * inverted dependency.  If there is no drop node we do no need to
 * worry.  In general there will only be one type of node for each
 * object but, when there are diffs, we sometimes have to implement the
 * diff as a drop and rebuild.  So, if there is a diff dependency on the
 * node, we have to convert the diff node into a drop and rebuild.
 * 
 * 		build	drop	diff	exists
 * 
 * build		n       n       n       g
 * drop    	f       i       i	g
 * diff    	*       *       n	g
 * exists		g	g	g	g
 * old	        f	i	i	g
 * 
 * 
 * n - normal dependency
 * i - inverted
 * f - fail (should not be possible)
 * g - ignore (do not make a dep)
 * * - convert the diff node to a drop and rebuild.
 * ? - builds depend on drops of the same node, but for explicit deps
 *     a build cannot depend on a drop
 * 
 * 
 * The * condition is a problem, because it completely changes the deps
 * tree.  If such a condition is encountered, the whole deps tree will
 * have to be dropped, the diff node converted into a drop and rebuild,
 * and then we will have to try to build the dependencies again.  Nasty.
 */ 

typedef enum {
    DEP_NORMAL = 7,
    DEP_IGNORE,
    DEP_INVERT
} DependencyType;

static DependencyType
getDepType(DagNode *source_node, DagNode *dep_node, boolean old)
{
    if (dep_node->build_type == EXISTS_NODE) {
	return DEP_IGNORE;
    }
    switch (source_node->build_type) {
    case BUILD_NODE:
	return DEP_NORMAL;
    case DROP_NODE:
	if (dep_node->build_type == BUILD_NODE) {
	    RAISE(TSORT_ERROR, 
		  newstr("getDepType: drops (%s) cannot depend on builds (%s)",
			 source_node->fqn->value, dep_node->fqn->value));
	}
	return DEP_INVERT;
    case DIFF_NODE:
	if (old) {
	    /* An old dep, is a dep that, after the diff is performed,
	     * will no longer exist.  Eg the dep on a column's type if
	     * the column is being dropped. */
	    if (dep_node->build_type == BUILD_NODE) {
		RAISE(TSORT_ERROR, 
		      newstr("getDepType: old diffs (%s) cannot depend on "
			     "builds (%s)",
			     source_node->fqn->value, dep_node->fqn->value));
	    }
	    return DEP_INVERT;
	}
	else {
	    if (dep_node->build_type != DIFF_NODE) {
		RAISE(NOT_IMPLEMENTED_ERROR, 
		      newstr("getDepType: diff dependencies to non-diffs "
			     "not implemented"));
		/* If there is both a build and a drop for the
		 * dependency, we should convert source_node into a
		 * build and drop pair as well.  This is left as an
		 * exercise for when it is actually needed. */
	    }
	    return DEP_NORMAL;
	}
    }
    RAISE(NOT_IMPLEMENTED_ERROR, 
	  newstr("getDepType: handling of build type %d not implemented.",
		 source_node->build_type));
}

static Depset *
makeDepset(Cons *deps)
{
    Depset *depset = depsetNew();
    depset->is_optional = depset->is_set = deps->cdr && TRUE;
    depset->deps = deps;
    return depset;
}

static void
addDepset(DagNode *node, Depset *depset)
{
    if (depset->is_optional) {
	/* We must add optional dependencies at the start of our deps
	 * list.  This is because of the way that cyclic dependencies
	 * are resolved, particularly when xnodes are added.  See
	 * addInvertedDepSet() for more information. */

	node->deps = consNew((Object *) depset, (Object *) node->deps);
    }
    else {
	node->deps = consAppend(node->deps, (Object *) depset);
    }
}


/* Inverted deps are mostly what they sound like: we simply invert the
 * direction of a dependency.  Dependency sets are a problem however.
 * In the (normal) forward direction of a dependency set, we process a
 * depset by attempting to traverse any of the dependencies in the set.
 * If we detect a cyclic dependency while processing one member, we
 * ignore it and move on to the next.  Only in the case that no member
 * can be processed do we have an invalid (cyclic) dependency problem.
 * When inverting the dependency, we have no obvious algorithm for
 * safely traversing the sets.  Consider the following set of
 * dependencies:
 *
 * a) R1: R3
 * b) R2: R3
 * c) R3: (R1 or R2 or R4)
 * d) R4: (R1 or R2 or R5)
 *
 * Inverting this gives us:
 * e) R3: R1
 * f) R3: R2
 * g) (R1 or R2 or R4): R3
 * h) (R1 or R2 or R5): R4
 *
 * So, it is not possible to state what R1 depends on, and the standard
 * tsort algorithm (even with skit modifications to handle dependency
 * sets) breaks down.
 *
 * I don't know if what follow is original but I have found a solution,
 * which I think works.  I have no proof, but tests are so far bearing
 * out my intuition that this algorithm is sound.
 *
 * When we wish to invert a depset like this R4: (R1 or R2 or R5), we
 * perform the following steps:
 * 1) Create a new dummy node.  I call this an Xnode.
 * 2) Make the originating node a dependency of the Xnode.
 * 3) Add the unmodified depset to the Xnode
 * 4) For each dependency in the depset, add an optional dependency back
 *    to the Xnode.
 * (Optional dependencies are deps that we attempt to traverse but in
 * the event of a cyclic dep being found, we just ignore).
 *
 * Taking the example set (a-d) above, our inverted deps now become:
 *
 *  R1: (R3X or nil) (R4X or nil)
 *  R2: (R3X or nil) (R4X or nil)
 *  R3: R1 R2
 *  R3X: R3 (R1 or R2 or R4)
 *  R4: (R3X or nil)
 *  R4X: R4 (R1 or R2 or R5)
 *  R5: (R4X or nil)
 *
 * The rules for dependency traversal in tsort are:
 * 1) process optional deps first
 * 2) if the visited node is already being visited raise cyclic dep
 *    error
 * 3) catch cyclic dep if traversing optional deps or sets of deps
 * 4) if exception caught in optional dep, ignore the dep
 * 5) if exception caught in dep set, try the next dep: if no more deps,
 *    re-raise the exception.
 */
static void
addInvertedDepSet(DagNode *node, Depset *depset, Hash *dagnodes)
{
    DagNode *xnode;
    DagNode *depnode;
    String *key;
    Cons *next;
    Cons *new_dep;
    Depset *new_depset;

    if (depset->is_set) {
	/* do depset inversion as described in the function header
	 * comment above */

	xnode = xnodeNew(node);
	key = stringNewByRef(newstr("%s", xnode->fqn->value));;
	(void) hashAdd(dagnodes, (Object *) key, (Object *) xnode);
	addDepset(xnode, depset);	/* Add original depset to xnode */
	new_dep = consNew((Object *) 
			  objRefNew((Object *) dereference((Object *) node)), 
			  NULL);
	new_depset = makeDepset(new_dep);
	addDepset(xnode, new_depset); /* Add dep on node to xnode */
	next = (Cons *) depset->deps;
	while (next) {
	    depnode = (DagNode *) dereference(next->car);
	    new_dep = consNew((Object *) 
			      objRefNew((Object *) 
					dereference((Object *) next->car)), 
			      NULL);
	    new_depset = makeDepset(new_dep);
	    new_depset->is_optional = TRUE;
	    addDepset(depnode, new_depset);
	    next = (Cons *) next->cdr;
	}
    }
    else {
	/* Do simple dependency inversion. */
	depnode = (DagNode *) dereference(depset->deps->car);
	new_dep = consNew((Object *) 
			  objRefNew((Object *) dereference((Object *) node)), 
			  NULL);
	objectFree((Object *) depset->deps, TRUE);
	depset->deps = new_dep;
	addDepset(depnode, depset);
    }
}


/* Takes a list of the form, 
 * (boolean . dep...)
 * and adds the appropriate dependency structures before freeing the list.
 */
static void
doAddDeps(DagNode *node, Cons *deps, Cons *hashes)
{
    boolean old = (deps->car) && TRUE;
    Cons *next = (Cons *) deps->cdr;
    DagNode *depnode;
    DependencyType first = DEP_IGNORE;
    DependencyType this;
    Depset *depset;
    char *errmsg;
    char *tmp;

    BEGIN {
	while (next) {
	    depnode = (DagNode *) dereference(next->car);
	    next = (Cons *) next->cdr;
	    this = getDepType(node, depnode, old);
	    if (this == DEP_IGNORE) {
		break;
	    }
	    if (first == DEP_IGNORE) {
		first = this;
	    }
	    else {
		if (this != first) {
		    tmp = objectSexp((Object *) deps);
		    errmsg = newstr("doAddDeps: Incompatible dep "
				    "types in set: %s", tmp);
		    skfree(tmp);
		    RAISE(TSORT_ERROR, errmsg);
		}
	    }
	}
	/* All deps in set are compatible, and the first variable tells
	 * us whether the dep is normal, inverted, or to be ignored. */
	if (first != DEP_IGNORE) {
	    /* Detach the actual deps list from the first (boolean)
	       element. */
	    next = (Cons *) deps->cdr;
	    deps->cdr = NULL;
	    depset = makeDepset(next);
	    if (first == DEP_INVERT) {
		addInvertedDepSet(node, depset, (Hash *) hashes->car);
	    }
	    else {
		addDepset(node, depset);
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) deps, TRUE);
    }
    END;
}

/* Add depsets for all explicitly defined dependencies. */
static void
addExplicitDepsForNode(DagNode *node, Cons *hashes)
{
    xmlNode *dep_node = NULL;
    Depset *depset;
    Cons *deps;

    while (dep_node = nextDepnode(node, dep_node)) {
	if (deps = identifyDeps(node, dep_node, hashes)) {
	    doAddDeps(node, deps, hashes);
	}
    }
}



/* Implicit dependencies are dependencies between parents and children,
 * and the dependency from a build node to any matching drop node.
 */
static void
addImplicitDepsForNode(DagNode *node, Cons *hashes)
{
    Hash *dagnodes;
    Depset *dep;
    char *base_fqn;
    String *dropnode_fqn;
    DagNode *drop_node;

    switch (node->build_type) {
    case BUILD_NODE:
	/* Add a dep to any matching drop node, then fall through to
	 * the next case in the switch to add the parent dep. */
	base_fqn = strchr(node->fqn->value, '.');
	dropnode_fqn = stringNewByRef(newstr("drop%s", base_fqn));
	dagnodes = (Hash *) hashes->car;
	drop_node = (DagNode *) hashGet(dagnodes, (Object *) dropnode_fqn);
	if (drop_node) {
	    dep = makeDepset(makeCons((Object *) drop_node));
	    addDepset(node, dep);
	}
	objectFree((Object *) dropnode_fqn, TRUE);
	/* Deliberately flow through to next case... */
    case DIFF_NODE:
	/* Create a dep on the parent node */
	if (node->parent) {
	    dep = makeDepset(makeCons((Object *) node->parent));
	    addDepset(node, dep);
	}
	break;
    case DROP_NODE:
	/* Need an inverted dep to our parent node */
	if (node->parent) {
	    dep = makeDepset(makeCons((Object *) node));
	    addDepset(node->parent, dep);
	}
	break;
    }
}


/* A HashEachFn, that identifies and adds dependencies to our DagNode.
 */
static Object *
addDepsForNode(Cons *node_entry, Object *hashes)
{
    DagNode *node = (DagNode *) node_entry->cdr;

    if (node->build_type != EXISTS_NODE) {
	addImplicitDepsForNode(node, (Cons *) hashes);
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
	//showAllDeps(dagnodes);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) hashes, FALSE);
    }
    END;
}



