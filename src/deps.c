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
 *   The process of resolving the DAG removes any cycles, turning it
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
 *   dependency graph.  Not though that either the fallback or its mirror
 *   can be used to satisfy a transient dependency.  Therefore
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


/* TODO: CONSIDER FUTURE IMPROVEMENTS:
 * - allow deps to be marked as "degrade-if-missing"
 *   This will allow the build to create commented out code for when we
 *   do not have sufficient privileges.  Note that node degradation will
 *   propagate to nodes on which we are dependent.  There must be
 *   command line options to deal with this, and the --list command
 *   should show degraded nodes.
 * - optimise fallback dependencies where they exist on both sides of
 *   the DAG, but do not need to.
 * - allow deps on fallbacks to be against either side of the DAG
 * - where multiple fallbacks have been invoked, allow the more powerful
 *   feedback (eg superuser) to be used in place of a lesser one if that
 *   means the lesser one can be eliminated entirely.
 *
 */


#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"


typedef struct ResolverState {
    Vector   *all_nodes;
    Hash     *by_fqn;
    Hash     *by_pqn;
    Vector   *dependency_sets;
    Vector   *connection_sets;
    Vector   *cycles;
    Vector   *activated_fallbacks;
    int       idx;
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
    case DIFF_NODE: return DIFFPREP_NODE;
    default:
        RAISE(TSORT_ERROR,
              newstr("mirrorBuildType: unhandled node build type (%d)", type));
    }
    return type;
}


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

    if (diff = nodeAttribute(node->node, "diff")) {
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

String *
directionForDep(xmlNode *node)
{
    String *direction_str = nodeAttribute(node, "direction");
    if (direction_str) {
        stringLowerInPlace(direction_str);
    }
    else {
        if (isDepNode(node->parent)) {
            return directionForDep(node->parent);
        }
    }
    return direction_str;
}

DependencyApplication
dependencyApplicationForString(String *direction)
{
    DependencyApplication result;
    if (direction) {
	if (streq(direction->value, "forwards")) {
	    result = FORWARDS;
	}
	else if (streq(direction->value, "backwards")) {
	    result = BACKWARDS;
	}
	else {
	    result = UNKNOWN_DIRECTION;
	}
	objectFree((Object *) direction, TRUE);
	return result;
    }
    return BOTH_DIRECTIONS;
}

static DependencyApplication
getDependencyDirection(xmlNode *depnode)
{
    return dependencyApplicationForString(directionForDep(depnode));
}

static int
getPriority(xmlNode *node)
{
    String *priority = nodeAttribute(node, "priority");
    int result = 100;   /* 100 is the default priority */

    if (priority) {
	result = atoi(priority->value);
    }
    objectFree((Object *) priority, TRUE);
    return result;
}


/* 
 * Create a simple fallback node, which we wil place into its own
 * document.  This will then be processed by the fallbacks.xsl script to
 * create a fully formed dbobject which will then be added to our 
 * document and to various hashes and vectors.
 */
static xmlNode *
makeXMLFallbackNode(String *fqn)
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
    dbobject = xmlCopyNode(getNextNode(root->children), 1);
    objectFree((Object *) doc, TRUE);
    if (!dbobject) {
	RAISE(TSORT_ERROR, newstr("Failed to create fallback for %s.",
				  fqn->value));
    }
    return dbobject;
}

static xmlNode *
firstDbobject(xmlNode *node)
{
    xmlNode *this = getNextNode(node->children);
    xmlNode *result;

    /* Search this level first. */
    while (this) {
	if (streq("dbobject", (char *) this->name)) {
	    return this;
	}
	this = getNextNode(this->next);
    }

    /* Nothing found, try recursing */
    this = getNextNode(node->children);
    while (this) {
	if (result = firstDbobject(this)) {
	    return result;
	}
	this = getNextNode(this->next);
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

static void 
recordNodeDeps(DagNode *node, volatile ResolverState *res_state);


static DagNode *
newFallbackNode(xmlNode *dbobj, DagNode *parent,
		DagNodeBuildType build_type, char *fqn_prefix,
		volatile ResolverState *res_state)
{
    DagNode *result = dagNodeNew(dbobj, build_type);
    char *newfqn;
    String *key;

    setParent(result, parent);
    result->deps = vectorNew(10);

    if (fqn_prefix) {
	newfqn = newstr("%s%s", fqn_prefix, result->fqn->value);
	skfree(result->fqn->value);
	result->fqn->value = newfqn;
    }

    vectorPush(res_state->all_nodes, (Object *) result);
    recordNodeDeps(result, res_state);
    key = stringNew(result->fqn->value);
    hashAdd(res_state->by_fqn, (Object *) key, (Object *) result);

    return result;
}

static void 
addSimpleDep(DagNode *from, DagNode *to)
{
    Dependency *dep = dependencyNew(to);
    dep->direction = FORWARDS;
    vectorPush(from->deps, (Object *) dep);
}

/* Create all 4 of the set of possibly needed fallback nodes (for build
 * side and drop sides of the DAG, and for creating the fallback
 * privilege and dropping it).
 * Return the build-side fallback node.
 */
static DagNode *
makeFallbackNodes(String *fqn, xmlNode *depnode, 
		  volatile ResolverState *res_state)
{
    xmlNode *dbobj;
    DagNode *fallback;
    DagNode *endfallback;
    DagNode *dsfallback;
    DagNode *dsendfallback;
    DagNode *parent;
    dbobj = makeXMLFallbackNode(fqn);
    parent = parentNodeForFallback(depnode, res_state->by_fqn);
    fallback = newFallbackNode(dbobj, parent, FALLBACK_NODE, NULL, res_state);
    endfallback = newFallbackNode(dbobj, parent, ENDFALLBACK_NODE, 
				  "end", res_state);
    dsfallback = newFallbackNode(dbobj, parent, FALLBACK_NODE, "ds", res_state);
    dsendfallback = newFallbackNode(dbobj, parent, ENDFALLBACK_NODE, 
				    "dsend", res_state);

    addSimpleDep(endfallback, fallback);
    addSimpleDep(fallback, dsendfallback);
    addSimpleDep(dsendfallback, dsfallback);

    fallback->mirror_node = dsfallback;
    dsfallback->mirror_node = fallback;
    endfallback->mirror_node = dsendfallback;
    dsendfallback->mirror_node = endfallback;

    fallback->endfallback = endfallback;
    dsfallback->endfallback = dsendfallback;

    return fallback;
}

static DagNode *
getFallbackNode(xmlNode *depnode, volatile ResolverState *res_state)
{
    String *fallback = NULL;
    DagNode *fallback_node = NULL;

    if (fallback = nodeAttribute(depnode, "fallback")) {
	fallback_node = (DagNode *) hashGet(res_state->by_fqn, 
					    (Object *) fallback);
	if (!fallback_node) {
	    fallback_node = makeFallbackNodes(fallback, depnode, res_state);
	}
	objectFree((Object *) fallback, TRUE);
    }
    return fallback_node;
}

static void
addDepsToVector(Vector *vec, Object *obj, DependencyApplication direction)
{
    Dependency *dep;
    Object *obj2;
    if (obj) {
	if (obj->type == OBJ_DAGNODE) {
	    dep = dependencyNew((DagNode *) obj);
	    dep->direction = direction;
	    vectorPush(vec, (Object *) dep);
	}
	else if (obj->type == OBJ_CONS) {
	    obj2 = dereference(((Cons *) obj)->car);
	    addDepsToVector(vec, obj2, direction);

	    if (obj2 = ((Cons *) obj)->cdr) {
		addDepsToVector(vec, obj2, direction);
	    }
	}
	else {
	    dbgSexp(obj);
	    RAISE(TSORT_ERROR, newstr("addDepsToVector - not implemented."));
	}
    }
    else {
	/* Record the absence of an object - depending on the context,
	 * this may be an error. */
	vectorPush(vec, NULL);
    }
}

static void
recordDepElementNew(DagNode *node, xmlNode *depnode, Vector *vec,
		    volatile ResolverState *res_state)
{
    xmlNode *this = NULL;
    char *tmp;
    char *errmsg;
    Object *deps;
    DependencySet *depset;
    DependencyApplication direction;

    if (isDependencies(depnode)) {   
        while (this = nextDependency(depnode->children, this)) {
            recordDepElementNew(node, this, vec, res_state);
        }
    }
    else if (isDependency(depnode)) {
        deps = findNodeDep(depnode, node, res_state);
	direction = getDependencyDirection(depnode);
	addDepsToVector(vec, deps, direction);
    }
    else if (isDependencySet(depnode)) {
	depset = dependencySetNew();
	depset->fallback = getFallbackNode(depnode, res_state);
	depset->direction = getDependencyDirection(depnode);
	depset->priority = getPriority(depnode);

        while (this = nextDependency(depnode->children, this)) {
            recordDepElementNew(node, this, depset->deps, res_state);
        }
	vectorPush(vec, (Object *) depset);
    }
    else {
	tmp = nodestr(depnode);
	errmsg = newstr("Invalid dependency type: %s", tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
}

static void
removeEmptyDeps(DependencySet *depset)
{
    int from;
    int to = 0;
    Object *obj;
    EACH(depset->deps, from) {
	if (obj = ELEM(depset->deps, from)) {
	    ELEM(depset->deps, to) = obj;
	    to++;
	}
    }
    depset->deps->elems = to;
}


static void
processDepset(DependencySet *depset)
{
    Dependency *dep;
    int i;
    
    if (depset->deps->elems) {
	EACH(depset->deps, i) {
	    if (dep = (Dependency *) ELEM(depset->deps, i)) {
		dep->depset = depset;
		vectorPush(depset->definition_node->deps, (Object *) dep);
	    }
	}
    }
    else {
	if (depset->fallback) {
	    dep = dependencyNew(depset->fallback);
	    dep->direction = depset->direction;
	    vectorPush(depset->definition_node->deps, (Object *) dep);
	}
	else {
	    RAISE(TSORT_ERROR, 
		  newstr("Empty Dependency set found in %s", 
			 depset->definition_node->fqn->value));
	}
    }
}

static void 
recordNodeDeps(DagNode *node, volatile ResolverState *res_state)
{
    xmlNode *depnode = NULL;
    Vector *volatile vec = vectorNew(20);
    int i;
    Object *obj;

    BEGIN {
	while (depnode = nextDependency(node->dbobject->children, depnode)) {
	    recordDepElementNew(node, depnode, vec, res_state);
	    EACH(vec, i) {
		obj = ELEM(vec, i);
		if (obj) {
		    if (obj->type == OBJ_DEPENDENCY) {
			vectorPush(node->deps, obj);
		    }
		    else if (obj->type == OBJ_DEPENDENCYSET) {
			removeEmptyDeps((DependencySet *) obj);
			vectorPush(res_state->dependency_sets, obj);
			((DependencySet *) obj)->definition_node = node;
		    }
		    else {
			dbgSexp(obj);
			RAISE(TSORT_ERROR, 
			      newstr("unhandled dependency type (%d) in "
				     "recordNodeDeps()", obj->type));
		    }
		}
		else {
		    RAISE(TSORT_ERROR, 
			  newstr("No dependency found for dependency %d "
				 "in %s", i, node->fqn->value));
		}
	    }
	    vec->elems = 0;
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) vec, FALSE);
    }
    END;
}

static int
cmpDepset(const void *p1, const void *p2)
{
    DependencySet **p_ds1 = (DependencySet **) p1;
    DependencySet **p_ds2 = (DependencySet **) p2;
    return (*p_ds1)->priority - (*p_ds2)->priority;
}

/* This processes depsets (ie identfies their dependencies) in priority
 * order.  dependency-sets should be prioritised so that some fallbacks
 * will prove to be unnecessary as other fallbacks are activated.
 */
static void
processAllDepsets(Vector *nodes, volatile ResolverState *res_state)
{
    int i;
    DependencySet *depset;
    qsort(res_state->dependency_sets->contents->vector, 
	  res_state->dependency_sets->elems, 
	  sizeof(Vector *), cmpDepset);

    EACH(res_state->dependency_sets, i) {
	depset = (DependencySet *) ELEM(res_state->dependency_sets, i);
	processDepset(depset);
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

    processAllDepsets(nodes, res_state);
}

static boolean
maybePromoteNode(DagNode *node)
{
    int i;
    Dependency *dep;

    if (node->status == UNVISITED) {
	node->status = VISITED;
	if ((node->build_type == EXISTS_NODE) ||
	    (node->build_type == DIFF_NODE))
	{
	    EACH(node->deps, i) {
		dep = (Dependency *) ELEM(node->deps, i);
		if (maybePromoteNode(dep->dep)) {
		    node->build_type = REBUILD_NODE;
		    break;
		}
	    }
	}
    }
    return node->build_type == REBUILD_NODE;
}

/* Reset each node status to unvisited.
 */
static void
resetNodes(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	node->status = UNVISITED;
    }
}

static void
promoteRebuilds(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	(void) maybePromoteNode(node);
    }
    resetNodes(nodes);
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
	     * later we can continue to easily differentiate between
	     * build and rebuild nodes.  */
	    makeMirrorNode(node, nodes);
	}
    }
}

typedef enum {
    RETAIN, INVERT, MIRROR, DUPANDMIRROR, 
    DSFALLBACK, BOTHFALLBACK, IGNORE, FB2REBUILD,
    REBUILD2DROP, MRETAIN, ERROR
} DepTransform;

static DepTransform transform_for_build_types
    [EXISTS_NODE][EXISTS_NODE] = 
{
    /* BUILD */ 
    {RETAIN, INVERT, RETAIN,         	  /* BUILD, DROP  REBUILD, */
     IGNORE, RETAIN, ERROR},         	  /* DIFF, FALLBACK, ENDFALLBACK */
    /* DROP */ 
    {RETAIN, INVERT, MIRROR,     	  /* BUILD, DROP  REBUILD, */
     IGNORE, DSFALLBACK, ERROR},     	  /* DIFF, FALLBACK, ENDFALLBACK */
    /* REBUILD */ 
    {RETAIN, REBUILD2DROP, DUPANDMIRROR,  /* BUILD, DROP  REBUILD, */
     IGNORE, BOTHFALLBACK, ERROR},        /* DIFF, FALLBACK, ENDFALLBACK */
    /* DIFF */ 
    {MRETAIN, INVERT, DUPANDMIRROR,        /* BUILD, DROP  REBUILD, */
     IGNORE, BOTHFALLBACK, ERROR},        /* DIFF, FALLBACK, ENDFALLBACK */
    /* FALLBACK */ 
    {RETAIN, INVERT, FB2REBUILD,          /* BUILD, DROP  REBUILD, */
     IGNORE, ERROR, RETAIN},              /* DIFF, FALLBACK, ENDFALLBACK */
    /* ENDFALLBACK */ 
    {RETAIN, INVERT, FB2REBUILD,          /* BUILD, DROP  REBUILD, */
     IGNORE, RETAIN, ERROR}               /* DIFF, FALLBACK, ENDFALLBACK */
};

/* Notes:
 *   REBUILD->DROP 
 *    This is actually the dependency from the build side of a rebuild
 *    to the drop side.
 *   BUILD->DROP 
 *    This happens when an object is being built abd the privs we need
 *    to build that object (eg usage on a schema) are being dropped.
 *   DROP->BUILD
 *    This happens when an object is being dropped but it optionally
 *    depends on a transient dependency which is being created. 
 */

static DepTransform
transformForDep(DagNode *node, Dependency *dep)
{
    DepTransform transform;
    DagNode *depnode;

    assert(dep->type == OBJ_DEPENDENCY,
	  "transformForDep: unexpected dep->type: %d.", dep->type);
    depnode = dep->dep;
    if (dep->deactivated) {
	transform = IGNORE;
    }
    else if ((node->build_type == EXISTS_NODE) ||
	(depnode->build_type == EXISTS_NODE)) 
    {
	transform = IGNORE;
    }
    else {
	transform = 
	  transform_for_build_types[node->build_type][depnode->build_type];

	if (transform == REBUILD2DROP) {
	    if (dep->dep == node->mirror_node) {
		/* This is a dep from the build element of a rebuild
		 * node to the drop element of the same node.
		 */
		transform = RETAIN;
	    }
	    else {
		transform = MIRROR;
	    }
	}
	if (transform == ERROR) {
	    fprintf(stderr, "\n--error--\n");
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

static DagNode *
mirrorOrThis(DagNode *this)
{
    if (this->mirror_node) {
	return this->mirror_node;
    }
    return this;
}


static boolean
isDropsideFallbackNode(DagNode *node)
{
    if ((node->build_type == FALLBACK_NODE) ||
	(node->build_type == ENDFALLBACK_NODE))
    {
	/* This is a crap way of testing whether this is the drop
	 * side fallback node but it's all I have right now.
	 */
	if (strncmp(node->fqn->value, "ds", 2) == 0) {
	    return TRUE;
	}
    }
    return FALSE;
}

static Dependency *
makeDupDependency(Dependency *dep, boolean make_depset,
		  volatile ResolverState *res_state);

static void
makeDupDepset(DependencySet *depset, volatile ResolverState *res_state)
{
    DependencySet *new = dependencySetNew();
    int i;
    Dependency *dep;

    new->degrade_if_missing = depset->degrade_if_missing;
    new->fallback = depset->fallback;

    EACH(depset->deps, i) {
	dep = (Dependency *) ELEM(depset->deps, i);
	dep = makeDupDependency(dep, FALSE, res_state);
	dep->depset = new;
	vectorPush(new->deps, (Object *) dep);
    }
    vectorPush(res_state->dependency_sets, (Object *) new);
}


static Dependency *
makeDupDependency(Dependency *dep, boolean make_depset,
		  volatile ResolverState *res_state)
{
    if (!dep->dup) {
	if (make_depset && dep->depset) {
	    makeDupDepset(dep->depset, res_state);
	}
	else {
	    dep->dup = dependencyNew(dep->dep);
	    dep->dup->direction = dep->direction;
	}
    }
    return dep->dup;
}

static Dependency *
dupDependency(Dependency *dep, volatile ResolverState *res_state)
{
    return makeDupDependency(dep, TRUE, res_state);
}

static boolean
applyDep(
    DagNode *node,
    Dependency *dep,
    boolean forwards,
    boolean invert_direction,
    boolean use_dep_mirror,
    boolean dep_already_used,
    volatile ResolverState *res_state)
{
    DagNode *depnode;
    assert(dep->type == OBJ_DEPENDENCY,
	   "dep must be of type OBJ_DEPENDENCY but is %d", dep->type);

    if (forwards) {
	if (dep->direction == BACKWARDS) {
	    return dep_already_used;
	}
    }
    else {
	if (dep->direction == FORWARDS) {
	    return dep_already_used;
	}
    }

    if (dep_already_used) {
	dep = dupDependency(dep, res_state);
    }

    if (use_dep_mirror) {
	dep->dep = mirrorOrThis(dep->dep);
    }

    if (invert_direction) {
	depnode = dep->dep;
	dep->dep = node;
	node = depnode;
    }

    myVectorPush(&(node->tmp_deps), (Object *) dep);
    return TRUE;
}

static void
dropDependency(Dependency *dep)
{
    if (dep->depset) {
	/* This is highly pedantic and need only be done in a
	 * development build so that we don't see broken objects when
	 * looking at res_state->dependency_sets. */
	(void) vectorDel(dep->depset->deps, (Object *) dep);
    }
    objectFree((Object *) dep, TRUE);
}

static void
deactivateDepset(DependencySet *depset)
{
    int i;
    Dependency *dep;
    EACH(depset->deps, i) {
	dep = (Dependency *) ELEM(depset->deps, i);
	dep->deactivated = TRUE;
    }
}

/* This takes deps defined for build-side DagNodes and redirects and
 * duplicates them as needed for the specific circumstances applying to
 * the node, and the dependency.  The resulting deps are recorded in the
 * tmp_deps vector from where they will be later moved.  Mirror nodes
 * have already been created at this point.
 */
static void
redirectNodeDeps(DagNode *node, volatile ResolverState *res_state)
{
    Dependency *dep;
    int i;
    DepTransform transform;
    boolean used;

    EACH(node->deps, i) {
	dep = (Dependency *) ELEM(node->deps, i);
	transform = transformForDep(node, dep);

	switch (transform) {
	case BOTHFALLBACK:
	    used = applyDep(node, dep, TRUE, FALSE, FALSE, FALSE, res_state);
	    used = applyDep(node->mirror_node, dep, 
			    FALSE, FALSE, TRUE, used, res_state);
	    break;
	case DSFALLBACK:
	    used = applyDep(node, dep, TRUE, FALSE, TRUE, FALSE, res_state);
	    break;
	case RETAIN: 
	    used = applyDep(node, dep, TRUE, FALSE, FALSE, FALSE, res_state);
	    break;
	case MRETAIN: 
	    used = applyDep(node->mirror_node, dep, 
			    TRUE, FALSE, FALSE, FALSE, res_state);
	    break;
	case INVERT:
	    used = applyDep(node, dep, FALSE, TRUE, FALSE, FALSE, res_state);
	    break;
	case MIRROR:
	    //used = mirrorNodeDep(node, dep, FALSE, res_state);
	    used = applyDep(mirrorOrThis(node), dep, 
			    FALSE, TRUE, TRUE, FALSE, res_state);
	    break;
	case DUPANDMIRROR:
	    used = applyDep(node, dep, TRUE, FALSE, FALSE, FALSE, res_state);
	    used = applyDep(mirrorOrThis(node), dep, 
	    		    FALSE, TRUE, TRUE, used, res_state);
	    break;
	case IGNORE:
	    if (dep->depset) {
		/* The dependency being ignored is part of a
		 * dependency-set.  This means the whole dependency-set
		 * can be ignored. */
		if (!dep->deactivated) {
		    deactivateDepset(dep->depset);
		}
	    }
	    used = FALSE;
	    break;
	case FB2REBUILD:
	    if (isDropsideFallbackNode(node)) {
		dep->dep = dep->dep->mirror_node;
		used = applyDep(node, dep, 
				FALSE, TRUE, FALSE, FALSE, res_state);
	    }
	    else {
		used = applyDep(node, dep, 
				TRUE, FALSE, FALSE, FALSE, res_state);
	    }
	    break;
	default: 
	    dbgSexp(node);
	    dbgSexp(dep);
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("redirectNodeDeps: unhandled transform %d",
			 transform));
	}
	if (!used) {
	    dropDependency(dep);
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

/* This converts the node->deps vector of dependencies into a vector of
 * DagNodes.
 */
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
		if (dep->type != OBJ_DEPENDENCY) {
		    dbgSexp(node);
		    dbgSexp(node->deps);
		    dbgSexp(dep);
		}
		assert(dep->type == OBJ_DEPENDENCY,
		       "Invalid object type");

		if (!dep->deactivated) {
		    setPush(newvec, (Object *) dep->dep);
		}
		objectFree((Object *) dep, TRUE);
	    }
	    objectFree((Object *) node->deps, FALSE);
	    node->deps = newvec;
	}
    }
}

/* A connection_set is a record of DagNodes that are strongly connected,
 * and within which cycles may be found.
 */
static void
recordConnectionSet(
    DagNode *cycle_node, 
    Vector *cycle, 
    volatile ResolverState *res_state)
{
    Vector *connection_set  = NULL;
    int i;
    Dependency *dep;
    EACH(cycle, i) {
	dep = (Dependency *) ELEM(cycle, i);
	if (connection_set = dep->dep->connection_set) {
	    break;
	}
    }
    if (!connection_set) {
	connection_set = vectorNew(10);
	if (!res_state->connection_sets) {
	    res_state->connection_sets = vectorNew(10);
	}
	vectorPush(res_state->connection_sets, (Object *) connection_set);
    }
    EACH(cycle, i) {
	dep = (Dependency *) ELEM(cycle, i);
	dep->dep->connection_set = connection_set;
	setPush(connection_set, (Object *) dep->dep);
    }
}

/* This should create the vector that recordCycle would create, and also
 * create the vector which is the known start of any cycles being
 * discovered by recordAllCycles().
 */
static Vector *
recordPath(DagNode *from, DagNode *to)
{
    Vector *cycle = vectorNew(10);
    Dependency *dep;
    do {
	dep = (Dependency *) ELEM(from->deps, from->cur_dep);
	vectorPush(cycle, (Object *) dep);
	from = dep->dep;
    } while (from != to);
    return cycle;
}


static void
recordCycle(DagNode *cycle_node, volatile ResolverState *res_state)
{
    Vector *cycle = recordPath(cycle_node, cycle_node);

    myVectorPush((Vector **) &(res_state->cycles), (Object *) cycle);
    recordConnectionSet(cycle_node, cycle, res_state);
}

/* Clear the checked state for all nodes in a connectio_set.  When we
 * are scanning deps in recordCyclicPaths, we will only traverse those
 * deps that are to nodes in the connection_set, and which we have not
 * already checked.  Setting this flag clears the way to check those
 * nodes in the connection_set exactly once. 
 */
static void
uncheckConnectionSetNodes(Vector *connection_set)
{
    int i;
    DagNode *node;
    EACH(connection_set, i) {
	node = (DagNode *) ELEM(connection_set, i);
	node->checked = FALSE;
    }
}

static void
recordCyclicPaths(
    DagNode *from, 
    DagNode *to,
    Vector *cycle,
    volatile ResolverState *res_state)
{
    int i;
    Dependency *dep;
    DagNode *node;
    Vector *found;
    EACH(from->deps, i) {
	dep = (Dependency *) ELEM(from->deps, i);
	node = dep->dep;
	if (node->connection_set == to->connection_set) {
	    if (node == to) {
		found = vectorCopy(cycle);
		vectorPush(found, (Object *) dep);
		vectorPush(res_state->cycles, (Object *) found);
	    }
	    if (!node->checked) {
		node->checked = TRUE;
		vectorPush(cycle, (Object *) dep);
		recordCyclicPaths(node, to, cycle, res_state);
		(void) vectorPop(cycle);
	    }
	}
    }
}

static void
recordAllCycles(
    DagNode *node, 
    Dependency *cur_dep, 
    volatile ResolverState *res_state)
{
    Vector *cycle = recordPath(node, cur_dep->dep);
    int i;
    Dependency *dep;

    uncheckConnectionSetNodes(node->connection_set);
    node->checked = TRUE;
    EACH(cycle, i) {
	dep = (Dependency *) ELEM(cycle, i);
	dep->dep->checked = TRUE;
    }
    recordCyclicPaths(cur_dep->dep, node, cycle, res_state);

    objectFree((Object *) cycle, FALSE);
}


/* If any of the nodes in cur_dep->dep's connection_set are being
 * visited, then we have new cycles to record.
 */
static void
recordPossibleCycles(Dependency *cur_dep, volatile ResolverState *res_state)
{
    Vector *connection_set = cur_dep->dep->connection_set;
    int i;
    DagNode *node;
    EACH(connection_set, i) {
	node = (DagNode *) ELEM(connection_set, i);
	if (node->status == VISITING) {
	    recordAllCycles(node, cur_dep, res_state);
	}
    }
}

typedef enum {
    ALL_IS_WELL = 0,
    CYCLE_DETECTED,
    POSSIBLE_CYCLE
} resolver_status;

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
	if (status == POSSIBLE_CYCLE) {
	    recordPossibleCycles(dep, res_state);
	}
	else if (status == CYCLE_DETECTED) {
	    recordCycle(dep->dep, res_state);
	}
    }
}

static resolver_status
resolveNode(DagNode *node, volatile ResolverState *res_state)
{
    switch (node->status) {
    case VISITED:
	/* This node has already been resolved, but there could be
	 * unfound cycles. */
	return node->connection_set? POSSIBLE_CYCLE: ALL_IS_WELL;
    case VISITING:
	return CYCLE_DETECTED;
    case UNVISITED:
	node->status = VISITING;
	resolveDeps(node, res_state);
	node->status = VISITED;
	if (node->build_type == REBUILD_NODE) {
	    node->build_type = BUILD_NODE;
	}
	return ALL_IS_WELL;
    default: 
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
	return ALL_IS_WELL;
    }
}

/* Ensure that only a single dependency comes from each depset.
 */
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

/* If any item in the cycle has been deactivated, the cycle has been
 * broken. 
 */
static boolean
cycleIsBroken(Vector *cycle)
{
    int i;
    Dependency *dep;
    EACH(cycle, i) {
	dep = (Dependency *) ELEM(cycle, i);
	if (dep->deactivated) {
	    return TRUE;
	}
    }
    return FALSE;
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
    if (dep->depset) {
	return (dep->deactivated) || (dep->depset->chosen_dep == NULL);
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
	/* We must be deactivating this node because it is being
	 * replaced with a fallback.
	 */
	return;
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

static String *
fqnForBreaker(DagNode *node, String *breaker_name)
{
    xmlChar *old_fqn = xmlGetProp(node->dbobject, (xmlChar *) "fqn");
    char *fqn_suffix = strstr((char *) old_fqn, ".");
    char *new_fqn = newstr("%s%s", breaker_name->value, fqn_suffix);
    String *result = stringNewByRef(new_fqn);
    xmlFree(old_fqn);
    return result;
}

static DagNode *
makeBreaker(DagNode *node, String *breaker_name)
{
    String *breaker_fqn = fqnForBreaker(node, breaker_name);
    xmlNode *breaker_dbobject = xmlCopyNode(node->dbobject, 1);
    DagNode *breaker;

    xmlSetProp(breaker_dbobject, (xmlChar *) "type", 
	       (xmlChar *) breaker_name->value);
    xmlUnsetProp(breaker_dbobject, (xmlChar *) "cycle_breaker");
    xmlSetProp(breaker_dbobject, (xmlChar *) "fqn", 
	       (xmlChar *) breaker_fqn->value);

    objectFree((Object *) breaker_fqn, TRUE);

    breaker = dagNodeNew(breaker_dbobject, node->build_type);
    breaker->parent = node->parent;

    /* Make the breaker node a child of dbobject so that it will be
     * freed later. */
    (void) xmlAddChild(node->dbobject, breaker_dbobject);

    return breaker;
}

static DagNode *
getBreakerFor(Dependency *dep, volatile ResolverState *res_state)
{
    DagNode *node;
    String *breaker_name;
    DagNode *breaker = NULL;
    node = dep->dep;
    if (breaker_name = nodeAttribute(node->dbobject, "cycle_breaker")) {
	breaker = makeBreaker(node, breaker_name);
	objectFree((Object *) breaker_name, TRUE);
    }
    return breaker;
}

static void
copyDepsForBreaker(
    DagNode *referer, 
    DagNode *node, DagNode *breaker, 
    DagNode *refered, 
    volatile ResolverState *res_state)
{
    int i;
    Dependency *dep;
    DagNode *this;
    int j;

    EACH(node->deps, i) {
	dep = (Dependency *) ELEM(node->deps, i);
	if (dep->dep != refered) {
	    if (!dep->deactivated) {
		dep = dependencyNew(dep->dep);
		myVectorPush(&(breaker->deps), (Object *) dep);
	    }
	}
    }
    /* We also have to deal with dependencies on this node.  This is
     * an inefficient, naive implementation but since there will be
     * few cycle breakers used in real life, we can accept this cost. */
    EACH(res_state->all_nodes, i) {
	this = (DagNode *) ELEM(res_state->all_nodes, i);
	if (this != referer) {
	    EACH(this->deps, j) {
		dep = (Dependency *) ELEM(this->deps, j);
		if (dep->dep == node) {
		    dep = dependencyNew(breaker);
		    vectorPush(this->deps, (Object *) dep);
		    break;
		}
	    }
	}
    }
}

/* Make the referer node in a cycle refer to the cycle breaker rather
 * than the original node.
 */
static void
modifyRefererDeps(DagNode *referer, DagNode *node, DagNode *breaker)
{
    int i;
    Dependency *dep;
    EACH(referer->deps, i) {
	dep = (Dependency *) ELEM(referer->deps, i);
	if (dep->dep == node) {
	    dep->dep = breaker;
	}
    }
}
/* TODO: ensure cycle_breakers get re-used when possible (ie if there
 * are multiple cycles involving the same node). 
 */
static boolean
resolveCycleWithBreaker(DagNode *breaker, volatile ResolverState *res_state,
			int cycle, int element)
{
    Vector *this_cycle = (Vector *) ELEM(res_state->cycles, cycle);
    Dependency *dep;
    DagNode *node;
    int referer_idx;
    DagNode *referer;
    int refered_idx;
    DagNode *refered;

    dep = (Dependency *) ELEM(this_cycle, element);
    node = dep->dep;

    referer_idx = (element == 0)? this_cycle->elems - 1: element - 1;
    dep = (Dependency *) ELEM(this_cycle, referer_idx);
    referer = dep->dep;

    refered_idx = (element >= (this_cycle->elems - 1))? 0: element + 1;
    dep = (Dependency *) ELEM(this_cycle, refered_idx);
    refered = dep->dep;

    copyDepsForBreaker(referer, node, breaker, refered, res_state);
    modifyRefererDeps(referer, node, breaker);

    vectorPush(res_state->all_nodes, (Object *) breaker);
    return TRUE;
}

/* Backtracking function to resolve cycles.  It eliminates conditional
 * dependencies and creates cycle-breaking fallback nodes, attempting to
 * find a solution that leaves no cycles.
 */
static boolean
resolveCycles(volatile ResolverState *res_state, int cycle)
{
    int i;
    Dependency *dep;
    Vector *this_cycle;
    DagNode *breaker;

    while (TRUE) {
	if (cycle >= res_state->cycles->elems) {
	    return TRUE;
	}
	if (!cycleIsBroken((Vector *) ELEM(res_state->cycles, cycle))) {
	    break;
	}
	cycle++;
    }

    this_cycle = (Vector *) ELEM(res_state->cycles, cycle);
    EACH(this_cycle, i) {
	dep = (Dependency *) ELEM(this_cycle, i);
	if (!dep->deactivated) {
	    if (canDeactivate(dep)) {
		deactivateDep(dep);
		if (resolveCycles(res_state, cycle + 1)) {
		    return TRUE;
		}
		reactivateDep(dep);
	    }
	    if (breaker = getBreakerFor(dep, res_state)) {
		if (resolveCycleWithBreaker(breaker, res_state, cycle, i)) {
		    deactivateDep(dep);
		    if (resolveCycles(res_state, cycle + 1)) {
			return TRUE;
		    }
		}
	    }
	}
    }

    return FALSE;
}

static char *
describeDep(Dependency *dep)
{
    return newstr("(%s) %s",  
		  nameForBuildType(dep->dep->build_type),
		  dep->dep->fqn->value);
}

static char *
describeCycle(Vector *cycle)
{
    int i;
    Dependency *dep;
    char *result;
    char *tmp;
    char *tmp2;
    dep = (Dependency *) ELEM(cycle, cycle->elems - 1);
    result = describeDep(dep);
    EACH(cycle, i) {
	dep = (Dependency *) ELEM(cycle, i);
	tmp2 = result;
	tmp = describeDep(dep);
	result = newstr("%s -> %s", tmp2, tmp);
	skfree(tmp2);
	skfree(tmp);
    }
    return result;
}

static char *
describeCycles(volatile ResolverState *res_state)
{
    Vector *cycle;
    int i;
    Dependency *dep;
    int j;
    boolean fixed = FALSE;
    char *result = NULL;
    char *tmp;
    char *tmp2;
    
    EACH(res_state->cycles, i) {
	fixed = FALSE;
	cycle = (Vector *) ELEM(res_state->cycles, i);
	EACH(cycle, j) {
	    dep = (Dependency *) ELEM(cycle, j);
	    if (dep->deactivated) {
		fixed = TRUE;
		break;
	    }
	}
	if (!fixed) {
	    tmp = describeCycle(cycle);
	    if (result) {
		tmp2 = newstr("%s\n  %s", result, tmp);
		skfree(result);
		skfree(tmp);
		result = tmp2;
	    }
	    else {
		result = tmp;
	    }
	}
    }
    return result;
}

static boolean
nodeHasDep(DagNode *node, DagNode *depnode)
{
    int i;
    Dependency *this;
    EACH(node->deps, i) {
	this = (Dependency *) ELEM(node->deps, i);
	if (this->dep == depnode) {
	    return TRUE;
	}
    }
    return FALSE;
}

/* After resolving dependencies so that there are no cycles, we will
 * have determined which fallbacks are in play.  For those fallbacks
 * on which we are dependent, set up the necessary endfallback
 * dependencies.  
 */
static void
activateEndFallbacks(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    int j;
    Dependency *dep;
    DagNode *endfallback;
    Dependency *newdep;
    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	if (IS_BUILDABLE(node)) {
	    EACH(node->deps, j) {
		dep = (Dependency *) ELEM(node->deps, j);
		assert(dep->dep && (dep->dep->type == OBJ_DAGNODE),
		       "Dependency must be to a DagNode");
		if (dep->dep->build_type == FALLBACK_NODE) {
		    endfallback = dep->dep->endfallback;
		    assert(endfallback && (endfallback->type == OBJ_DAGNODE),
			   "endfallback node must be to a DagNode");
		    if (!IS_FORWARD_BUILDABLE(node)) {
			/* Dependencies on fallbacks will exist for 2
			 * reasons: 1 - they are the nodes that require
			 * a fallback; 2 - they are a dependency of the
			 * fallback but the dependency direction has
			 * been inverted for the drop side of the DAG.
			 * We only want the endfallback to have a copy
			 * in case 1, so we have to be able to
			 * distinguish the 2 cases.  We have case 2
			 * if the node also has a dependency on our
			 * endfallback node.
			 */
			if (nodeHasDep(node, endfallback)) {
			    /* Do not duplicate the dep. */
			    continue;
			}
		    }
		    newdep = dependencyNew(node);
		    myVectorPush(&(endfallback->deps), (Object *) newdep);
		    vectorPush(res_state->activated_fallbacks, 
			       (Object *) dep->dep);
		    vectorPush(res_state->activated_fallbacks, 
			       (Object *) endfallback);
		}
	    }
	}
    }
}


/* Node resolution uses a modified tsort algorithm to identify cycles,
 * which are then broken by trying to eliminate optional dependencies
 * (and creating fallback nodes where possible) using a backtracking
 * algorithm (in resolveCycles()).
 * Detecting all cycles is slightly tricky, since a dependency may be
 * part of multiple cycles.
 * Simplistically, using a simple tsort algorithm, any time we traverse
 * to a node that is currently being visited, we have a cycle and we can
 * record it.  However, the following case is not detected by this
 * simplistic approach:
 * Our Graph (--> is a dependency, -o> is an optional dependency) is:
 *    N1-->N3
 *    N2-->N3
 *    N3-o>N1
 *    N3-o>N2
 *    N3-o>N4
 *    N4-o>N1
 *    N4-o>N2
 *    N4-o>N5
 * 
 * We have traversed:
 *    N1-->N3-->N1         (cycle discovered: [N3, N1])
 *           -->N2-->N3    (cycle discovered: [N2, N3])
 *           -->N4-->N1    (cycle discovered: [N3, N4, N1])
 *                -->N2?
 *
 * We are about to visit N2 (from N3, from N1).  Since N2 has already
 * been visited, we would ordinarily ignore it.  But the sequence 
 * N3-->N4-->N2 is clearly a loop. 
 * 
 * Our simplistic approach fails to find this loop because we have
 * aleady dealt with N2's dependencies.  Our solution is to take a
 * closer look at visited nodes when they have been identified as cycle
 * nodes.  If the node being visited is present in any cycles,
 * then we check the remaining nodes in those cycles.  If any of them
 * are in the VISITING state, we have another cycle.
 * Continuing our example at N2:
 *                -->N2 
 * we discover that N2 is in the VISITED state.  It is in cycle [N2,
 * N3].  Scanning the cycle we discover that N3 is in the VISITING
 * state.  That gives us a cycle of [N3, N4, N2] which we can add to our
 * list of cycles. 
 */
static void
resolveNodes(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;
    char *tmp;
    char *errmsg;
    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	(void) resolveNode(node, res_state);
    }
    if (res_state->cycles) {
	if (!resolveCycles(res_state, 0)) {
	    tmp = describeCycles(res_state);
	    errmsg = newstr("resolveNodes: unable to resolve cycles:\n  %s", 
			    tmp);
	    skfree(tmp);
	    RAISE(TSORT_CYCLIC_DEPENDENCY, errmsg);
	}
	freeCycles(res_state);
    }
    activateEndFallbacks(res_state);
}

/* Remove any fallbacks that are inactive. */
static void
deactivateInactiveFallbacks(volatile ResolverState *res_state)
{
    int i;
    DagNode *node;

    EACH(res_state->all_nodes, i) {
	node = (DagNode *) ELEM(res_state->all_nodes, i);
	if ((node->build_type == FALLBACK_NODE) ||
	    (node->build_type == ENDFALLBACK_NODE)) 
	{
	    if (!vectorFind(res_state->activated_fallbacks, (Object *) node)) {
		node->build_type = DEACTIVATED_NODE;
	    }
	}
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
    res_state->all_nodes = NULL;
    res_state->by_fqn= NULL;
    res_state->by_pqn = NULL;
    res_state->dependency_sets = NULL;
    res_state->connection_sets = NULL;
    res_state->cycles = NULL;
    res_state->activated_fallbacks = vectorNew(10);
}

static void
cleanupResolverState(volatile ResolverState *res_state)
{
    int i;

    objectFree((Object *) res_state->dependency_sets, TRUE);
    objectFree((Object *) res_state->by_fqn, FALSE);
    objectFree((Object *) res_state->by_pqn, TRUE);
    objectFree((Object *) res_state->activated_fallbacks, FALSE);
    EACH(res_state->connection_sets, i) {
	objectFree(ELEM(res_state->connection_sets, i), FALSE);
    }
    objectFree((Object *) res_state->connection_sets, FALSE);
}

/*
 * Create a Dag from the supplied doc, returning it as a vector of DagNodes.
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
	//dbgSexp(doc);
	resolver_state.by_fqn = hashByFqn(resolver_state.all_nodes);
	resolver_state.by_pqn = hashByPqn(resolver_state.all_nodes);
	identifyDependencies(resolver_state.all_nodes, &resolver_state);

	promoteRebuilds(resolver_state.all_nodes);
	makeMirrors(resolver_state.all_nodes);
	//showVectorDeps(resolver_state.all_nodes);
	//fprintf(stderr, "\n------------------------------\n\n");


	redirectDependencies(&resolver_state);
	resolveNodes(&resolver_state);
	deactivateInactiveFallbacks(&resolver_state);
	//fprintf(stderr, "\n------------------------------\n\n");
	optimiseDepsets(&resolver_state);

	convertDependencies(resolver_state.all_nodes);
	resetNodes(resolver_state.all_nodes);
	//showVectorDeps(resolver_state.all_nodes);
    }
    EXCEPTION(ex) {
	freeDeps(resolver_state.all_nodes);
	freeCycles(&resolver_state);
	objectFree((Object *) resolver_state.all_nodes, TRUE);
	cleanupResolverState(&resolver_state);
    }
    END;

    //fprintf(stderr, "CHUNK %p\n", getChunk(1000));
    cleanupResolverState(&resolver_state);
    return resolver_state.all_nodes;
}
