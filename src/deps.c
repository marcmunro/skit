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
showDeps(DagNode *node)
{
    DagNode *sub;
    if (node) {
	printSexp(stderr, "NODE: ", (Object *) node);
	printSexp(stderr, "-->", (Object *) node->forward_deps);
	sub = node->forward_subnodes;
	while (sub) {
	    if (sub->forward_deps || sub->fallback_node) {
		printSexp(stderr, "optional->", (Object *) sub->forward_deps);
	    }
	    sub = sub->forward_subnodes;
	}
	printSexp(stderr, "<==", (Object *) node->backward_deps);
	sub = node->backward_subnodes;
	while (sub) {
	    if (sub->backward_deps || sub->fallback_node) {
		printSexp(stderr, "optional<=", (Object *) sub->backward_deps);
	    }
	    sub = sub->backward_subnodes;
	}
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


/* Predicate identifying whether a node is of a specific type.
 */
static boolean
xmlnodeMatch(xmlNode *node, char *name)
{
    return node && (node->type == XML_ELEMENT_NODE) && streq(node->name, name);
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
    return streq(node->name, DEPENDENCY_SET_STR);
}

boolean
isDependency(xmlNode *node)
{
    return streq(node->name, DEPENDENCY_STR);
}

boolean
isDependencies(xmlNode *node)
{
    return streq(node->name, DEPENDENCIES_STR);
}

boolean
isDepNode(xmlNode *node)
{
    return isDependency(node) || isDependencySet(node) || isDependencies(node);
}

static xmlNode *
nextDepElement(xmlNode *start)
{
    xmlNode *node = firstElement(start);
    while (node && !isDepNode(node)) {
	node = firstElement(node->next);
    }
    return node;
}

static xmlNode *
nextDependency(xmlNode *depnode, xmlNode *cur)
{
    xmlNode *node = NULL;
    if (!depnode) {
	return NULL;
    }
    if (cur) {
	node = nextDepElement(cur->next);
	if (!node) {
	    if (cur->parent) {
		/* We must have been stepping through dependencies in a 
		 * dependencies element. */
		node = nextDependency(depnode, cur->parent);
	    }
	}
    }
    else {
	if (isDependencySet(depnode)) {
	    node = nextDependency(depnode->children, NULL);
	}
	else {
	    node = nextDepElement(depnode);
	}
    }

    if (node && isDependencies(node)) {
	/* This is a dependencies node.  Return the dependency nodes
	 * which are its children. */
	node = nextDependency(node->children, NULL);
    }
    return node;
}


static Vector *
cons2Vector(Cons *cons)
{
    Vector *vector = vectorNew(10);
    Cons *cur;
    for (cur = cons; cur; cur = (Cons *) cur->cdr) {
	vectorPush(vector, cur->car);
    }
    return vector;
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
	/* By default, we don't want to do anything for a fallback
 	 * node.  Marking it as an exists node achieves that.  The
 	 * build_type will be modified if anything needs to actually
 	 * reference the fallback node.  */
	build_type = EXISTS_NODE;
    }
    else if (diff = nodeAttribute(node->node , "diff")) {
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
    else if (action = nodeAttribute(node->node , "action")) { 
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
				"build type for node %s", 
				diff->value, fqn->value);
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
    DagNodeBuildType build_type;
    DagNode *dagnode = NULL;

    BEGIN {
	if (streq(node->node->name, "dbobject")) {
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
	pqn = xmlGetProp(node->dbobject, "pqn");
	if (pqn) {
	    key = stringNew(pqn);
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
	parent_fqn  = nodeAttribute(parent, "fqn");
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
    String *condition_str = nodeAttribute(node, "condition");
    if (condition_str) {
	stringLowerOld(condition_str);
    }
    else {
	if (isDependencies(node->parent)) {
	    return conditionForDep(node->parent);
	}
	condition_str = stringNew("");
    }
    return condition_str;
}

/* 
 * Identify to which sides of the DAG, the current dependency or
 * dependency-set applies.
 */
static DependencyApplication 
applicationForDep(xmlNode *node)
{
    String *condition_str = nodeAttribute(node, "condition");
    String separators = {OBJ_STRING, " ()"};
    Cons *contents;
    Cons *elem;
    char *head;
    DependencyApplication result;
    boolean inverted = FALSE;
    xmlNode *parent;

    if (condition_str) {
	stringLowerOld(condition_str);
	elem = contents = stringSplit(condition_str, &separators);
	while (elem) {
	    head = ((String *) elem->car)->value;
	    if (streq(head, "forwards")) {
		result = FORWARDS;
	    }
	    else if (streq(head, "backwards")) {
		result = BACKWARDS;
	    }
	    else {
		result = CUSTOM;
	    }
	    elem = (Cons *) elem->cdr;
	}

	objectFree((Object *) condition_str, TRUE);
	objectFree((Object *) contents, TRUE);
	return result;
    }
    else {
	if (isDependencies(node->parent)) {
	    return applicationForDep(node->parent);
	}
    }
    return BOTH_DIRECTIONS;
}

/* 
 * Append dependencies, returning a vector if there are multiple
 * dependencies, otherwise returning a single DagNode.
 */
static Object *
appendDep(Object *cur, Object *new)
{
    Vector *vec;
    if (cur) {
	if (new) {
	    if (cur->type != OBJ_VECTOR) {
		vec = vectorNew(10);
		vectorPush(vec, cur);
		cur = (Object *) vec;
	    }

	    if (new->type == OBJ_VECTOR) {
		vectorAppend((Vector *) cur, (Vector *) new);
		objectFree((Object *) new, FALSE);
	    }
	    else {
		vectorPush((Vector *) cur, new);
	    }
	}
	return cur;
    }
    return new;
}
    
static Object *
getDepSet(xmlNode *node, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode = NULL;
    Object *dep;
    Cons *cons;
    Object *result = NULL;
    String *qn = NULL;

    if (isDependencySet(node)) {
	while (depnode = nextDependency(node, depnode)) {
	    dep = getDepSet(depnode, byfqn, bypqn);
	    if (dep) {
		if ((dep->type == OBJ_DAGNODE) &&
		    (((DagNode *) dep)->build_type == EXISTS_NODE)) {
		    /* This dependency set is automatically satisfied.
		     * This means we do not need to return a set but
		     * just this simple dependency.  So we clean up and
		     * exit.
		     */

		    if (result && result->type == OBJ_VECTOR) {
			objectFree(result, FALSE);
		    }
		    return dep;
		}
		else {
		    result = appendDep(result, dep);
		}
	    }
	}
    }
    else {
	if (qn = nodeAttribute(node, "fqn")) {
	    dep = hashGet(byfqn, (Object *) qn);
	    result = appendDep(result, dep);
	}
	else if (qn = nodeAttribute(node, "pqn")) {
	    if (cons = (Cons *) hashGet(bypqn, (Object *) qn)) {
		assert(cons->type == OBJ_CONS, "DEP BY PQN IS NOT A CONS CELL");
		if (!cons->cdr) {
		    /* List has only a single element */
		    dep = dereference(cons->car);
		}
		else {
		    dep = (Object *) toVector(cons);
		}
		result = appendDep(result, dep);
	    }
	}
	objectFree((Object *) qn, TRUE);
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
    if ((applies == FORWARDS) || (applies == BOTH_DIRECTIONS)) {
	addDepToVector(&(node->forward_deps), dep);
    }
    if ((applies == BACKWARDS) || (applies == BOTH_DIRECTIONS)) {
	addDepToVector(&(node->backward_deps), dep);
    }
}


static DagNode *
newSubNode(DagNode *node, DependencyApplication applies)
{
    DagNode *new = dagNodeNew(node->dbobject, OPTIONAL_NODE);
    new->supernode = node;
    if ((applies == FORWARDS) || (applies == BOTH_DIRECTIONS)) {
	new->forward_subnodes = node->forward_subnodes;
	node->forward_subnodes = new;
    }
    if ((applies == BACKWARDS) || (applies == BOTH_DIRECTIONS)) {
	new->backward_subnodes = node->backward_subnodes;
	node->backward_subnodes = new;
    }
    return new;
}

static DagNode *
makeBreakerNode(DagNode *from_node, String *breaker_type)
{
    xmlNode *dbobject = from_node->dbobject;
    xmlChar *old_fqn = xmlGetProp(dbobject, "fqn");
    char *fqn_suffix = strstr(old_fqn, ".");
    char *new_fqn = newstr("%s%s", breaker_type->value, fqn_suffix);
    xmlNode *breaker_dbobject = xmlCopyNode(dbobject, 1);
    DagNode *breaker;
    DagNode *dep;
    Vector *deps;
    int i;
    boolean forwards;
    xmlSetProp(breaker_dbobject, "type", breaker_type->value);
    xmlUnsetProp(breaker_dbobject, "cycle_breaker");
    xmlSetProp(breaker_dbobject, "fqn", new_fqn);
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
	if (breaker_type = nodeAttribute(node->dbobject, "cycle_breaker")) {
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
    Vector *newdeps;
    int i;

    /* Eliminiate from breaker any dependency on thru */
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

static xmlNode *
getContentNode(xmlNode *dbobject)
{
    String *type = nodeAttribute(dbobject, "type");
    xmlNode *node;
    if (type) {
	if (node = firstElement(dbobject->children)) {
	    do {
		if (streq(node->name, type->value)) {
		    objectFree((Object *) type, TRUE);
		    return node;
		}
	    } while (node = firstElement(node->next));
	}
	objectFree((Object *) type, TRUE);
    }
    return NULL;
}

/* 
 * Create a simple fallback node, which we wil place into its own
 * document.  This will then be processed by the fallbacks.xsl script to
 * create a fully formed dbobject which will then be added to our 
 * document and to various hashes and vectors.
 */
static xmlNode *
domakeFallbackNode(String *fqn, Hash *byfqn)
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

    docStackPush(doc);
    fallback_processor = getFallbackProcessor();
    applyXSL(fallback_processor);
    doc = docStackPop();
    root = xmlDocGetRootElement(doc->doc);
    dbobject = xmlCopyNode(getElement(root->children), 1);
    objectFree((Object *) doc, TRUE);
    //dNode(dbobject);
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
    
static DagNode *
makeFallbackNode(String *fqn, Hash *byfqn, Hash *bypqn)
{
    xmlNode *dbobj;
    DagNode *result;

    dbobj = domakeFallbackNode(fqn, byfqn);
    result = dagNodeNew(dbobj, EXISTS_NODE);
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

static DagNode *
getFallbackNode(xmlNode *dep_node, Hash *byfqn, Hash *bypqn, Vector *allnodes)
{
    String *fallback = NULL;
    DagNode *fallback_node = NULL;
    char *errmsg;
    DagNode *parent;

    if (isDependencySet(dep_node)) {
	fallback = nodeAttribute(dep_node, "fallback");
	if (fallback) {
	    fallback_node = (DagNode *) hashGet(byfqn, (Object *) fallback);
	    if (!fallback_node) {
		fallback_node = makeFallbackNode(fallback, byfqn, bypqn);
		parent = parentNodeForFallback(dep_node, byfqn);
		setParent(fallback_node, parent);
		hashAdd(byfqn, (Object *) fallback, (Object *) fallback_node);
		setPush(allnodes, (Object *) fallback_node);
	    }
	    else {
		objectFree((Object *) fallback, TRUE);
	    }
	}
    }
    return fallback_node;
}



/* Add both forward and back dependencies to a DagNode from the source
 * xml objects.
 */
static void
addDepsForNode(DagNode *node, Vector *allnodes, Hash *byfqn, Hash *bypqn)
{
    xmlNode *depnode = NULL;
    Object *deps;
    DagNode *fallback_node;
    DagNode *sub;
    DependencyApplication applies;
    Object *tmp;

    assert(node, "addDepsForNode: no node provided");
    assert(node->dbobject, "addDepsForNode: node has no dbobject");

    while (depnode = nextDependency(node->dbobject->children, depnode)) {
	/* depnode is either a dependency or a dependency-set node and
	 * so there may be a single dependency or a set of dependencies
	 * for this depnode.  If there is a set, only one of the
	 * dependencies from the set needs to be resolved.  We deal with
	 * that by creating a subnode for dependency sets.
	 */
	applies = applicationForDep(depnode);
	fallback_node = getFallbackNode(depnode, byfqn, bypqn, allnodes);
	deps = getDepSet(depnode, byfqn, bypqn);

	if (deps && (deps->type == OBJ_DAGNODE) && 
	    (((DagNode *) deps)->build_type == EXISTS_NODE))
	{
	    /* There is no need to record a dependency against this dep,
	     * as it is an EXISTS_NODE.
	     */
	    continue;
	}

	if (fallback_node || (deps && (deps->type == OBJ_VECTOR))) {

	    if ((!deps) || ((deps->type != OBJ_VECTOR))) {
		tmp = deps;
		deps = (Object *) vectorNew(10);
		if (tmp) {
		    setPush((Vector *) deps, tmp);
		}
	    }
	    if ((applies == FORWARDS) || (applies == BOTH_DIRECTIONS)) {

		// No failure before here
		sub = newSubNode(node, FORWARDS);
		sub->forward_deps = (Vector *) deps;
		sub->fallback_node = fallback_node;
	    }

	    if ((applies == BACKWARDS) || (applies == BOTH_DIRECTIONS)) {
		sub = newSubNode(node, BACKWARDS);
		if (deps && (applies == BOTH_DIRECTIONS)) {
		    sub->backward_deps = vectorCopy((Vector *) deps);
		}
		else {
		    sub->backward_deps = (Vector *) deps;
		}
		sub->fallback_node = fallback_node;
	    }
	}
	else if (deps) {
	    addDep(node, (DagNode *) deps, applies);
	}
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
activateFallback(DagNode *fallback, Vector *nodes)
{
    DagNode *endfallback;
    int i;
    DagNode *dep;
    char *endfqn;

    if (fallback->build_type != FALLBACK_NODE) {
	/* Promote this node from an exists node into a build node, and
	 * create the matching drop node for it. */
	fallback->build_type = FALLBACK_NODE;
	endfallback = dagNodeNew(fallback->dbobject, ENDFALLBACK_NODE);
	endfqn = newstr("end%s", endfallback->fqn->value);
	skfree(endfallback->fqn->value);
	endfallback->fqn->value = endfqn;
	endfallback->parent = fallback->parent;
	fallback->fallback_node = endfallback;
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
addFallbackDeps(DagNode *node, Vector *nodes)
{
    DagNode *fallback;
    DagNode *endfallback;
    DagNode *super = node->supernode;

    if (!(fallback = node->fallback_node)) {
	RAISE(TSORT_ERROR, 
	      newstr("No fallback found for dependency-set in %s",
		  node->fqn->value));
    }
    activateFallback(fallback, nodes);
    endfallback = fallback->fallback_node;

    addDep(super, fallback, FORWARDS);
    addDep(fallback, super, BACKWARDS);
    addDep(endfallback, super, FORWARDS);
    addDep(super, endfallback, BACKWARDS);
}

static void
resolveNode(DagNode *node, DagNode *from, boolean forwards, Vector *nodes);

static void
resolveDeps(DagNode *node, DagNode *from, boolean forwards, Vector *nodes)
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
	addFallbackDeps(node, nodes);
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
    Vector *deps;
    DagNode *dep;
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
    case UNVISITED: 
	node->status = VISITING;
	BEGIN {
	    resolveDeps(node, from, forwards, nodes);
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
	      newstr("Unexpected node status: %d", node->status));
    }
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
    DagNode *sub;
    DagNode *dep;
    int i;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNode(node, NULL, TRUE, nodes);
    }

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	resolveNode(node, NULL, FALSE, nodes);
    }

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	sub = node;
	while (sub = sub->forward_subnodes) {
	    if (sub->dep_idx >= 0) {
		dep = (DagNode *) ELEM(sub->forward_deps, sub->dep_idx);
		addDepToVector(&(node->forward_deps), dep);
	    }
	}

	sub = node;
	while (sub = sub->backward_subnodes) {
	    if (sub->dep_idx >= 0) {
		dep = (DagNode *) ELEM(sub->backward_deps, sub->dep_idx);
		addDepToVector(&(node->backward_deps), dep);
	    }
	}
    }
}

static void
promoteRebuildDeps(DagNode *node)
{
    DagNode *dep;
    int i;
    EACH(node->backward_deps, i) {
	dep = (DagNode *) ELEM(node->backward_deps, i);
	if ((dep->build_type != REBUILD_NODE) &&
	    (dep->build_type != FALLBACK_NODE) &&
	    (dep->build_type != ENDFALLBACK_NODE)) 
	{
	    /* This node needs to be promoted. */
	    dep->build_type = REBUILD_NODE;
	    promoteRebuildDeps(dep);
	}
    }
}

/* Any node that depends on a node with a buld_type of REBUILD, must
 * itself be promoted to a rebuild. */
static void
promoteRebuilds(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (node->build_type == REBUILD_NODE) {
	    promoteRebuildDeps(node);
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
processConditionalContexts(DagNode *node, DagNode *mirror_node)
{
    xmlNode *context = findContext(node->dbobject);
    xmlNode *next;
    xmlNode *copy;
    String *condition = nodeAttribute(context, "condition");
    if (condition) {
	next = getElement(context->next);
	if (!streq("context", (char *) next->name)) {
	    objectFree((Object *) condition, TRUE);
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("Conditional contexts are not paired in %s",
			 node->fqn->value));
	}

	copy = xmlCopyNode(node->dbobject, 1);
	if (streq(condition->value, "backwards")) {
	    /*  We remove the backwards context from node, and the
	     *  forwards context from mirror_node. */
	    xmlUnlinkNode(context);
	    xmlFreeNode(context);
	    context = findContext(copy);
	    next = getElement(context->next);
	    objectFree((Object *) condition, TRUE);
	    condition = nodeAttribute(next, "condition");
	    if (streq(condition->value, "forwards")) {
		objectFree((Object *) condition, TRUE);
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
		objectFree((Object *) condition, TRUE);
		RAISE(XML_PROCESSING_ERROR, 
		      newstr("Failed to find forwards conditional context "
			     "in %s.", node->fqn->value));
	    }
		
	}
	else {
	    objectFree((Object *) condition, TRUE);
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("Not implemented: handling of conditional contexts"
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
    processConditionalContexts(node, mirror);
}

/* For nodes which must be expanded into a pair of nodes, create the
 * mirror node.  Note that we do not yet create any dependencies for
 * these new nodes. */
static void
createMirrorNodes(Vector *nodes)
{
    int i;
    DagNode *node;
    DagNode *mirror;
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
	    prefix = NULL;
	    vectorPush(mirrors, (Object *) node->mirror_node);
	}
    }

    vectorAppend(nodes, mirrors);
    objectFree((Object *) mirrors, FALSE);
}

/* UNSURE IS FOR USE DURING DEVELOPMENT ONLY */
typedef enum {
    FCOPY, FMCOPY, REVCOPYC, IGNORE, INVERT, INVERT2MC, 
    ERROR, UNSURE, MINVERTC
} redirectActionType;

static redirectActionType redirect_action
    [EXISTS_NODE][EXISTS_NODE][2] = 
{
    /* Array is indexed by [node->build_type][depnode->build_type]
     *      [build_direction (backwards before forwards)] */
    /* BUILD */ 
    {{IGNORE, FCOPY}, {ERROR, ERROR},       /* BUILD, DROP */
     {ERROR, ERROR}, {IGNORE, FCOPY},       /* REBUILD, DIFF */
     {IGNORE, FCOPY}, {IGNORE, ERROR}},     /* FALLBACK, ENDFALLBACK */
    /* DROP */ 
    {{ERROR, ERROR}, {INVERT, IGNORE},      /* BUILD, DROP */
     {ERROR, ERROR}, {INVERT, IGNORE},      /* REBUILD, DIFF */
     {ERROR, IGNORE}, {INVERT, ERROR}},     /* FALLBACK, ENDFALLBACK */
    /* REBUILD */ 
    {{ERROR, ERROR}, {ERROR, ERROR},        /* BUILD, DROP */
     {REVCOPYC, FCOPY}, {ERROR, ERROR},     /* REBUILD, DIFF */
     {ERROR, FCOPY}, {REVCOPYC, ERROR}},    /* FALLBACK, ENDFALLBACK */
    /* DIFF */ 
    {{ERROR, FCOPY}, {INVERT2MC, ERROR},    /* BUILD, DROP */
     {ERROR, ERROR}, {REVCOPYC, FCOPY},     /* REBUILD, DIFF */
     {ERROR, FCOPY}, {IGNORE, ERROR}},      /* FALLBACK, ENDFALLBACK */
    /* FALLBACK */ 
    {{IGNORE, FCOPY}, {INVERT2MC, IGNORE},  /* BUILD, DROP */
     {REVCOPYC, FCOPY}, {REVCOPYC, FCOPY} , /* REBUILD, DIFF */
     {ERROR, ERROR}, {REVCOPYC, ERROR}},    /* FALLBACK, ENDFALLBACK */
    /* ENDFALLBACK */ 
    {{IGNORE, FCOPY}, {INVERT2MC, IGNORE},  /* BUILD, DROP */
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
	(dep->build_type == EXISTS_NODE)) {
	/* Although most exists nodes should have been removed, we may
	 * be facing the situtation where a fallback node has been
	 * rendered harmless, so this quick exit is still needed. */

	return;
    }
    action = redirect_action[node->build_type][dep->build_type][direction];
    switch(action) {
    case REVCOPYC:
	/* This is a backward dependency that should be reversed and
	 * applied to the drop side of the DAG conditionally (if it
	 * exists). */
	mirror = node->mirror_node;
	dmirror = dep->mirror_node;
	if (mirror && dmirror) {
	    addDepToVector(&(dmirror->tmp_fdeps), mirror);
	}
	return;
    case FMCOPY:
	/* This is a forward dependency on the dep's mirror, that should
	 * be copied. */
	addDepToVector(&node->tmp_fdeps, dep->mirror_node);
	return;
    case FCOPY:
	/* This is a forward dependency that should be copied.  We use
	 * tmp_fdeps so that we don't stomp over partial results.  The
	 * tmp_fdeps vector will be copied over the forward_deps vector
	 * later. 
	 */
	addDepToVector(&node->tmp_fdeps, dep);
	return;
    case INVERT:
	/* This is a backawrd dependency that should be converted into a
	 * dependency in the opposite direction.
	 */
	addDepToVector(&dep->tmp_fdeps, node);
	return;
    case MINVERTC:
	/* This is a backawrd dependency that should be converted to a
	 * dependency from the mirror conditionally (if there is one).
	 */
	dmirror = dep->mirror_node;
	if (!dmirror) {
	    dmirror = dep;
	}
	addDepToVector(&dmirror->tmp_fdeps, node);
	return;
    case INVERT2MC:
	/* This is a backawrd dependency that should be converted into a
	 * dependency on the mirror.
	 */
	mirror = node->mirror_node;
	if (!mirror) {
	    mirror = node;
	}
	addDepToVector(&dep->tmp_fdeps, mirror);
	return;
    case UNSURE:
	/* This is for debugging only. */
	fprintf(stderr, "UNSURE: %d\n", direction);
	dbgSexp(node);
	dbgSexp(dep);
    case IGNORE:
	return;
    }
    showDeps(node);
    showDeps(dep);
    RAISE(DEPS_ERROR, 
	  newstr("%s dep from (%s) %s to (%s) %s not handled.", 
		 direction? "Forward": "Backward",
		 nameForBuildType(node->build_type),
		 node->fqn->value, 
		 nameForBuildType(dep->build_type),
		 dep->fqn->value));
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
    DagNode *rnode;
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
    Vector *fallback_nodes = vectorNew(10);

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
	    addDepToVector(&node->forward_deps, node->mirror_node);

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
    
//    showVectorDeps(nodes);
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

DagNode *
getNode(Hash *byfqn, char *fqn)
{
    String *key = stringNew(fqn);
    DagNode *node = (DagNode *) hashGet(byfqn, (Object *) key);;
    objectFree((Object *) key, TRUE);
    return node;
}

/*
showFallbackDeps(Hash *byfqn)
{
    DagNode *node;
    node = getNode(byfqn, "fallback.grant.wibble.superuser");
    showDeps(node);
    if (node->mirror_node) {
	showDeps(node->mirror_node);
    }
    node = getNode(byfqn, "table.regressdb.public.thing");
    showDeps(node);
    if (node->mirror_node) {
	showDeps(node->mirror_node);
    }
    node = getNode(byfqn, "role.cluster.wibble");
    showDeps(node);
    if (node->mirror_node) {
	showDeps(node->mirror_node);
    }
    }*/

/* Create a Dag from the supplied doc, returning it as a vector of DocNodes.
 * See the file header comment for a more detailed description of what
 * this does.
 */
Vector *
dagFromDoc(Document *doc)
{
   Vector *volatile nodes = dagNodesFromDoc(doc);
   Hash *volatile byfqn = hashByFqn(nodes);
   Hash *volatile bypqn = NULL;
   DagNode *volatile this = NULL;
   int volatile i;
   
   BEGIN {
//dbgSexp(doc);
       identifyParents(nodes, byfqn);
       bypqn = hashByPqn(nodes);

       addDependencies(nodes, byfqn, bypqn);
//fprintf(stderr, "============INITIAL==============\n");
//showVectorDeps(nodes);

       resolveGraphs(nodes);
//fprintf(stderr, "============RESOLVED==============\n");
//showVectorDeps(nodes);

       promoteRebuilds(nodes);
       createMirrorNodes(nodes);
       redirectDeps(nodes);

//fprintf(stderr, "============REDIRECTED==============\n");
//showVectorDeps(nodes);

       swapBackwardBreakers(nodes);
//fprintf(stderr, "============SWAPPED==============\n");
//showVectorDeps(nodes);

   }
   EXCEPTION(ex) {
       objectFree((Object *) nodes, TRUE);
       objectFree((Object *) bypqn, TRUE);
       objectFree((Object *) byfqn, FALSE);
   }
   END;
   objectFree((Object *) bypqn, TRUE);
   objectFree((Object *) byfqn, FALSE);

   return nodes;
}
