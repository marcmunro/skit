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
 * source xml stream by the adddeps.xsl xsl transform.  
 * dagFromDoc() reads the transformed document and creates a Directed
 * Acyclic Graph (DAG) of DagNodes from it.
 * The DAG is created in the following steps:
 * 
 * 1) DagNodes are created for each node in the document by
 *    dagNodesFromDoc() 
 * 2) The dependencies from the document are recorded into each DagNode,
 *    by addDependencies().  Build dependencies are added to the
 *    forward_deps element, and drop dependencies to the backward_deps
 *    element.  Both sets of dependencies are added in the build
 *    direction.
 * 3) A dependency set is a set of dependencies only one of which must
 *    be satisfied.  If no dependency from a set can be satisfied, a
 *    fallback may be provided.  Dependency sets are implemented as
 *    subnodes of the parent DagNode.  Multiple dependency sets are
 *    recorded as a linked list of subnodes.  Adding dependency sets is
 *    also handled by addDependencies().
 * 4) The graphs created by the set of forward_deps, and by the set of
 *    backward_deps are separately resolved into true DAGs.  This
 *    process identifies which dependency from a set is to be used,
 *    whether fallbacks are to be used, and in the case of cylcic
 *    dependencies, how cycle breaking nodes are added to the DAGs.
 *    This process of resolving the DAGs can only be done in the build
 *    direction, which is why the two sets of dependencies are both
 *    recorded in the same direction.  Done by resolveGraphs().
 * 5) The backward_deps set of dependencies is inverted for diffprep and
 *    drop nodes from each forward build direction node to its
 *    corresponding backward direction node (if any).  The inversion
 *    creates forward_deps in the mirror nodes.  This leaves us with a
 *    complete DAG formed from the forward_deps.  The original
 *    backward_deps elements play no further part.  This is done by
 *    redirectDeps()
 * 6) Depending on the type of build, and the actions defined for the
 *    dagnodes, individual dagnodes may be duplicated into a mirror pair
 *    (of build and drop nodes, or diffprep and diffcomplete nodes).
 *    The diffprep and drop nodes will later have their dependencies
 *    inverted.  This is done by expandDagNodes().
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

void
showDeps(DagNode *node, boolean show_optional)
{
    DagNode *sub;
    char *tmp;
    if (node) {
	printSexp(stderr, "NODE: ", (Object *) node);
	printSexp(stderr, "-->", (Object *) node->forward_deps);
	sub = node->forward_subnodes;
	while (show_optional && sub) {
	    if (sub->forward_deps || sub->fallback_node) {
		tmp = objectSexp((Object *) sub->fallback_node);
		fprintf(stderr, "fallback %s", tmp);
		skfree(tmp);
		printSexp(stderr, " for\n  optional->", 
			  (Object *) sub->forward_deps);
	    }
	    sub = sub->forward_subnodes;
	}
	printSexp(stderr, "<==", (Object *) node->backward_deps);
	sub = node->backward_subnodes;
	while (show_optional && sub) {
	    if (sub->backward_deps || sub->fallback_node) {
		tmp = objectSexp((Object *) sub->fallback_node);
		fprintf(stderr, "fallback %s", tmp);
		skfree(tmp);
		printSexp(stderr, " for\n  optional<=", 
			  (Object *) sub->backward_deps);
	    }
	    sub = sub->backward_subnodes;
	}
	if (node->tmp_fdeps) {
	    printSexp(stderr, "tmp>", (Object *) node->tmp_fdeps);
	}
    }
}

void
showVectorDeps(Vector *nodes, boolean show_optional)
{
    int i;
    DagNode *node;
    EACH (nodes, i) {
        node = (DagNode *) ELEM(nodes, i);
        showDeps(node, show_optional);
    }
}


/* Predicate identifying whether a node is of a specific type.
 */
static boolean
xmlnodeMatch(xmlNode *node, char *name)
{
    return node && (node->type == XML_ELEMENT_NODE) && 
	streq((char *) node->name, name);
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

boolean
isDependencySet(xmlNode *node)
{
    return streq((char *) node->name, DEPENDENCY_SET_STR);
}

boolean
isDependency(xmlNode *node)
{
    return streq((char *) node->name, DEPENDENCY_STR);
}

boolean
isDependencies(xmlNode *node)
{
    return streq((char *) node->name, DEPENDENCIES_STR);
}

boolean
isDepNode(xmlNode *node)
{
    return isDependency(node) || isDependencySet(node) || isDependencies(node);
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

    if (nodeHasAttribute(node->node , (xmlChar *) "fallback")) {
	/* By default, we don't want to do anything for a fallback
 	 * node.  Marking it as an exists node achieves that.  The
 	 * build_type will be modified if anything needs to actually
 	 * reference the fallback node.  */
	build_type = EXISTS_NODE;
    }
    else if (diff = nodeAttribute(node->node , (xmlChar *) "diff")) {
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
	    fqn = nodeAttribute(((Node *) node)->node, (xmlChar *) "fqn");
	    errmsg = newstr("buildTypeForDagnode: unexpected diff "
			    "type \"%s\" in %s", diff->value, fqn->value);
	}
	objectFree((Object *) diff, TRUE);
    }
    else if (action = nodeAttribute(node->node , (xmlChar *) "action")) { 
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
	    fqn = nodeAttribute(((Node *) node)->node, (xmlChar *) "fqn");
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
		fqn = nodeAttribute(((Node *) node)->node, (xmlChar *) "fqn");
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
	parent_fqn  = nodeAttribute(parent, (xmlChar *) "fqn");
	if (parent_fqn) {
	    node->parent = (DagNode *) hashGet(nodes_by_fqn, 
					       (Object *) parent_fqn);
	    assert(node->parent, 
		   "identifyParents: parent of %s (%s) not found",
		   node->fqn->value, parent_fqn->value);
	    objectFree((Object *) parent_fqn, TRUE);
	}
    }
}

String *
conditionForDep(xmlNode *node)
{
    String *condition_str = nodeAttribute(node, (xmlChar *) "condition");
    if (condition_str) {
	stringLowerInPlace(condition_str);
    }
    else {
	if (isDependencies(node->parent)) {
	    return conditionForDep(node->parent);
	}
	condition_str = stringNew("");
    }
    return condition_str;
}

String *
directionForDep(xmlNode *node)
{
    String *direction_str = nodeAttribute(node, (xmlChar *) "direction");
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

/* 
 * Identify to which sides of the DAG, the current dependency or
 * dependency-set applies.
 */
static DependencyApplication 
applicationForDep(xmlNode *node)
{
    String *direction_str = directionForDep(node);
    DependencyApplication result;

    if (direction_str) {
	if (streq(direction_str->value, "forwards")) {
	    result = FORWARDS;
	}
	else if (streq(direction_str->value, "backwards")) {
	    result = BACKWARDS;
	}
	else {
	    result = CUSTOM;
	}
	objectFree((Object *) direction_str, TRUE);
    }
    else {
	result = BOTH_DIRECTIONS;
    }
    return result;
}

static void
addDepToVector(Vector **p_vector, DagNode *dep)
{
    if (!dep) {
	RAISE(GENERAL_ERROR, newstr("addDepToVector: dep is NULL"));
    }
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
    if (!node) {
	RAISE(GENERAL_ERROR, newstr("addDep: node is NULL"));
    }

    if ((applies == FORWARDS) || (applies == BOTH_DIRECTIONS)) {
	addDepToVector(&(node->forward_deps), dep);
    }
    if ((applies == BACKWARDS) || (applies == BOTH_DIRECTIONS)) {
	addDepToVector(&(node->backward_deps), dep);
    }
}


static DagNode *
makeBreakerNode(DagNode *from_node, String *breaker_type)
{
    xmlNode *dbobject = from_node->dbobject;
    xmlChar *old_fqn = xmlGetProp(dbobject, (xmlChar *) "fqn");
    char *fqn_suffix = strstr((char *) old_fqn, ".");
    char *new_fqn = newstr("%s%s", breaker_type->value, fqn_suffix);
    xmlNode *breaker_dbobject = xmlCopyNode(dbobject, 1);
    DagNode *breaker;
    DagNode *dep;
    Vector *deps;
    int i;
    boolean forwards;
    xmlSetProp(breaker_dbobject, (xmlChar *) "type", 
	       (xmlChar *) breaker_type->value);
    xmlUnsetProp(breaker_dbobject, (xmlChar *) "cycle_breaker");
    xmlSetProp(breaker_dbobject, (xmlChar *) "fqn", (xmlChar *) new_fqn);
    xmlFree(old_fqn);
    skfree(new_fqn);

    breaker = dagNodeNew(breaker_dbobject, from_node->build_type);
    breaker->parent = from_node->parent;
    breaker->breaker_for = from_node;

    /* Make the breaker node a child of dbobject so that it will be
     * freed later. */
    (void) xmlAddChild(dbobject, breaker_dbobject);


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
	if (breaker_type = nodeAttribute(node->dbobject, 
					 (xmlChar *) "cycle_breaker")) {
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
    int i;

    /* Eliminate from breaker any dependency on thru */
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
    String *parent = nodeAttribute(dep_node, (xmlChar *) "parent");
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
    fqn = nodeAttribute(node, (xmlChar *) "fqn");
    result = (DagNode *) hashGet(byfqn, (Object *) fqn);
    objectFree((Object *) fqn, TRUE);
    return result;
}
    
static DagNode *
makeFallbackNode(String *fqn)
{
    xmlNode *dbobj;
    DagNode *result;

    dbobj = domakeFallbackNode(fqn);
    result = dagNodeNew(dbobj, INACTIVE_NODE);
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
addDepsForNode(DagNode *dbobject, Vector *allnodes, Hash *byfqn, Hash *bypqn);

static DagNode *
getFallbackNode(xmlNode *dep_node, Hash *byfqn, Hash *bypqn, Vector *allnodes)
{
    String *volatile fallback = NULL;
    DagNode *fallback_node = NULL;
    DagNode *parent;

    if (isDependencySet(dep_node)) {
	BEGIN {
	    fallback = nodeAttribute(dep_node, (xmlChar *) "fallback");
	    if (fallback) {
		fallback_node = (DagNode *) hashGet(byfqn, (Object *) fallback);
		if (!fallback_node) {
		    fallback_node = makeFallbackNode(fallback);
		    parent = parentNodeForFallback(dep_node, byfqn);
		    setParent(fallback_node, parent);
		    hashAdd(byfqn, (Object *) fallback, 
			    (Object *) fallback_node);
		    fallback = NULL;
		    setPush(allnodes, (Object *) fallback_node);
		    addDepsForNode(fallback_node, allnodes, byfqn, bypqn);
		}
		else {
		    objectFree((Object *) fallback, TRUE);
		}
	    }
	}
	EXCEPTION(ex) {
	    objectFree((Object *) fallback, TRUE);
	}
	END;
    }
    return fallback_node;
}


/* TODO: Fix the api for this function - it is ugly. */
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


static boolean
directionMatch(DependencyApplication applies, boolean forwards)
{
    switch (applies) {
    case FORWARDS: return forwards;
    case BACKWARDS: return !forwards;
    default: return TRUE;
    }
}

static DagNode *
newSubNode(DagNode *node, DependencyApplication applies, DagNode *fallback)
{
    DagNode *new = dagNodeNew(node->dbobject, OPTIONAL_NODE);
    new->supernode = node;
    if ((applies == FORWARDS) || (applies == BOTH_DIRECTIONS)) {
	new->forward_subnodes = node->forward_subnodes;
	node->forward_subnodes = new;
	new->forward_deps = vectorNew(10);
    }
    if ((applies == BACKWARDS) || (applies == BOTH_DIRECTIONS)) {
	new->backward_subnodes = node->backward_subnodes;
	node->backward_subnodes = new;
	new->backward_deps = vectorNew(10);
    }
    new->fallback_node = fallback;
    return new;
}

static void
processDepNode(
    DagNode *dbobject, 
    xmlNode *depnode,
    boolean in_depset,
    Vector *allnodes, 
    Hash *byfqn, 
    Hash *bypqn)
{
    DependencyApplication applies;
    xmlNode *this = NULL;
    DagNode *fallback_node;
    DagNode *subnodef;
    DagNode *subnodeb;
    DagNode *dep;
    Cons *cons;
    String *qn = NULL;
    char *tmp;
    char *errmsg;

    applies = applicationForDep(depnode);
    if (isDependencies(depnode)) {
	while (this = nextDependency(depnode->children, this)) {
	    processDepNode(dbobject, this, FALSE, 
			   allnodes, byfqn, bypqn);
	}
    }
    else if (isDependencySet(depnode)) {
	fallback_node = getFallbackNode(depnode, byfqn, bypqn, allnodes);
	if ((applies == FORWARDS) || (applies == BOTH_DIRECTIONS)) {
	    subnodef = newSubNode(dbobject, FORWARDS, fallback_node);
	}
	if ((applies == BACKWARDS) || (applies == BOTH_DIRECTIONS)) {
	    subnodeb = newSubNode(dbobject, BACKWARDS, fallback_node);
	}
	while (this = nextDependency(depnode->children, this)) {
	    if ((applies == FORWARDS) || (applies == BOTH_DIRECTIONS)) {
		processDepNode(subnodef, this, TRUE, 
			       allnodes, byfqn, bypqn);
	    }
	    if ((applies == BACKWARDS) || (applies == BOTH_DIRECTIONS)) {
		processDepNode(subnodeb, this, TRUE, 
			       allnodes, byfqn, bypqn);
	    }
	}	
	/* TODO: raise an error if no dependencies were found and there
	 * is no fallback. */
    }
    else if (isDependency(depnode)) {
	if (qn = nodeAttribute(depnode, (xmlChar *) "fqn")) {
	    if (dep = (DagNode *) hashGet(byfqn, (Object *) qn)) {
		if (in_depset || dep->build_type != EXISTS_NODE) {
		    /* If dep is an EXISTS_NODE it can be ignored unless
		     * this is part of a dependency set.  EXISTS_NODE
		     * elements can always be satisfied, so are redundant
		     * except for when choosing which dependency in a
		     * dependency set may be satisfied. */
		    addDep(dbobject, dep, applies);
		}
	    }
	    else {
		/* No dep was found.  FQN dependencies must be satisfied
		 * unless as part of a dependency-set. */
		if (!in_depset) {
		    errmsg = newstr("Cannot find dependency %s for %s.",
				    qn->value, dbobject->fqn->value);
		    objectFree((Object *) qn, TRUE);
		    dbgSexp(dbobject);
		    dNode(depnode);
		    RAISE(TSORT_ERROR, errmsg);
		}
	    }
	}
	else if (qn = nodeAttribute(depnode, (xmlChar *) "pqn")) {
	    if (cons = (Cons *) hashGet(bypqn, (Object *) qn)) {
		while (cons) {
		    dep = (DagNode *) dereference(cons->car);
		    addDep(dbobject, dep, applies);
		    cons = (Cons *) cons->cdr;
		}
	    }
	}
	objectFree((Object *) qn, TRUE);
    }
    else {
	tmp = nodestr(depnode);
	errmsg = newstr("Unexpected node type in dbobject %s.   "
			"Expecting dependency, found: %s", 
			dbobject->fqn->value, tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
}

static void
addDepsForNode(DagNode *dbobject, Vector *allnodes, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode = NULL;

    assert(dbobject, "addDepsForNode: no dbobject provided");
    assert(dbobject->dbobject, 
	   "addDepsForNode: dbobject node has no dbobject");

    while (depnode = nextDependency(dbobject->dbobject->children, depnode)) {
	processDepNode(dbobject, depnode, FALSE, allnodes, byfqn, bypqn);
    }
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
processDepNode2(
    DagNode *dbobject, 
    xmlNode *depnode,
    Vector *allnodes, 
    Hash *byfqn, 
    Hash *bypqn)
{
    DependencyApplication applies;
    xmlNode *this = NULL;
    DagNode *dep;
    Cons *cons;
    String *qn = NULL;
    char *tmp;
    char *errmsg;

    applies = applicationForDep(depnode);
    if (isDependencies(depnode)) {
	while (this = nextDependency(depnode->children, this)) {
	    processDepNode2(dbobject, this, allnodes, byfqn, bypqn);
	}
    }
    else if (isDependencySet(depnode)) {
	/* Dependency sets are processed later, when we have enough
	 * information to handle conditionality (in
	 * resolveConditional()).
	 */
    }
    else if (isDependency(depnode)) {
	if (qn = nodeAttribute(depnode, (xmlChar *) "fqn")) {
	    if (dep = (DagNode *) hashGet(byfqn, (Object *) qn)) {
		if (dep->build_type != EXISTS_NODE) {
		    /* If dep is an EXISTS_NODE it can be ignored.
		     * dependencies on EXISTS_NODE elements can always
		     * be satisfied, so dependencies on them are
		     * effectively redundant.
		     */
		    addDep(dbobject, dep, applies);
		}
	    }
	    else {
		/* No dep was found.  FQN dependencies must be
		 * satisfied.
		 */
		errmsg = newstr("Cannot find dependency %s for %s.",
				qn->value, dbobject->fqn->value);
		objectFree((Object *) qn, TRUE);
		dbgSexp(dbobject);
		dNode(depnode);
		RAISE(TSORT_ERROR, errmsg);
	    }
	}
	else if (qn = nodeAttribute(depnode, (xmlChar *) "pqn")) {
	    if (cons = (Cons *) hashGet(bypqn, (Object *) qn)) {
		while (cons) {
		    dep = (DagNode *) dereference(cons->car);
		    addDep(dbobject, dep, applies);
		    cons = (Cons *) cons->cdr;
		}
	    }
	}
	objectFree((Object *) qn, TRUE);
    }
    else {
	tmp = nodestr(depnode);
	errmsg = newstr("Unexpected node type in dbobject %s.   "
			"Expecting dependency, found: %s", 
			dbobject->fqn->value, tmp);
	skfree(tmp);
	RAISE(TSORT_ERROR, errmsg);
    }
}

static void
addUnconditionalDepsForNode(
    DagNode *dbobject, 
    Vector *allnodes, 
    Hash *byfqn, 
    Hash *bypqn)
{
    xmlNode *depnode = NULL;

    assert(dbobject, "addDepsForNode: no dbobject provided");
    assert(dbobject->dbobject, 
	   "addDepsForNode: dbobject node has no dbobject");

    while (depnode = nextDependency(dbobject->dbobject->children, depnode)) {
	processDepNode2(dbobject, depnode, allnodes, byfqn, bypqn);
    }
}

static void
addUnconditionalDependencies(Vector *nodes, Hash *byfqn, Hash *bypqn)
{
   DagNode *volatile this = NULL;
   int volatile i;
   EACH(nodes, i) {
       this = (DagNode *) ELEM(nodes, i);
       assert(this->type == OBJ_DAGNODE, "incorrect node type");
       addUnconditionalDepsForNode(this, nodes, byfqn, bypqn);
   }
}

static void
prepareFallback(DagNode *fallback, Vector *nodes)
{
    DagNode *endfallback;
    int i;
    DagNode *dep;
    char *endfqn;

    if (!fallback->fallback_node) {
	endfallback = dagNodeNew(fallback->dbobject, ENDFALLBACK_NODE);
	endfqn = newstr("end%s", endfallback->fqn->value);
	skfree(endfallback->fqn->value);
	endfallback->fqn->value = endfqn;
	endfallback->parent = fallback->parent;
	fallback->fallback_node = endfallback;
	endfallback->fallback_node = fallback;
	setPush(nodes, (Object *) endfallback);

	/* Copy dependencies from fallback node to endfallback (they
	 * both depend on the same objects (eg the role to be given
	 * superuser in the postgres fallback to superuser instance). */
	EACH(fallback->forward_deps, i) {
	    dep = (DagNode *) ELEM(fallback->forward_deps, i);
	    addDep(endfallback, dep, FORWARDS);
	}
	EACH(fallback->backward_deps, i) {
	    dep = (DagNode *) ELEM(fallback->backward_deps, i);
	    addDep(endfallback, dep, BACKWARDS);
	}

	/* Add dependencies from endfallback to fallback. */
	addDep(endfallback, fallback, FORWARDS);
	addDep(fallback, endfallback, BACKWARDS);
    }
}

static void
activateFallback(DagNode *fallback, Vector *nodes)
{
    DagNode *endfallback;
    DagNode *dep;
    char *endfqn;
    int i;

    assert(fallback && fallback->type == OBJ_DAGNODE, 
	   "activateFallback: node is not a DagNode");
 
    if (fallback->build_type == INACTIVE_NODE) {
	assert(!fallback->fallback_node,
	       "activateFallback: fallback_node already defined");
	
	endfallback = dagNodeNew(fallback->dbobject, ENDFALLBACK_NODE);
	endfqn = newstr("end%s", endfallback->fqn->value);
	skfree(endfallback->fqn->value);
	endfallback->fqn->value = endfqn;
	endfallback->parent = fallback->parent;
	fallback->fallback_node = endfallback;
	endfallback->fallback_node = fallback;
	setPush(nodes, (Object *) endfallback);

	/* Copy dependencies from fallback node to endfallback (they
	 * both depend on the same objects (eg the role to be given
	 * superuser in the postgres fallback to superuser instance). */
	EACH(fallback->forward_deps, i) {
	    dep = (DagNode *) ELEM(fallback->forward_deps, i);
	    addDep(endfallback, dep, FORWARDS);
	}
	EACH(fallback->backward_deps, i) {
	    dep = (DagNode *) ELEM(fallback->backward_deps, i);
	    addDep(endfallback, dep, BACKWARDS);
	}

	/* Add dependencies from endfallback to fallback. */
	addDep(endfallback, fallback, FORWARDS);
	addDep(fallback, endfallback, BACKWARDS);

	fallback->build_type = FALLBACK_NODE;
	endfallback->build_type = ENDFALLBACK_NODE;
    }
}

static void
addDepsForFallback(DagNode *node, DagNode *fallback, boolean forwards)
{
    DagNode *endfallback = fallback->fallback_node;
    if (forwards) {
	addDep(node, fallback, FORWARDS);
	addDep(endfallback, node, FORWARDS);
    }
    else {
	addDep(fallback, node, BACKWARDS);
	addDep(node, endfallback, BACKWARDS);
    }
}

static void
addFallbackForNode(
    DagNode *subnode, 
    Vector *nodes, 
    DagNode *fallback,
    boolean forwards)
{
    DagNode *super = subnode->supernode;

    if (!fallback) {
	RAISE(TSORT_ERROR, 
	      newstr("No fallback found for dependency-set in %s",
		  subnode->fqn->value));
    }
    if (fallback->build_type == INACTIVE_NODE) {
	/* This is an inactive fallback node, so we activate it. */
	prepareFallback(fallback, nodes);
    }

    if (fallback->fallback_node) {
	/* This is an activated fallback node */
	addDepsForFallback(super, fallback, forwards);
    }
    else {
	/* This must be an existing node that is being used as a
	   fallback.   In this case, it is just as if this were a normal
	   dependency.  */
	addDep(super, fallback, forwards? FORWARDS: BACKWARDS);
    }
}


static void
resolveNode(DagNode *node, DagNode *from, boolean forwards, Vector *nodes);

static void
resolveDeps(DagNode *node, boolean forwards, Vector *nodes)
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
	    if (dep->build_type == INACTIVE_NODE) {
		/* Dependencies on inactive fallback nodes are not
 		 * considered to be satisfied, so try the next one. */
		continue;
	    }
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
		     * dependency and we can just try the next one.
		     */
		    skfree(errmsg);
		    continue;
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
		return;
	    }
	}
    }

    if (node->supernode) {
	/* We are processing a set of optional dependencies but have not
	 * found any that satisfy or we would have already returned to
	 * the caller.  Now we must invoke our fallback.
	 */
	node->dep_idx = -1;
	/* Use the specified fallback node for this dependency set. */
	addFallbackForNode(node, nodes, node->fallback_node, forwards);
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
	/* Flow through to UNVISITED state.  RESOLVED_F means the same
	 * as UNVISITED and occurs when resolving backwards deps after
	 * the forward deps have been resolved. */
    case UNVISITED: 
	node->status = VISITING;
	BEGIN {
	    resolveDeps(node, forwards, nodes);
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
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
    }
}

static DagNode *
nextSubnode(DagNode *sub, boolean forwards)
{
    if (forwards) {
	return (DagNode *) sub->forward_subnodes;
    }
    return (DagNode *) sub->backward_subnodes;
}

static DagNode *supe = NULL;

static void
promoteSubnodeDeps(DagNode *node, boolean forwards)
{
    DagNode *sub = node;
    Vector *deps;
    DagNode *dep;
    DagNode *endfallback;

    while (sub = nextSubnode(sub, forwards)) {
	if (sub->dep_idx >= 0) {
	    /* The index gives the chosen dep. */
	    deps = forwards? sub->forward_deps: sub->backward_deps;
	    dep = (DagNode *) ELEM(deps, sub->dep_idx);

	    if (dep == supe) {
		dbgSexp(node);
		dbgSexp(dep);
	    }

	    if ((dep->build_type == INACTIVE_NODE) ||
		(dep->build_type == FALLBACK_NODE))
	    {
		/* If this dep is to a fallback node, promote it and
		 * then add the dep */
		 
		dep->build_type = FALLBACK_NODE;
		endfallback = dep->fallback_node;

		if (forwards) {
		    addDepToVector(&(node->forward_deps), dep);
		    addDepToVector(&(endfallback->forward_deps), node);
		}
		else {
		    addDepToVector(&(node->backward_deps), endfallback);
		    addDepToVector(&(dep->backward_deps), node);
		}
	    }
	    else {
		if (forwards) {
		    addDepToVector(&(node->forward_deps), dep);
		}
		else {
		    addDepToVector(&(node->backward_deps), dep);
		}
	    }
	}
	else {
	    /* No dep was chosen.  The fallback has already been applied
	     * but it may need to be promoted. */
	    if (sub->fallback_node->build_type == INACTIVE_NODE) {
		sub->fallback_node->build_type = FALLBACK_NODE;
	    }
	}
    }
}

static void
resolveNodeUnconditional(DagNode *node, boolean forwards, Vector *nodes);

static void
resolveDepsUnconditional(DagNode *node, boolean forwards, Vector *nodes)
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
	    if (dep->build_type == INACTIVE_NODE) {
		/* TODO: Figure out if this logic still makes sense - it
		 * may be a remnant from a past version that is no
		 * longer appropriate since we refactored. */

		/* Dependencies on inactive fallback nodes are not
 		 * considered to be satisfied, so try the next one. */
		continue;
	    }
	    BEGIN {
		cyclic_exception = FALSE;
		resolveNodeUnconditional(dep, forwards, nodes);
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
	}
    }
}

static void
resolveNodeUnconditional(DagNode *node, boolean forwards, Vector *nodes)
{
    assert(node && node->type == OBJ_DAGNODE, 
	   "resolveNodeConditional: node is not a DagNode");
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("%s", node->fqn->value), node);
    case RESOLVED_F:
	if (forwards) {
	    break;
	}
	/* Flow through to UNVISITED state.  RESOLVED_F here means the
	 * same as UNVISITED and occurs when resolving backwards deps
	 * after the forward deps have been resolved.  Having two states
	 * for RESOLVED means that we don't have to explicitly reset the
	 * state after processing each direction.  */
    case UNVISITED: 
	node->status = VISITING;
	BEGIN {
	    resolveDepsUnconditional(node, forwards, nodes);
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
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
    }
}


/* Resolves the non-optional parts of the forward and backward DAGS,
 * eliminating cycles.  The algorithm for resolving the graph is
 * essentially a classic tsort algorithm, with cyclic exceptions trapped
 * and handled by adding a cycle breaker.
 */
static void
resolveUnconditional(Vector *nodes)
{
    DagNode *node;
    int i;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNodeUnconditional(node, TRUE, nodes);
    }
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNodeUnconditional(node, FALSE, nodes);
    }
}

static xmlNode *
nextDepNode(xmlNode *from, xmlNode *root)
{
    xmlNode *node;
    if (from) {
	if (from->children) {
	    node = firstElement(from->children);
	}
	else {
	    node = firstElement(from->next);
	}

	while (TRUE) {
	    if (node) {
		if (isDepNode(node)) {
		    return node;
		}
		node = firstElement(node->next);
	    }
	    else {
		from = from->parent;
		if (!from || (from == root)) {
		    return NULL;
		}
		node = firstElement(from->next);
	    }
	}
    }
    return NULL;
}

static xmlNode *
nextDepSet(xmlNode *from, xmlNode *dbobject, boolean forwards)
{
    DependencyApplication applies;
    xmlNode *node = nextDepNode(from, dbobject);

    while (node) {
	if (isDependencySet(node)) {
	    applies = applicationForDep(node);
	    if (directionMatch(applies, forwards)) {
		return node;
	    }
	}
	node = nextDepNode(node, dbobject);
    }
    return node;
}
 
static xmlNode *
nextDependency2(xmlNode *from, xmlNode *parent)
{
    xmlNode *node = nextDepNode(from, parent);

    while (node) {
	if (isDependency(node)) {
	    return node;
	}
	node = nextDepNode(node, parent);
    }
    return node;
}

static void
resolveNodeConditional(
    DagNode *node, 
    boolean forwards, 
    Vector *nodes,
    Hash *byfqn,
    Hash *bypqn);

static DagNode *
resolveDependencySet(
    xmlNode *volatile depset, 
    volatile boolean forwards,
    Vector *volatile nodes,
    Hash *volatile byfqn,
    Hash *volatile bypqn)
{
    xmlNode *volatile depnode;
    DagNode *volatile dep = NULL;
    String *qn;
    volatile Cons single_dep = {OBJ_CONS, NULL, NULL};
    volatile Cons *volatile deps;

    depnode = depset;
    while (depnode = nextDependency2(depnode, depset)) {
	deps = NULL;
	if (qn = nodeAttribute(depnode, (xmlChar *) "fqn")) {
	    if (dep = (DagNode *) hashGet(byfqn, (Object *) qn)) {
		single_dep.car = (Object *) dep;
		deps = &single_dep;
	    }
	}
	else if (qn = nodeAttribute(depnode, (xmlChar *) "pqn")) {
	    deps = (Cons *) hashGet(bypqn, (Object *) qn);
	}
	objectFree((Object *) qn, TRUE);

	while (deps) {
	    BEGIN {
		dep = (DagNode *) dereference(deps->car);
		resolveNodeConditional(dep, forwards, nodes, byfqn, bypqn);
		/* If there is a cyclic dependency, we will not reach
		 * this point as an exception will have been raised.
		 */
		deps = NULL; 
	    }
	    EXCEPTION(ex);
	    WHEN(TSORT_CYCLIC_DEPENDENCY) {
		/* Nothing to do here, we just continue trying other
		 * dependencies from the set.  If this is the last dep,
		 * the function must return NULL.
		 */
		dep = NULL;
	    }
	    END;
	    if (dep) {
		/* We found a dep, so return it. */
		return dep;
	    }
	    deps = (Cons *) deps->cdr;
	}
    }
    return dep;
}

/* Returns a vector of DagNode references (for all of the resolved
 * dependencies), to the caller. */
static void
resolveDepsConditional(
    DagNode *node, 
    volatile boolean forwards, 
    Vector *volatile nodes, 
    Hash *volatile byfqn, 
    Hash *volatile bypqn)
{
    Vector *volatile deps = forwards? node->forward_deps: node->backward_deps;
    int i;
    DagNode *dep;
    xmlNode *depset = node->dbobject;
    Vector *volatile resolved_deps = NULL;

    /* Traverse the unconditional deps first. */
    if (deps) {
	EACH(deps, i) {
	    dep = (DagNode *) ELEM(deps, i);
	    resolveNodeConditional(dep, forwards, nodes, byfqn, bypqn);
	}
    }

    BEGIN {
	while (depset = nextDepSet(depset, node->dbobject, forwards)) {
	    dep = resolveDependencySet(depset, forwards, nodes, byfqn, bypqn);
	    if (!dep) {
		dep = getFallbackNode(depset, byfqn, bypqn, nodes);
		if (!dep) {
		    RAISE(TSORT_ERROR,
			  newstr("Unable to resolve dependency-set for %s.",
				 node->fqn->value));
		}
	    }
	    if (!resolved_deps) {
		resolved_deps = vectorNew(10);
	    }
	    vectorPush(resolved_deps, (Object *) dep);
	}
    }
    EXCEPTION(ex) {
	if (resolved_deps) {
	    objectFree((Object *) resolved_deps, TRUE);
	}
    }
    END;
    node->tmp_fdeps = resolved_deps;
}

static void
resolveNodeConditional(
    DagNode *node, 
    boolean forwards, 
    Vector *nodes,
    Hash *byfqn,
    Hash *bypqn)
{
    DagNodeStatus orig_status;

    assert(node && node->type == OBJ_DAGNODE, 
	   "resolveNodeCconditional: node is not a DagNode");
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("%s", node->fqn->value), node);
    case RESOLVED_F:
	if (forwards) {
	    break;
	}
	/* Flow through to UNVISITED state.  RESOLVED_F here means the
	 * same as UNVISITED and occurs when resolving backwards deps
	 * after the forward deps have been resolved.  Having two states
	 * for RESOLVED means that we don't have to explicitly reset the
	 * state after processing each direction.  */
    case UNVISITED: 
	orig_status = node->status;
	node->status = VISITING;
	BEGIN {
	    if (node->tmp_fdeps) {
		objectFree((Object *) node->tmp_fdeps, FALSE);
	    }
	    resolveDepsConditional(node, forwards, nodes, byfqn, bypqn);
	}
	EXCEPTION(ex) {
	    node->status = orig_status;
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
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
    }
    return;
}

static void
addDepToNode(DagNode *node, DagNode *dep, boolean forwards) {
    Vector **p_deps = forwards? &(node->forward_deps): &(node->backward_deps);

    if (dep->build_type == FALLBACK_NODE) {
	addDepsForFallback(node, dep, forwards);
    }
    else {
	addDepToVector(p_deps, dep);
    }
}


/* Once the conditional deps have been resolved, each node containing
 * such deps will have a vector in tmp_fdeps.  Move the contents of this
 * into the appropriate xxxx_deps array. 
 */
static void
addResolvedConditionalDeps(Vector *nodes, boolean forwards)
{
    DagNode *node;
    int i;
    DagNode *dep;
    int j;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (node->tmp_fdeps) {
	    EACH(node->tmp_fdeps, j) {
		dep = (DagNode *) ELEM(node->tmp_fdeps, j);
		if (dep->build_type == INACTIVE_NODE) {
		    /* This dependency is on a fallback node that has
		     * not yet been activated. */
		    activateFallback(dep, nodes);
		}
		addDepToNode(node, dep, forwards);
	    }
	    objectFree((Object *) node->tmp_fdeps, FALSE);
	    node->tmp_fdeps = NULL;
	}
    }
}

/* Resolves the optional parts of the forward and backward DAGS,
 * identifying a suitable dependency for each dependency-set, or
 * invoking the fallback if no suitable existing dependency can be
 * found.
 */
static void
resolveConditional(Vector *nodes, Hash *byfqn, Hash *bypqn)
{
    DagNode *node;
    int i;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNodeConditional(node, TRUE, nodes, byfqn, bypqn);
    }
    addResolvedConditionalDeps(nodes, TRUE);

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNodeConditional(node, FALSE, nodes, byfqn, bypqn);
    }
    addResolvedConditionalDeps(nodes, FALSE);
}

/* Takes the dependency graphs (forward_deps and backward_deps), and
 * resolves them into true DAGs, elimminating cycles, and choosing a
 * single appropriate dependency (or fallback) from each dependency set.
 * The algorithm for resolving the graph is essentially a classic tsort
 * algorithm, with cyclic exceptions trapped and handled by choosing a
 * different optional dependency, or adding a cycle breaker.
 */
static void
resolveGraphs(Vector *nodes)
{
    DagNode *node;
    int i;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNode(node, NULL, TRUE, nodes);
    }
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNode(node, NULL, FALSE, nodes);
    }

    /* Now do final resolution of the subnodes.  This has to be done
     * after the resolution passes have completed as during resolution
     * the subnodes may have to be re-resolved.  Note that the final
     * resolution of fallback nodes is a little convoluted.
     */
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	promoteSubnodeDeps(node, TRUE);
	promoteSubnodeDeps(node, FALSE);
    }
}


/* If we are a promotable node and any of our forward dependencies are
 * on a rebuild node, we promote node to be a rebuild node.
 */
static boolean
promoteRebuildForwards(DagNode *node)
{
    int i;
    DagNode *dep;
    switch (node->status) {
    case UNVISITED:
	/* This node's status has already been reset by this operation.
	 * There is notyhing more to be done with it.
	 */
	 break;
    case RESOLVED:
	if ((node->build_type == EXISTS_NODE) ||
	    (node->build_type == DIFF_NODE)) {
	    /* This node is promotable. */
	    EACH(node->forward_deps, i) {
		dep = (DagNode *) ELEM(node->forward_deps, i);
		if (promoteRebuildForwards(dep)) {
		    /* We depend on a REBUILD_NODE, promote this node,
		     * and we are done.  */
		    node->build_type = REBUILD_NODE;
		    break;
		}
	    }
	}
	break;
    default:
	RAISE(DEPS_ERROR, 
	      newstr("promoteRebuildForwards: unexpected status: %d",
		     node->status));
    }
    node->status = UNVISITED;
    return node->build_type == REBUILD_NODE;
}


static void promoteBackwardDeps(DagNode *node);

/* If we are a rebuild node, we will promote any of our backward
 * dependents that we can.
 */
static void
promoteRebuildBackwards(DagNode *node)
{
    if ((node->build_type == EXISTS_NODE) ||
	(node->build_type == DIFF_NODE)) {
	node->build_type = REBUILD_NODE;
    }
    promoteBackwardDeps(node);
}

static void
promoteBackwardDeps(DagNode *node)
{
    int i;
    DagNode *dep;
    if (node->build_type == REBUILD_NODE) {
	EACH(node->backward_deps, i) {
	    dep = (DagNode *) ELEM(node->backward_deps, i);
	    promoteRebuildBackwards(dep);
	}
    }
}

/* Any node that depends on a node with a buld_type of REBUILD, must
 * itself be promoted to a rebuild.   All nodes start with a status of
 * RESOLVED and will finish with a status of UNVISITED. 
 */ 
static void
promoteRebuildsNew(Vector *nodes)
{
    int i;
    DagNode *node;
 
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	(void) promoteRebuildForwards(node);
    }
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	promoteBackwardDeps(node);
    }
}


/* Any node that depends on a node with a buld_type of REBUILD, must
 * itself be promoted to a rebuild. */
static void
promoteRebuilds(Vector *nodes)
{
    int i;
    DagNode *node;
    int j;
    DagNode *dep;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if ((node->build_type == EXISTS_NODE) ||
	    (node->build_type == DIFF_NODE))
	{
	    /* This node may need to be promoted to a rebuild node. */
	    EACH(node->forward_deps, j) {
		dep = (DagNode *) ELEM(node->forward_deps, j);
		if (dep->build_type == REBUILD_NODE) {
		    node->build_type = REBUILD_NODE;
		    break;
		}
	    }
	}
    }
}

static xmlNode *
findContext(xmlNode *node)
{
    xmlNode *this;
    if (node) {
	for (this = getElement(node->children); 
	     this; 
	     this = getElement(this->next)) 
	{
	    if (streq("context", (char *) this->name)) {
		return this;
	    }
	}
    }
    return NULL;
}

static void
processDirectionalContexts(DagNode *node, DagNode *mirror_node)
{
    xmlNode *context = findContext(node->dbobject);
    xmlNode *next;
    xmlNode *copy;
    // TODO: Rewrite this to simplify.
    String *direction = nodeAttribute(context, (xmlChar *) "direction");
    if (direction) {
	next = getElement(context->next);
	if (!streq("context", (char *) next->name)) {
	    objectFree((Object *) direction, TRUE);
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("Directional contexts are not paired in %s",
			 node->fqn->value));
	}

	copy = xmlCopyNode(node->dbobject, 1);
	if (streq(direction->value, "backwards")) {
	    /*  We remove the backwards context from node, and the
	     *  forwards context from mirror_node. */
	    xmlUnlinkNode(context);
	    xmlFreeNode(context);
	    context = findContext(copy);
	    next = getElement(context->next);
	    objectFree((Object *) direction, TRUE);
	    direction = nodeAttribute(next, (xmlChar *) "direction");
	    if (direction && streq(direction->value, "forwards")) {
		objectFree((Object *) direction, TRUE);
		/* Remove the forwards node from our copy. */
		xmlUnlinkNode(next);
		xmlFreeNode(next);

		/* Add the copy into our source document (so that it can
		 * be properly freed).  This is safe as we are no longer
		 * parsing the document at this stage - we are just
		 * processing a vector of DagNodes. */
		xmlAddChild(node->dbobject->parent, copy);
		mirror_node->dbobject = copy;
	    }
	    else {
		objectFree((Object *) direction, TRUE);
		RAISE(XML_PROCESSING_ERROR, 
		      newstr("Failed to find forwards directional context "
			     "in %s.", node->fqn->value));
	    }
		
	}
	else {
	    objectFree((Object *) direction, TRUE);
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("Not implemented: handling of directional contexts"
			 " in unexpected order"));
	}
    }
}

static void
makeMirrorNode(DagNode *node, DagNodeBuildType build_type, char *prefix)
{
    DagNode *mirror = dagNodeNew(node->dbobject, build_type);
    char *newfqn;
    DagNode *parent;
    if (prefix) {
	newfqn = newstr("%s%s", prefix, mirror->fqn->value);
	skfree(mirror->fqn->value);
	mirror->fqn->value = newfqn;
    }
    if (parent = node->parent) {
	if (parent->mirror_node) {
	    parent = parent->mirror_node;
	}
    }
    mirror->parent = parent;
    node->mirror_node = mirror;
    processDirectionalContexts(node, mirror);
}

/* For nodes which must be expanded into a pair of nodes, create the
 * mirror node.  Note that we do not yet create any dependencies for
 * these new nodes. */
static void
createMirrorNodes(Vector *nodes)
{
    int i;
    DagNode *node;
    DagNodeBuildType new_build_type;
    char *prefix = NULL;
    boolean single_dag = TRUE;
    Vector *mirrors = vectorNew(nodes->elems);

    EACH(nodes, i) {
	new_build_type = UNSPECIFIED_NODE;
	node = (DagNode *) ELEM(nodes, i);
	if (node->build_type == REBUILD_NODE) {
	    new_build_type = DROP_NODE;
	} else if (node->build_type == DIFF_NODE) {
	    new_build_type = DIFFPREP_NODE;
	}

	if (new_build_type != UNSPECIFIED_NODE) {
	    makeMirrorNode(node, new_build_type, prefix);
	    single_dag = FALSE;
	    vectorPush(mirrors, (Object *) node->mirror_node);
	}
    }

    if (single_dag) {
	objectFree((Object *) mirrors, FALSE);
	return;
    }

    EACH(nodes, i) {
	new_build_type = UNSPECIFIED_NODE;
	node = (DagNode *) ELEM(nodes, i);
	if (node->build_type == FALLBACK_NODE) {
	    new_build_type = FALLBACK_NODE;
	    prefix = "ds";
	}
	else if (node->build_type == ENDFALLBACK_NODE) {
	    new_build_type = ENDFALLBACK_NODE;
	    prefix = "endds";
	}
	if (new_build_type != UNSPECIFIED_NODE) {
	    makeMirrorNode(node, new_build_type, prefix);
	    vectorPush(mirrors, (Object *) node->mirror_node);
	    prefix = NULL;
	}
    }

    vectorAppend(nodes, mirrors);
    objectFree((Object *) mirrors, FALSE);
}

/* UNSURE IS FOR USE DURING DEVELOPMENT ONLY */
typedef enum {
    FCOPY, REVCOPYC, IGNORE, ERROR, UNSURE, FBDROP, FBBUILD
} redirectActionType;

static redirectActionType redirect_action
    [EXISTS_NODE][EXISTS_NODE][2] = 
{
    /* Array is indexed by [node->build_type][depnode->build_type]
     *      [build_direction (backwards before forwards)] */
    /* BUILD */ 
    {{IGNORE, FCOPY}, {REVCOPYC, IGNORE},       /* BUILD, DROP */
     {IGNORE, FCOPY}, {IGNORE, FCOPY},       /* REBUILD, DIFF */
     {IGNORE, FCOPY}, {IGNORE, ERROR}},     /* FALLBACK, ENDFALLBACK */
    /* DROP */ 
    {{ERROR, ERROR}, {REVCOPYC, IGNORE},      /* BUILD, DROP */
     {REVCOPYC, IGNORE}, {REVCOPYC, IGNORE},      /* REBUILD, DIFF */
     {REVCOPYC, IGNORE}, {REVCOPYC, ERROR}},     /* FALLBACK, ENDFALLBACK */
    /* REBUILD */ 
    {{ERROR, FCOPY}, {REVCOPYC, ERROR},        /* BUILD, DROP */
     {REVCOPYC, FCOPY}, {REVCOPYC, FCOPY},     /* REBUILD, DIFF */
     {ERROR, FCOPY}, {REVCOPYC, ERROR}},    /* FALLBACK, ENDFALLBACK */
    /* DIFF */ 
    {{ERROR, FCOPY}, {REVCOPYC, ERROR},    /* BUILD, DROP */
     {REVCOPYC, FCOPY}, {REVCOPYC, FCOPY},     /* REBUILD, DIFF */
     {ERROR, FCOPY}, {REVCOPYC, ERROR}},      /* FALLBACK, ENDFALLBACK */
    /* FALLBACK */ 
    {{IGNORE, FBBUILD}, {REVCOPYC, FBDROP},  /* BUILD, DROP */
     {REVCOPYC, FCOPY}, {REVCOPYC, FCOPY} , /* REBUILD, DIFF */
     {ERROR, ERROR}, {REVCOPYC, ERROR}},    /* FALLBACK, ENDFALLBACK */
    /* ENDFALLBACK */ 
    {{IGNORE, FCOPY}, {REVCOPYC, IGNORE},  /* BUILD, DROP */
     {REVCOPYC, FCOPY}, {REVCOPYC, FCOPY},  /* REBUILD, DIFF */
     {ERROR, FCOPY}, {ERROR, ERROR}}        /* FALLBACK, ENDFALLBACK */
};


static void
redirectNodeDeps(DagNode *node, DagNode *dep, int direction)
{
    redirectActionType action;
    DagNode *mirror;
    DagNode *dmirror;

    if ((node->build_type == EXISTS_NODE) ||
	(node->build_type == INACTIVE_NODE) ||
	(dep->build_type == INACTIVE_NODE) ||
	(dep->build_type == EXISTS_NODE)) {
	/* Although most exists nodes should have been removed, we may
	 * be facing the situtation where a fallback node has been
	 * rendered harmless, so this quick exit is still needed. */

	return;
    }
    action = redirect_action[node->build_type][dep->build_type][direction];
    switch(action) {
    case FBDROP:
	/* This is a forward dependency on a drop from a fallback node,
	 * and is considered a very sepcial case.  The dependency on the
	 * underlying object must be satisfied from both the fallback
	 * and drop-side fallback nodes, even though the object is about
	 * to be dropped.  The drop side of the DAG is already handled
	 * by the normal rules but the build side is this special case.
	 * In this case we take the forward dependency from the fallback
	 * node to the drop node, and convert it into a dependency from
	 * the drop node to the endfallback node.  This is enough to
	 * ensure that the fallback and endfallback are performed before
	 * the node in question is dropped.  Although this modifies the
	 * underlying DAGs defined by the forward and backward deps, it
	 * should not lead to a cyclic graph as nothing significant can
	 * depend on the drop node (only other drops and fallbacks).
	 * At least that's what I hope and believe. */
	/* TODO: find a way to eliminate this inversion of the DAG
	 * direction.  Also for FBBUILD below. */

	mirror = node->fallback_node;
	assert(mirror,
	       "Failed to find node for endfallback from node %s.",
	       node->fqn->value);
	assert(mirror->build_type == ENDFALLBACK_NODE,
	       "Expected endfallback node, found %d from node %s.",
	       mirror->build_type, node->fqn->value);
        addDepToVector(&(dep->tmp_fdeps), mirror);
	return;
    case FBBUILD:
	/* Like FBDROP, this is a special case.  This is for
	 * dependencies from fallback nodes to build nodes and ensures
	 * any drop side fallback is also a dependent.  Note that this
	 * does not respetc the existing DAGs so may not be a
	 * tremendously safe solution.  A better solution is needed.
	 */
	if (mirror = node->mirror_node) {
	    addDepToVector(&mirror->tmp_fdeps, dep);
	}
	/* Note the deliberate flow-thru to the FCOPY case. */
    case FCOPY:
	/* This is a forward dependency that should be copied.  We use
	 * tmp_fdeps so that we don't stomp over partial results.  The
	 * tmp_fdeps vector will be copied over the forward_deps vector
	 * later. 
	 */
	addDepToVector(&node->tmp_fdeps, dep);
	return;
    case REVCOPYC:
        /* This is a backward dependency that should be reversed and
         * applied to the drop side of the DAG conditionally (if it
         * exists). */
        mirror = node->mirror_node? node->mirror_node: node;
        dmirror = dep->mirror_node? dep->mirror_node: dep;
        addDepToVector(&(dmirror->tmp_fdeps), mirror);
        return;
    case UNSURE:
	/* This is for debugging only. */
	fprintf(stderr, "UNSURE: %d\n", direction);
	dbgSexp(node);
	dbgSexp(node->mirror_node);
	dbgSexp(dep);
	dbgSexp(dep->mirror_node);
	break;
    case IGNORE:
	return;
    default:
	showDeps(node, TRUE);
	showDeps(dep, TRUE);
	RAISE(DEPS_ERROR, 
	      newstr("%s dep from (%s) %s to (%s) %s not handled.", 
		     direction? "Forward": "Backward",
		     nameForBuildType(node->build_type),
		     node->fqn->value, 
		     nameForBuildType(dep->build_type),
		     dep->fqn->value));
    }
}

static void
renderHarmless(Vector *redundants)
{
    int i;
    DagNode *node;
    EACH(redundants, i) {
	node = (DagNode *) ELEM(redundants, i);
	node->build_type = EXISTS_NODE;
    }
}

static boolean
endForFallback(DagNode *end, DagNode *fallback)
{
    char *fb;
    char *endfb;
    boolean result;
    fb = fallback->fqn->value;
    endfb = end->fqn->value;
    result = streq(fb, endfb + 3);
    return result;
}

/* To remove redundant fallback nodes, we modify them to be exists
 * nodes.  A redundant fallback node is identified as follows:
 * 1) it is a forward direction (ie the build side of the DAG) fallback
 *    node which appears in no dependencies of non-fallback nodes.
 * 2) it is a backward direction (ie the drop side of the DAG) fallback
 *    node which has no dependencies on non-fallback nodes.
 * 3) is is an endfallback node for a redundant fallback node.
 */
static void
disableRedundantFallbacks(Vector *nodes, Vector *fallback_nodes)
{
    Vector *removed = vectorNew(fallback_nodes->elems);
    DagNode *node;
    DagNode *dep;
    DagNode *fnode;
    int i;
    int j;

    /* Remove fallback nodes that have dependencies */
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (node->build_type != ENDFALLBACK_NODE) {
	    EACH(node->forward_deps, j) {
		dep = (DagNode *) ELEM(node->forward_deps, j);
		if (dep->build_type == FALLBACK_NODE) {
		    if (dep = (DagNode *) vectorDel(fallback_nodes, 
						    (Object *) dep)) 
		    {
			vectorPush(removed, (Object *) dep);
		    }
		}
	    }
	}
    }

    /* Remove any endfallback nodes that match the removed fallback
     * nodes.  This leaves only the unreferenced fallback nodes in the
     * fallback nodes vector.  */
    EACH(removed, i) {
	fnode = (DagNode *) ELEM(removed, i);
	EACH(fallback_nodes, j) {
	    node = (DagNode *) ELEM(fallback_nodes, j);
	    if (node->build_type == ENDFALLBACK_NODE) {
		if (endForFallback(node, fnode)) {
		    (void) vectorRemove(fallback_nodes, j);
		    break;
		}
	    }
	}
    }

    renderHarmless(fallback_nodes);
    objectFree((Object *) fallback_nodes, FALSE);
    objectFree((Object *) removed, FALSE);
}

static void
redirectDeps(Vector *nodes)
{
    int i;
    DagNode *node;
    int j;
    DagNode *dep;
    DagNode *fallback;
    Vector *fallback_nodes = vectorNew(10);

    BEGIN {
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    EACH(node->backward_deps, j) {
		dep = (DagNode *) ELEM(node->backward_deps, j);
		redirectNodeDeps(node, dep, 0);
	    }
	    EACH(node->forward_deps, j) {
		dep = (DagNode *) ELEM(node->forward_deps, j);
		redirectNodeDeps(node, dep, 1);
	    }
	}
    }
    EXCEPTION(ex) {
	/* Free any tmp_fdeps created prior to the exception. */
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    objectFree((Object *) node->tmp_fdeps, FALSE);
	    node->tmp_fdeps = NULL;
	}
	objectFree((Object *) fallback_nodes, FALSE);
    }
    END;

    /* Replace the original forward deps with newly minted ones, and
     * identify fallback nodes that are explicit dependencies, so that
     * we can remove any redundant ones. */
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	objectFree((Object *) node->forward_deps, FALSE);
	objectFree((Object *) node->backward_deps, FALSE);
	node->backward_deps = NULL;
	node->forward_deps = node->tmp_fdeps;
	node->tmp_fdeps = NULL;
	if (node->mirror_node) {
	    if (node->build_type == ENDFALLBACK_NODE) {
		/* We want fallback nodes to depend on the endfallback
		 * nodes of their mirrors.  This gives us a DAG like
		 * this:
		 *    endfback->fback->dsendfback->dsfallback
		 * Which means that the fallback cannot be done until
		 * the drop-side endfallback is done.  If we didn't take
		 * this special action the DAG would look like this:
		 *   endfback->fback------+
		 *           ->dsendfback-+--> dsfback
		 * which would allow fback and dsendfback to be
		 * performed in either order, which would be wrong.
		 */
		if (node->mirror_node) {
		    /* node->mirror_node is enddsfallback */
		    if (fallback = node->fallback_node) {
			addDepToVector(&fallback->forward_deps, 
				       node->mirror_node);
		    }
		    else {
			RAISE(DEPS_ERROR,
			      newstr("Failed to find fallback matching %s", 
				     node->fqn->value));
		    }
		}
	    }
	    else if (node->build_type != FALLBACK_NODE) {
		addDepToVector(&node->forward_deps, node->mirror_node);
	    }

	    if (node->build_type == REBUILD_NODE) {
		node->build_type = BUILD_NODE;
	    }
	    else if (node->build_type == DIFF_NODE) {
		node->build_type = DIFFCOMPLETE_NODE;
	    }
	    else if ((node->build_type == FALLBACK_NODE) ||
		     (node->build_type == ENDFALLBACK_NODE))
	    {
		vectorPush(fallback_nodes, (Object *) node);
		vectorPush(fallback_nodes, (Object *) node->mirror_node);
	    }
	}
    }
    
    disableRedundantFallbacks(nodes, fallback_nodes);
}

static void
swapBackwardBreakers(Vector *nodes)
{
    int i;
    DagNode *node;
    DagNode *dropnode;
    DagNode *breaker;
    String *fqntmp;
    xmlNode *dbobjecttmp;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (breaker = node->breaker_for) {
	    if ((node->build_type == DIFFPREP_NODE) ||
		(node->build_type == DROP_NODE))
	    {
		dropnode = node;
	    }
	    else {
		dropnode = node->mirror_node;
	    }
	    if (dropnode) {
		assert(dropnode,
		       "swapBackwardBreakers: no dropnode");
		assert(dropnode->type == OBJ_DAGNODE,
		       "swapBackwardBreakers: invalid dropnode object");

		if (!((breaker->build_type == DIFFPREP_NODE) ||
		      (breaker->build_type == DROP_NODE)))
		{
		    breaker = breaker->mirror_node;
		}
		assert(breaker,
		       "swapBackwardBreakers: no breaker");
		assert(breaker->type == OBJ_DAGNODE,
		       "swapBackwardBreakers: invalid breaker object");

		/* We now invert the meaning of the dropnode and its
		 * cycle breaker.  This is due to the need to invert the
		 * order of operations for dropping a cycle breaker,
		 * from the order used to create it.  Sadly, it is not
		 * enough to simply invert the order of dependencies.
		 * This is because when creating cyclic objects, the
		 * cycle breaker must be created first.  When dropping
		 * them, the cycle breaker must also occur first, but by
		 * inverting the order of dependencies (as is
		 * necessary) it will be last.  The safe and simple
		 * solution is this inversion.  To invert the actions
		 * that will be performed, all that is necessary is to
		 * swap the fqns.
		 */
		fqntmp = dropnode->fqn;
		dbobjecttmp = dropnode->dbobject;

		dropnode->fqn = breaker->fqn;
		dropnode->dbobject = breaker->dbobject;

		breaker->fqn = fqntmp;
		breaker->dbobject = dbobjecttmp;
	    }
	}
    }
}

/* Create a Dag from the supplied doc, returning it as a vector of DocNodes.
 * See the file header comment for a more detailed description of what
 * this does.
 */
Vector *
dagFromDoc(Document *doc)
{
    Hash *volatile byfqn = NULL;
    Hash *volatile bypqn = NULL;
    Vector *volatile nodes = dagNodesFromDoc(doc);
    byfqn = hashByFqn(nodes);
    boolean original = TRUE;

    // Need to add a new promoteRebuilds which will be done before
    // handling conditional deps.  This is so that the build-type for
    // each node can be known in order to apply optional deps
    // conditionally.  The new promoteRebuilds can only be done after
    // resolving the graphs for all non-conditional dependencies, so
    // that cycle-breakers will have been added if necessary.  This
    // means that our new dagFromDep proceeds in the following steps:
    // - addDependencies
    // - resolveUnconditionalDeps
    // - promoteRebuilds
    // - resolveConditionalDeps
    // - createMirrorNodes
    // - redirectDeps
    // - swapBackwardBreakers


    BEGIN {
//dbgSexp(doc);
	identifyParents(nodes, byfqn);
	bypqn = hashByPqn(nodes);
	if (original) {
	    addDependencies(nodes, byfqn, bypqn);

//fprintf(stderr, "============INITIAL==============\n");
//showVectorDeps(nodes, TRUE);

	    resolveGraphs(nodes);
//fprintf(stderr, "\n============RESOLVED==============\n");
//showVectorDeps(nodes, FALSE);

	    promoteRebuilds(nodes);
	    createMirrorNodes(nodes);
	    redirectDeps(nodes);
//fprintf(stderr, "\n============REDIRECTED==============\n");
//showVectorDeps(nodes, FALSE);

	    swapBackwardBreakers(nodes);
//fprintf(stderr, "\n============SWAPPED==============\n");
//showVectorDeps(nodes, FALSE);
//fprintf(stderr, "============DONE==============\n");

	}
	else {
	    addUnconditionalDependencies(nodes, byfqn, bypqn);

	    resolveUnconditional(nodes);
//fprintf(stderr, "\n============RESOLVED (U)==============\n");
//showVectorDeps(nodes, FALSE);

	    promoteRebuildsNew(nodes);  
//fprintf(stderr, "\n============PROMOTED==============\n");
//showVectorDeps(nodes, FALSE);


	    resolveConditional(nodes, byfqn, bypqn);
fprintf(stderr, "\n============RESOLVED (C)==============\n");
showVectorDeps(nodes, FALSE);


	    createMirrorNodes(nodes);
	    redirectDeps(nodes);
//fprintf(stderr, "\n============REDIRECTED==============\n");
//showVectorDeps(nodes, FALSE);

	    swapBackwardBreakers(nodes);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) byfqn, FALSE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) bypqn, TRUE);
    }
    END;
    objectFree((Object *) bypqn, TRUE);
    objectFree((Object *) byfqn, FALSE);

    return nodes;
}
