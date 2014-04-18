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
 */

/* Resolving dependencies:
 *   Each dbobject in the source document may result in 2 DagNode
 *   entries being created: one for the build side of the DAG and one
 *   for the drop side.  
 */

/* Resolving the DAG:
 *   The process of resolving the DAG removes any cylces, turning it
 *   from a DG into a true DAG.  The process proceeds as follows:
 *   Each dependency, including those on fallbacks, is traversed
 *   regardless of whether it is optional or not.  When a cycle is
 *   detected, the cycle is recorded in a list of cycles.
 *   Once each node in the graph has been resolved, we set about
 *   removing cycles, and then optimising the dependencies to remove any
 *   that are unnecessary.
 */

/* Handling of fallbacks:
 *   A fallback node gives temporary privilege to satisfy a transient
 *   dependency.  Typically this gives superuser privilege to the owner
 *   of an object so that the owner can then create the object or grant
 *   other rights.  Associated with each fallback node is an endfallback
 *   which removes the transient privilege.
 *   As with other nodes, fallbacks and endfallbacks can have mirror
 *   nodes.  In the case of a rebuild operation, the fallback privilege
 *   will be needed on both the build side and the drop side of hte
 *   dependency graph.  Not though that eithe the fallback or its mirror
 *   can be used to satisfy a transient dependency.  Therefor
 *   dependencies on fallbacks may often be satisfied by a dependency
 *   set consisting of both the fallback node and its mirror.
 *   A final wrinkle is that the privilege that the fallback is supposed
 *   to satisfy may already exist on one side of the DAG.
 *
 *   Fallback nodes may be referenced directly from a dependency, or may
 *   be created as the fallback of a dependency-set.  Fallback nodes are
 *   created (with a mode of inactive) as soon as any declaration for
 *   them is encountered (ie when the dependency-set is being parsed).
 *
 *   After the initial reolution pass, we check that each dependency set
 *   has been satisfied.  For any that have not been satisfied,
 *   fallbacks will be created and added to those dependency sets.
 */


#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"


typedef struct ResolverState {
    Vector   *all_nodes;
    Hash     *by_fqn;
    Hash     *by_pqn;
    Vector   *dependency_sets;
    boolean   both_directions;
    Vector   *cycles;
} ResolverState;

void
showDeps(DagNode *node)
{
    if (node) {
        printSexp(stderr, "NODE: ", (Object *) node);
        printSexp(stderr, "-->", (Object *) node->deps);
    }
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


static DagNodeBuildType
mirroredBuildType(DagNodeBuildType type)
{
    switch(type) {
    case REBUILD_NODE: return DROP_NODE;
    default:
        RAISE(TSORT_ERROR,
              newstr("mirrorBuildType: unhandled node build type (%d)", type));
    }
    return type;
}


#ifdef wibble
static DagNodeBuildType
unmirroredBuildType(DagNodeBuildType type)
{
    switch(type) {
    case REBUILD_NODE: return BUILD_NODE;
    default: break;
    }
    return type;
}
#endif


static boolean
mirrorNeeded(DagNodeBuildType type)
{
    return (type == REBUILD_NODE) || (type == DIFF_NODE);
}


static void
makeMirrorNode(DagNode *node, Vector *all_nodes)
{
    DagNode *mirror;
    Dependency *mirrordep;
    mirror = dagNodeNew(node->dbobject, 
			mirroredBuildType(node->build_type));
    mirror->mirror_node = node;
    node->mirror_node = mirror;
    mirror->mirror_node = node;
    mirrordep = dependencyNew(mirror);
    mirrordep->direction = FORWARDS;
    vectorPush(node->deps, (Object *) mirrordep);
    vectorPush(all_nodes, (Object *) mirror);
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
    String *fqn;
    char *errmsg = NULL;
    DagNodeBuildType build_type = UNSPECIFIED_NODE;

    if (nodeHasAttribute(node->node , "fallback")) {
	build_type = INACTIVE_NODE;
    }
    else if (diff = nodeAttribute(node->node, "diff")) {
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
    else if (action = nodeAttribute(node->node, "action")) { 
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
				"build type for node %s", fqn->value);
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
    DagNode *dagnode = NULL;

    BEGIN {
	if (streq((char *) node->node->name, "dbobject")) {
	    dagnode = dagNodeNew(node->node, UNSPECIFIED_NODE);
	    dagnode->build_type = buildTypeForDagNode(node);
	    vectorPush((Vector *) vector, (Object *) dagnode);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) dagnode, TRUE);
    }
    END;

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
    char *errmsg;

    EACH(vector, i) {
	node = (DagNode *) ELEM(vector, i);
	assert(node->type == OBJ_DAGNODE, "Incorrect node type");
	key = stringDup(node->fqn);

	if (old = hashAdd(hash, (Object *) key, (Object *) node)) {
	    errmsg = newstr("hashbyFqn: duplicate node \"%s\"", key->value);
	    objectFree((Object *) hash, FALSE);
	    RAISE(GENERAL_ERROR, errmsg);
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
	pqn = xmlGetProp(node->dbobject, (xmlChar *) "pqn");
	if (pqn) {
	    key = stringNew((char *) pqn);
	    xmlFree(pqn);
	    new = (Object *) objRefNew((Object *) node);
	    if (entry = (Cons *) hashGet(hash, (Object *) key)) {
		consAppend(entry, new);
		objectFree((Object *) key, TRUE);
	    }
	    else {
		entry = consNew(new, NULL);
		hashAdd(hash, (Object *) key, (Object *) entry);
	    }
	}
    }
    return hash;
}

static xmlNode *
nextDependency(xmlNode *start, xmlNode *prev)
{
    xmlNode *node;
    if (prev) {
	node = firstElement(prev->next);
    }
    else {
	node = firstElement(start);
    }
    while (node && !isDepNode(node)) {
	node = firstElement(node->next);
    }
    return node;
}


static Object *
findNodeDep(xmlNode *node, DagNode *dagnode, volatile ResolverState *res_state)
{
    Object *result = NULL;
    String *qn;
    if (qn = nodeAttribute(node, "fqn")) {
	result = hashGet(res_state->by_fqn, (Object *) qn);
	objectFree((Object *) qn, TRUE);
    }
    else if (qn = nodeAttribute(node, "pqn")) {
	result = hashGet(res_state->by_pqn, (Object *) qn);
	objectFree((Object *) qn, TRUE);
    }
    else {
	RAISE(XML_PROCESSING_ERROR, 
	      "No qualified name found in dependency or %s.", 
	      dagnode->fqn->value);
    }
    return result;
}

static void
addDepToTarget(Object *target, Object *dep) 
{
    DependencySet *depset;
    if (target->type == OBJ_DAGNODE) {
	vectorPush(((DagNode *) target)->deps, dep);
    }
    else if (target->type == OBJ_DEPENDENCYSET) {
	assert(dep->type == OBJ_DEPENDENCY, 
	       "Cannot add a non-dependency object to a DependencySet.");
	depset = (DependencySet *) target;
	vectorPush(depset->deps, dep);
	((Dependency *) dep)->depset = depset;
    }
    else {
	RAISE(TSORT_ERROR, 
	      newstr("addDepToTarget: no implementation for target->type %d",
		     (int) target->type));
    }
}

static void
addDepSetToTarget(Object *target, DependencySet *depset) 
{
    int i;
    EACH(depset->deps, i) {
	addDepToTarget(target, ELEM(depset->deps, i));
    }
}

static boolean
containsExistsNode(Object *deps)
{
    if (deps->type == OBJ_DAGNODE) {
	return ((DagNode *) deps)->build_type == EXISTS_NODE;
    }
    else if (deps->type == OBJ_CONS) {
	RAISE(TSORT_ERROR, newstr("Not implemented: cons handling (in containsExistsNode)"));
    }
    else {
	RAISE(TSORT_ERROR, newstr("Not implemented: (in containsExistsNode)"));
    }
    return FALSE;
}


/* Convert DagNodes and lists of DagNodes into Dependencies and
 * DependencySets. 
 */
static Object *
convertToDepType(Object *deps, DependencyApplication direction)
{
    Dependency *dep;
    if (deps->type == OBJ_DAGNODE) {
	dep = dependencyNew((DagNode *) deps);
	dep->direction = direction;
	return (Object *) dep;
    }
    else {
	RAISE(TSORT_ERROR, newstr("Not implemented: (in convertToDepType)"));
	return NULL;
    }
}

String *
directionForDep(xmlNode *node)
{
    String *direction_str = nodeAttribute(node, "direction");
    if (direction_str) {
        stringLowerInPlace(direction_str);
    }
    else {
        if (isDependencies(node->parent)) {
            return directionForDep(node->parent);
        }
    }
    return direction_str;
}

static DependencyApplication
getDependencyDirection(xmlNode *depnode)
{
    String *direction_str = directionForDep(depnode);
    if (direction_str) {
	if (streq(direction_str->value, "forwards")) {
	    return FORWARDS;
	}
	else if (streq(direction_str->value, "backwards")) {
	    return BACKWARDS;
	}
	else {
	    return UNKNOWN_DIRECTION;
	}
    }
    return BOTH_DIRECTIONS;
}

#ifdef wibble
/* 
 * Create a simple fallback node, which we wil place into its own
 * document.  This will then be processed by the fallbacks.xsl script to
 * create a fully formed dbobject which will then be added to our 
 * document and to various hashes and vectors.
 */
static xmlNode *
domakeFallbackNode(String *fqn)
{
    xmlNode *fallback;
    xmlNode *root;
    xmlNode *dbobject;
    xmlDoc *xmldoc;
    Document *doc;
    Document *fallback_processor;

    fallback = xmlNewNode(NULL, BAD_CAST "fallback");
    xmlNewProp(fallback, BAD_CAST "fqn", BAD_CAST fqn->value);
    xmldoc = xmlNewDoc(BAD_CAST "1.0");
    xmlDocSetRootElement(xmldoc, fallback);
    doc = documentNew(xmldoc, NULL);

    BEGIN {
	docStackPush(doc);
	fallback_processor = getFallbackProcessor();
	applyXSL(fallback_processor);
    }
    EXCEPTION(ex) {
	doc = docStackPop();
	objectFree((Object *) doc, TRUE);
	RAISE();
    }
    END;
    doc = docStackPop();
    root = xmlDocGetRootElement(doc->doc);
    dbobject = xmlCopyNode(getElement(root->children), 1);
    objectFree((Object *) doc, TRUE);
    if (!dbobject) {
	RAISE(TSORT_ERROR, newstr("Failed to create fallback for %s.",
				  fqn->value));
    }
    return dbobject;
}
#endif

#ifdef wibble
static xmlNode *
firstDbobject(xmlNode *node)
{
    xmlNode *this = getElement(node->children);
    xmlNode *result;

    /* Search this level first. */
    while (this) {
	if (streq("dbobject", (char *) this->name)) {
	    return this;
	}
	this = getElement(this->next);
    }

    /* Nothing found, try recursing */
    this = getElement(node->children);
    while (this) {
	if (result = firstDbobject(this)) {
	    return result;
	}
	this = getElement(this->next);
    }
    return NULL;
}

static xmlNode *
getParentByXPath(xmlNode *node, char *expr)
{
    xmlDoc *xmldoc = node->doc;
    Document *doc = xmldoc->_private;
    xmlXPathObject *obj;
    xmlNodeSet *nodeset;
    xmlNode *result = NULL;

    if (doc) {
	if (obj = xpathEval(doc, node, expr)) {
	    nodeset = obj->nodesetval;
	    if (nodeset && nodeset->nodeNr) {
		result = nodeset->nodeTab[0];
	    }
	}
	xmlXPathFreeObject(obj);
    }
    return result;
}

static xmlNode *
getRootNode(xmlNode *dep_node)
{
    xmlNode *root;
    assert(dep_node->doc, "No document for dep_node");
    assert(dep_node->doc->children, "No root node dep_node->doc");
    root = dep_node->doc->children;
    return firstDbobject(root);
}

static DagNode *
parentNodeForFallback(xmlNode *dep_node, Hash *byfqn)
{
    String *parent = nodeAttribute(dep_node, "parent");
    xmlNode *node = NULL;
    DagNode *result;
    String *fqn;

    if (parent) {
	node = getParentByXPath(dep_node, parent->value);
	objectFree((Object *) parent, TRUE);
    }
    else {
	node = getRootNode(dep_node);
    }
    if (!node) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("No root node found for fallback"));
    }
    fqn = nodeAttribute(node, "fqn");
    result = (DagNode *) hashGet(byfqn, (Object *) fqn);
    objectFree((Object *) fqn, TRUE);
    return result;
}

static void
setParent(DagNode *node, DagNode *parent)
{
    xmlNode *xmlnode = node->dbobject;
    xmlNode *xmlparent = parent->dbobject;
    node->parent = parent;
    xmlAddChild(xmlparent, xmlnode);
}
#endif

static void 
recordNodeDeps(DagNode *node, volatile ResolverState *res_state);

#ifdef wibble
static DagNode *
makeFallbackNode(String *fqn, xmlNode *depnode, 
		 volatile ResolverState *res_state)
{
    xmlNode *dbobj;
    DagNode *fallback;
    DagNode *parent;

    dbobj = domakeFallbackNode(fqn);
    fallback = dagNodeNew(dbobj, INACTIVE_NODE);
    parent = parentNodeForFallback(depnode, res_state->by_fqn);
    setParent(fallback, parent);
    hashAdd(res_state->by_fqn, (Object *) fqn, (Object *) fallback);
    vectorPush(res_state->all_nodes, (Object *) fallback);
    fallback->deps = vectorNew(10);
    recordNodeDeps(fallback, res_state);

    return fallback;
}
#endif

static Dependency *
copyDependency(Dependency *dep)
{
    Dependency *result = dependencyNew(dep->dep);
    if (dep->depset) {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("copyDependency: copy of deps in depsets not "
		     "implemented"));
    }
    result->direction = dep->direction;
    return result;
}

static DagNode *
makeFallbackCopy(DagNode *fallback, boolean endfallback, boolean mirror,
		 volatile ResolverState *res_state)
{
    DagNode *new = dagNodeNew(fallback->dbobject, 
			      endfallback? ENDFALLBACK_NODE: FALLBACK_NODE);
    char *fqn;
    Dependency *newdep;
    int i;

    fqn = newstr("%s%s%s", mirror? "ds": "", endfallback? "end": "",
		 new->fqn->value);
    skfree(new->fqn->value);
    new->fqn->value = fqn;
    new->deps = vectorNew(10);
    new->parent = fallback->parent;

    EACH(fallback->deps, i) {
	newdep = copyDependency((Dependency *) ELEM(fallback->deps, i));
	vectorPush(new->deps, (Object *) newdep);
    }

    if (endfallback) {
	/* Add a dependency from endfallback to fallback. */
	newdep = dependencyNew(fallback);
	newdep->direction = BOTH_DIRECTIONS;
	newdep->immutable = TRUE;
	vectorPush(new->deps, (Object *) newdep);
    }

    vectorPush(res_state->all_nodes, (Object *) new);
    return new;
}

/* Activate a fallback node.  If the fallback is inactive, we convert it
 * into an active fallback, also creating the endfallback and (possibly)
 * mirrors.
 */
static DagNode *
activateFallback(DependencySet *depset, volatile ResolverState *res_state)
{
    DagNode *fallback;
    DagNode *endfallback;
    Dependency *newdep;
    DagNode *dsfallback;
    DagNode *dsendfallback;

    if (fallback = depset->fallback) {
	if (fallback->build_type == INACTIVE_NODE) {
	    /* Make end fallback. */
	    endfallback = makeFallbackCopy(fallback, TRUE, FALSE, res_state);
	    fallback->endfallback = endfallback;

	    if (res_state->both_directions) {
		/* Make mirrors. */
		dsfallback = makeFallbackCopy(fallback, FALSE, TRUE, res_state);
		dsendfallback = makeFallbackCopy(fallback, TRUE, TRUE, 
						 res_state);
		dsfallback->endfallback = dsendfallback;

		fallback->mirror_node = dsfallback;
		endfallback->mirror_node = dsendfallback;

		newdep = dependencyNew(dsendfallback);
		vectorPush(fallback->deps, (Object *) newdep);
	    }
	    fallback->build_type = FALLBACK_NODE;
	}
	return fallback;
    }

    return NULL;
}


/* Rules for adding dependencies
  Each item in deps is either a dependency-set or a dependency

  - If the node is a rebuild or diffnode, we must create a mirror
  - If the node is a drop node, deps are added only to the mirror
  - Nodes will be added to either the node or the mirror depending
    on direction conditionality.
  - Dependencies on EXISTS_NODES will be deemed to be always satisfied
    and will be dropped
  - Ditto for dependency sets containing a dependency on an exists node.
  - Promotion to rebuild node only happens to diff nodes and must be
    done as a separate pass.
 */
static DagNode *
getFallbackNode(xmlNode *depnode, volatile ResolverState *res_state)
{
    String *fallback = NULL;
    DagNode *fallback_node = NULL;

    if (fallback = nodeAttribute(depnode, "fallback")) {
	fallback_node = (DagNode *) hashGet(res_state->by_fqn, 
					    (Object *) fallback);
	if (!fallback_node) {
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("getFallbackNode: not implemented - new fallback creation."));
	}
	else {
	    objectFree((Object *) fallback, TRUE);
	}
    }
    return fallback_node;
}

static void
recordDepElement(DagNode *node, xmlNode *depnode, 
		 volatile ResolverState *res_state, Object *target, 
		 boolean *p_contains_exists, boolean required)
{
    xmlNode *this = NULL;
    Object *deps = NULL;
    char *tmp;
    char *errmsg;
    DagNode *fallback;
    DependencySet *depset;
    Dependency *dep;
    DependencyApplication direction;
    boolean contains_exists = FALSE;

    if (isDependencies(depnode)) {
        while (this = nextDependency(depnode->children, this)) {
            recordDepElement(node, this, res_state, target, NULL, TRUE);
        }
    }
    else if (isDependency(depnode)) {
        if (deps = findNodeDep(depnode, node, res_state)) {
	    if (containsExistsNode(deps)) {
		deps = NULL;
	    }
	    else {
		direction = getDependencyDirection(depnode);
		deps = convertToDepType(deps, direction);
	    }
	}
	else {
	    /* No node found for dependency entry. */
	    if (required) {
		tmp = nodestr(depnode);
		errmsg = newstr("recordDepElement: required "
				"dependency not found for: %s", tmp);
		skfree(tmp);
		RAISE(TSORT_ERROR, errmsg);
	    }
	}
    }
    else if (isDependencySet(depnode)) {
	depset = dependencySetNew();
	deps = (Object *) depset;
	vectorPush(res_state->dependency_sets, deps);

	depset->fallback = getFallbackNode(depnode, res_state);

        while (this = nextDependency(depnode->children, this)) {
            recordDepElement(node, this, res_state, (Object *) depset, 
			     &contains_exists, FALSE);
        }
	if (depset->deps->elems == 0) {
	    if (fallback = activateFallback(depset, res_state)) {
		dep = dependencyNew(fallback);
		dep->direction = BOTH_DIRECTIONS;
		/* Add fallback to current depset. */
		addDepToTarget((Object *) depset, (Object *) dep);
		/* Add fallback to current node. */
		dep = dependencyNew(fallback);
		dep->direction = BOTH_DIRECTIONS;
		dep->immutable = TRUE;
		addDepToTarget((Object *) node, (Object *) dep);
		/* Add node to endfallback's deps. */
		dep = dependencyNew(node);
		dep->direction = BOTH_DIRECTIONS;
		dep->immutable = TRUE;
		addDepToTarget((Object *) fallback->endfallback,
			       (Object *) dep);
	    }
	    else {
		RAISE(TSORT_ERROR, 
		      newstr("No dependency found and no fallback specified "
			     "for dependency-set in %s.", node->fqn->value));
	    }
	}
	if (contains_exists) {
	    RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("recordDepElement: depset containing EXISTS_NODE"
		     "not implemented."));
	}
    }
    else {
	tmp = nodestr(depnode);
	errmsg = newstr("Invalid dependency type: %s", tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
    if (deps) {
	if (deps->type == OBJ_DEPENDENCY) {
	    addDepToTarget(target, deps);
	}
	else if (deps->type == OBJ_DEPENDENCYSET) {
	    addDepSetToTarget(target, (DependencySet *) deps);
	}
	else {
	    dbgSexp(deps);
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("recordDepElement: no implementation "
			 "for deps->type %d", (int) deps->type));
	}
    }
}


static void 
recordNodeDeps(DagNode *node, volatile ResolverState *res_state)
{
    xmlNode *depnode = NULL;
    while (depnode = nextDependency(node->dbobject->children, depnode)) {
	recordDepElement(node, depnode, res_state, (Object *) node, NULL, TRUE);
    }
}


static void
identifyDependencies(Vector *nodes, volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (!node->deps) {
	    node->deps = vectorNew(10);
	    recordNodeDeps(node, res_state);
	}
    }
}

static void
makeMirrors(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if ((!node->mirror_node) && mirrorNeeded(node->build_type)) {
	    /* Note that the unmirrored part of the mirrored pair
	     * retains its original build_type.  This is so that
	     * later we can more easily promote nodes that depend on
	     * rebuild  nodes.  */
	    makeMirrorNode(node, nodes);
	}
    }
}

typedef enum {
    RETAIN, INVERT, DUPANDINVERT, 
    DROP, ERROR
} DepTransform;

static DepTransform transform_for_build_types
    [EXISTS_NODE][EXISTS_NODE] = 
{
    /* BUILD */ 
    {RETAIN, ERROR, ERROR,       /* BUILD, DROP  REBUILD, */
     ERROR, RETAIN, ERROR},      /* DIFF, FALLBACK, ENDFALLBACK */
    /* DROP */ 
    {ERROR, INVERT, ERROR,       /* BUILD, DROP  REBUILD, */
     ERROR, RETAIN, ERROR},      /* DIFF, FALLBACK, ENDFALLBACK */
    /* REBUILD */ 
    {ERROR, RETAIN, DUPANDINVERT,   /* BUILD, DROP  REBUILD, */
     ERROR, ERROR, ERROR},         /* DIFF, FALLBACK, ENDFALLBACK */
    /* DIFF */ 
    {ERROR, ERROR, ERROR,       /* BUILD, DROP  REBUILD, */
     ERROR, ERROR, ERROR},      /* DIFF, FALLBACK, ENDFALLBACK */
    /* FALLBACK */ 
    {RETAIN, INVERT, ERROR,       /* BUILD, DROP  REBUILD, */
     ERROR, ERROR, ERROR},      /* DIFF, FALLBACK, ENDFALLBACK */
    /* ENDFALLBACK */ 
    {RETAIN, INVERT, ERROR,       /* BUILD, DROP  REBUILD, */
     ERROR, RETAIN, ERROR}       /* DIFF, FALLBACK, ENDFALLBACK */
};

/* Notes:
 *   REBUILD->DROP 
 *    This is actually the dependency from the build side of a rebuild
 *    to the drop side.
 */

static DepTransform
transformForDep(DagNode *node, Dependency *dep)
{
    DepTransform transform;
    DagNode *depnode;

    assert(dep->type == OBJ_DEPENDENCY,
	  "transformForDep: unexpected dep->type: %d.", dep->type);
    depnode = dep->dep;

    if ((node->build_type == EXISTS_NODE) ||
	(node->build_type == INACTIVE_NODE) ||
	(depnode->build_type == INACTIVE_NODE) ||
	(depnode->build_type == EXISTS_NODE)) 
    {
	transform = DROP;
    }
    else {
	transform = 
	  transform_for_build_types[node->build_type][depnode->build_type];

	if (transform == ERROR) {
	    dbgSexp(node);
	    dbgSexp(depnode);
	    showDeps(node);

	    if (depnode->endfallback) {
		showDeps(depnode);
		showDeps(depnode->endfallback);
	    }

	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("transformForDep: no handler for deps "
			 "from build_type %d to build_type %d", 
			 node->build_type, depnode->build_type));
	}
    }
    return transform;
}

/* Like vectorPush but creates the vector if needed. 
 */
static void
myVectorPush(Vector **p_vector, Object *obj)
{
    if (!*p_vector) {
	*p_vector = vectorNew(10);
    }
    vectorPush(*p_vector, obj);
}

/* Copy dep into node->tmp_deps or drop it if it is not needed.
 */
static void
tmpifyNodeDep(DagNode *node, Dependency *dep)
{
    assert(dep->type == OBJ_DEPENDENCY,
	   "dep must be of type OBJ_DEPENDENCY but is %d", dep->type);
    if ((dep->direction == FORWARDS) ||
	(dep->direction == BOTH_DIRECTIONS)) 
    {
	myVectorPush(&(node->tmp_deps), (Object *) dep);
    }
    else {
	objectFree((Object *) dep, TRUE);
    }
}

static void
dropNodeDep(DagNode *node, Dependency *dep)
{
    assert(dep->type == OBJ_DEPENDENCY,
	   "dep must be of type OBJ_DEPENDENCY but is %d", dep->type);

    objectFree((Object *) dep, TRUE);
}

static void
invertNodeDep(DagNode *node, Dependency *dep)
{
    DagNode *depnode;
    if (dep->type == OBJ_DEPENDENCY) {
	if ((dep->direction == BACKWARDS) ||
	    (dep->direction == BOTH_DIRECTIONS)) 
	{
	    depnode = dep->dep;
	    if (dep->immutable) {
		/* For immutable dependencies, we do not invert the
		 * dependency direction. */
		myVectorPush(&(node->tmp_deps), (Object *) dep);
	    }
	    else {
		dep->dep = node;
		myVectorPush(&(depnode->tmp_deps), (Object *) dep);
	    }
	}
	else {
	    objectFree((Object *) dep, TRUE);
	}
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("invertNodeDep: no handler for deps of type %d",
		     dep->type));
    }
}

static DagNode *
mirrorOrThis(DagNode *this)
{
    if (this->mirror_node) {
	return this->mirror_node;
    }
    return this;
}

static Dependency *
makeInvertedDep(Dependency *dep, volatile ResolverState *res_state)
{
    Dependency *dup = dependencyNew(mirrorOrThis(dep->dep));
    DependencySet *dup_depset;
    dup->direction = dep->direction;
    if (dep->depset) {
	if (!(dup_depset = dep->depset->mirror)) {
	    dup_depset = dependencySetNew();
	    dup_depset->is_temporary = dep->depset->is_temporary;
	    dup_depset->degrade_if_missing = dep->depset->degrade_if_missing;
	    dep->depset->mirror = dup_depset;
	    vectorPush(res_state->dependency_sets, (Object *) dup_depset);
	}
	dup->depset = dup_depset;
	vectorPush(dup_depset->deps, (Object *) dup);
    }
    return dup;
}

static void
dupAndInvertNodeDep(
    DagNode *node, Dependency *dep, volatile ResolverState *res_state)
{
    Dependency *dup;

    if (dep->type == OBJ_DEPENDENCY) {
	dup = makeInvertedDep(dep, res_state);
	dup->direction = dep->direction;
	tmpifyNodeDep(node, dep);
	invertNodeDep(node->mirror_node, dup);
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("dupAndInvertNodeDep: no handler for deps of type %d",
		     dep->type));
    }
}

static void
redirectNodeDeps(DagNode *node, volatile ResolverState *res_state)
{
    Dependency *dep;
    int i;
    DepTransform transform;

    EACH(node->deps, i) {
	dep = (Dependency *) ELEM(node->deps, i);
	transform = transformForDep(node, dep);
	switch (transform) {
	case RETAIN: 
	    tmpifyNodeDep(node, dep);
	    break;
	case INVERT:
	    invertNodeDep(node, dep);
	    break;
	case DUPANDINVERT:
	    dupAndInvertNodeDep(node, dep, res_state);
	    break;
	case DROP:
	    dropNodeDep(node, dep);
	    break;
	default: 
	    dbgSexp(node);
	    dbgSexp(dep);
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("redirectNodeDeps: unhandled transform %d",
			 transform));
	}
	ELEM(node->deps, i) = NULL;
    }
    objectFree((Object *) node->deps, FALSE);
    node->deps = NULL;
}

static void
redirectDependencies(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	redirectNodeDeps(node, res_state);
    }
    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	node->deps = node->tmp_deps;
	node->tmp_deps = NULL;
    }
}

static void
convertDependencies(Vector *nodes)
{
    DagNode *node;
    int i;
    Dependency *dep;
    int j;
    Vector *newvec;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (node->deps) {
	    newvec = vectorNew(node->deps->elems);
	    EACH(node->deps, j) {
		dep = (Dependency *) ELEM(node->deps, j);
		assert(dep->type == OBJ_DEPENDENCY,
		       "Invalid_object type");

		setPush(newvec, (Object *) dep->dep);
		objectFree((Object *) dep, TRUE);
	    }
	    objectFree((Object *) node->deps, FALSE);
	    node->deps = newvec;
	}
    }
}

typedef enum {
    ALL_IS_WELL = 0,
    CYCLE_DETECTED
} resolver_status;

static void
recordCycle(DagNode *cycle_node, volatile ResolverState *res_state)
{
    DagNode *this = cycle_node;
    Dependency *dep;
    Vector *cycle = vectorNew(10);

    if (!res_state->cycles) {
	res_state->cycles = vectorNew(10);
    }
    vectorPush(res_state->cycles, (Object *) cycle);
    do {
	dep = (Dependency *) ELEM(this->deps, this->cur_dep);
	vectorPush(cycle, (Object *) dep);
	this = dep->dep;
    } while (this != cycle_node);
}

static resolver_status
resolveNode(DagNode *node, volatile ResolverState *res_state);

static void 
resolveDeps(DagNode *node, volatile ResolverState *res_state)
{
    int i;
    Dependency *dep;
    resolver_status status;

    EACH (node->deps, i) {
	dep = (Dependency *) ELEM(node->deps, i);
	node->cur_dep = i;
	status = resolveNode(dep->dep, res_state);
	if (status == CYCLE_DETECTED) {
	    recordCycle(dep->dep, res_state);
	}
    }
}

static resolver_status
resolveNode(DagNode *node, volatile ResolverState *res_state)
{
    switch (node->status) {
    case VISITED:
	/* This node has already been resolved, so nohing more to do. */
	return ALL_IS_WELL;
    case VISITING:
	return CYCLE_DETECTED;
    case UNVISITED:
	node->status = VISITING;
	resolveDeps(node, res_state);
	node->status = VISITED;
	return ALL_IS_WELL;
    default: 
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
	return ALL_IS_WELL;
    }
}

static
void
optimiseDepsets(volatile ResolverState *res_state)
{
    DependencySet *depset;
    int i;
    Dependency *dep;
    int j;
    boolean found = FALSE;

    EACH(res_state->dependency_sets, i) {
	depset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	if (depset->deps->elems > 1) {
	    found = FALSE;
	    EACH(depset->deps, j) {
		dep = (Dependency *) ELEM(depset->deps, j);
		if (!dep->deactivated) {
		    if (found) {
			dep->deactivated = TRUE;
		    }
		    else {
			found = TRUE;
		    }
		}
	    }
	}
    }
}

static boolean
isCycle(Vector *cycle)
{
    int i;
    Dependency *dep;
    EACH(cycle, i) {
	dep = (Dependency *) ELEM(cycle, i);
	if (dep->deactivated) {
	    return FALSE;
	}
    }
    return TRUE;
}


static void
freeCycles(volatile ResolverState *res_state)
{
    int i;

    EACH(res_state->cycles, i) {
	objectFree(ELEM(res_state->cycles, i), FALSE);
    }
    objectFree((Object *) res_state->cycles, FALSE);
    res_state->cycles = NULL;
}

static boolean
canDeactivate(Dependency *dep)
{
    char *tmp;
    char *errmsg;
    if (dep->depset) {
	if (dep->deactivated) {
	    tmp = objectSexp((Object *) dep);
	    errmsg = newstr("canDeactivate: dep is already deactivated - %s",
			    tmp);
	    skfree(tmp);
	    RAISE(TSORT_ERROR, errmsg);
	}
	return dep->depset->chosen_dep == NULL;
    }
    return FALSE;
}

static void
deactivateDep(Dependency *dep)
{
    char *tmp;
    char *errmsg;
    DependencySet *depset;
    Dependency *dep2;
    int i;
    int active_count = 0;

    if (!(depset = dep->depset)) {
	tmp = objectSexp((Object *) dep);
	errmsg = newstr("deactivateDep: dep is not in a depset %s", tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
    if (dep->deactivated) {
	tmp = objectSexp((Object *) dep);
	errmsg = newstr("deactivateDep: dep is already deactivated - %s",
			tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
    dep->deactivated = TRUE;
    EACH(depset->deps, i) {
	dep2 = (Dependency *) ELEM(depset->deps, i);
	if (!dep2->deactivated) {
	    depset->chosen_dep = dep2;
	    active_count++;
	}
    }
    if (active_count != 1) {
	depset->chosen_dep = NULL;
    }
}

static void
reactivateDep(Dependency *dep)
{
    char *tmp;
    char *errmsg;
    if (!dep->depset) {
	tmp = objectSexp((Object *) dep);
	errmsg = newstr("reactivateDep: dep is not in a depset %s", tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
    if (!dep->deactivated) {
	tmp = objectSexp((Object *) dep);
	errmsg = newstr("reactivateDep: dep is not deactivated - %s",
			tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
    dep->depset->chosen_dep = NULL;
    dep->deactivated = FALSE;
}

static boolean
resolveCycles(volatile ResolverState *res_state, int cycle)
{
    int i;
    Dependency *dep;
    Vector *this_cycle;

    if (cycle >= res_state->cycles->elems) {
//	fprintf(stderr, "No cycle %d\n", cycle);
	return TRUE;
    }

    while (!isCycle((Vector *) ELEM(res_state->cycles, cycle))) {
	cycle++;
    }
    this_cycle = (Vector *) ELEM(res_state->cycles, cycle);
//    fprintf(stderr, "\nCycle %d: ", cycle);
//    dbgSexp(this_cycle);
    EACH(this_cycle, i) {
	dep = (Dependency *) ELEM(this_cycle, i);
	if (canDeactivate(dep)) {
//	    fprintf(stderr, "\nIn cycle %d, trying dep %d (%s)\n", 
//		    cycle, i, dep->dep->fqn->value);
	    deactivateDep(dep);
	    if (resolveCycles(res_state, cycle + 1)) {
//		fprintf(stderr, "Success for cycle %d\n", cycle);
		return TRUE;
	    }
	    reactivateDep(dep);
//	    fprintf(stderr, "No joy at %d.%d\n", cycle, i);
	}
//	else {
//	    fprintf(stderr, "\nIn cycle %d, skipping dep %d (%s)\n", 
//		    cycle, i, dep->dep->fqn->value);
//	}
    }

    return FALSE;
}

#ifdef wibble
static char *
describeCycles(volatile ResolverState *res_state)
{
    Vector *cycle;
    int i;
    
    EACH(res_state->cycles, i) {
	cycle = (Vector *) ELEM(res_state->cycles, i);
//	dbgSexp(cycle);
    }
    return NULL;
}
#endif

/* This traverses the DAG, picking one dep from each depset (
 * optionally creating fallbacks), and adding cycle breakers when
 * necessary.
 */
static void
resolveNodes(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	(void) resolveNode(node, res_state);
    }
    if (res_state->cycles) {
//	dbgSexp(res_state->cycles);
	if (!resolveCycles(res_state, 0)) {
//	    describeCycles(res_state);
	    RAISE(TSORT_ERROR,
		  newstr("resolveNodes: unable to resolve cycles"));
	}
	freeCycles(res_state);
    }
}

static void
freeNodeDeps(DagNode *node)
{
    int i;
    Object *obj;
    EACH(node->deps, i) {
	obj = ELEM(node->deps, i);
	if (obj && (obj->type == OBJ_DEPENDENCY)) {
	    objectFree(obj, TRUE);
	}
    }
}

static void
freeDeps(Vector *nodes)
{
    int i;
    DagNode *node;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	freeNodeDeps(node);
    }
}

static void
initResolverState(volatile ResolverState *res_state, Document *doc)
{
    xmlXPathObject *obj;
    xmlNodeSet *nodeset;

    res_state->all_nodes = NULL;
    res_state->by_fqn= NULL;
    res_state->by_pqn = NULL;
    res_state->dependency_sets = NULL;
    res_state->both_directions = FALSE;

    if (dereference(symbolGetValue("build")) &&
	dereference(symbolGetValue("drop"))) {
	res_state->both_directions = TRUE;
    }
    else {
	if (obj = xpathEval(doc, xmlDocGetRootElement(doc->doc), 
			    "//dbobject[@diff]")) 
	{
	    nodeset = obj->nodesetval;
	    if (nodeset && nodeset->nodeNr) {
		res_state->both_directions = TRUE;
	    }
	    xmlXPathFreeObject(obj);
	}
    }
    res_state->cycles = NULL;
}

/*
 * Create a Dag from the supplied doc, returning it as a vector of DocNodes.
 * See the file header comment for a more detailed description of what
 * this does.
 */
Vector *
dagFromDoc(Document *doc)
{
    ResolverState volatile resolver_state;
    initResolverState(&resolver_state, doc);
    resolver_state.all_nodes = dagNodesFromDoc(doc);
    resolver_state.dependency_sets = 
	vectorNew(resolver_state.all_nodes->elems * 2);

    BEGIN {
	resolver_state.by_fqn = hashByFqn(resolver_state.all_nodes);
	resolver_state.by_pqn = hashByPqn(resolver_state.all_nodes);
	identifyDependencies(resolver_state.all_nodes, &resolver_state);
	makeMirrors(resolver_state.all_nodes);

	//showVectorDeps(nodes);
	//fprintf(stderr, "---------------------\n\n");

	// TODO: promote rebuilds; remove deps involving an EXISTS_NODE
	redirectDependencies(&resolver_state);
	resolveNodes(&resolver_state);
	optimiseDepsets(&resolver_state);
	convertDependencies(resolver_state.all_nodes);
    }
    EXCEPTION(ex) {
	freeDeps(resolver_state.all_nodes);
	freeCycles(&resolver_state);
	objectFree((Object *) resolver_state.dependency_sets, TRUE);
	objectFree((Object *) resolver_state.all_nodes, TRUE);
	objectFree((Object *) resolver_state.by_fqn, FALSE);
	objectFree((Object *) resolver_state.by_pqn, TRUE);
    }
    END;

    objectFree((Object *) resolver_state.dependency_sets, TRUE);
    objectFree((Object *) resolver_state.by_fqn, FALSE);
    objectFree((Object *) resolver_state.by_pqn, TRUE);
    return resolver_state.all_nodes;
}
