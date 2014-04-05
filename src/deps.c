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


#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"

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
	/* By default, we don't want to do anything for a fallback
 	 * node.  Marking it as an exists node achieves that.  The
 	 * build_type will be modified if anything needs to actually
 	 * reference the fallback node.  */
	build_type = EXISTS_NODE;
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
findNodeDep(xmlNode *node, Hash *byfqn, Hash *bypqn, DagNode *dagnode)
{
    Object *result = NULL;
    String *qn;
    if (qn = nodeAttribute(node, "fqn")) {
	result = hashGet(byfqn, (Object *) qn);
	objectFree((Object *) qn, TRUE);
    }
    else if (qn = nodeAttribute(node, "pqn")) {
	result = hashGet(bypqn, (Object *) qn);
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
    else {
	RAISE(TSORT_ERROR, newstr("Not implemented: (in containsExistsNode)"));
	return FALSE;
    }
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

static void 
recordNodeDeps(DagNode *node, Vector *all_nodes, 
	       Hash *byfqn, Hash *bypqn, Vector *depsets);

static DagNode *
makeFallbackNode(String *fqn, xmlNode *depnode, Hash *byfqn, 
		 Hash *bypqn, Vector *all_nodes, Vector *depsets)
{
    xmlNode *dbobj;
    DagNode *fallback;
    DagNode *parent;

    dbobj = domakeFallbackNode(fqn);
    fallback = dagNodeNew(dbobj, INACTIVE_NODE);
    parent = parentNodeForFallback(depnode, byfqn);
    setParent(fallback, parent);
    hashAdd(byfqn, (Object *) fqn, (Object *) fallback);
    vectorPush(all_nodes, (Object *) fallback);
    fallback->deps = vectorNew(10);
    recordNodeDeps(fallback, all_nodes, byfqn, bypqn, depsets);

    return fallback;
}

static void
getFallbackForDepset(DependencySet *depset, xmlNode *depnode, Hash *byfqn, 
		     Hash *bypqn, Vector *all_nodes, Vector *depsets)
{
    String *fallback = NULL;
    DagNode *fallback_node;

    if (fallback = nodeAttribute(depnode, "fallback")) {
	fallback_node = (DagNode *) hashGet(byfqn, (Object *) fallback);
	if (!fallback_node) {
	    fallback_node = makeFallbackNode(fallback, depnode, byfqn, 
					     bypqn, all_nodes, depsets);
	}
	else {
	    objectFree((Object *) fallback, TRUE);
	}
	depset->fallback = fallback_node;
    }
}

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
activateFallback(DependencySet *depset, Vector *nodes)
{
    DagNode *fallback;
    DagNode *endfallback;
    Dependency *newdep;
    int i;
    char * endfqn;

    if (fallback = depset->fallback) {
	if (fallback->build_type == INACTIVE_NODE) {
	    endfallback = dagNodeNew(fallback->dbobject, ENDFALLBACK_NODE);
	    endfallback->deps = vectorNew(10);
	    endfqn = newstr("end%s", endfallback->fqn->value);
	    skfree(endfallback->fqn->value);
	    endfallback->fqn->value = endfqn;
	    endfallback->parent = fallback->parent;
	    fallback->mirror_node = endfallback;
	    endfallback->mirror_node = fallback;
	    vectorPush(nodes, (Object *) endfallback);

	    EACH(fallback->deps, i) {
		newdep = copyDependency((Dependency *) ELEM(fallback->deps, i));
		vectorPush(endfallback->deps, (Object *) newdep);
	    }


	    /* Add dependencies from endfallback to fallback. */
	    newdep = dependencyNew(fallback);
	    newdep->direction = BOTH_DIRECTIONS;
	    newdep->immutable = TRUE;
	    vectorPush(endfallback->deps, (Object *) newdep);
	    fallback->build_type = FALLBACK_NODE;
	    endfallback->build_type = ENDFALLBACK_NODE;
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
static void
recordDepElement(DagNode *node, xmlNode *depnode, Vector *all_nodes, 
		 Hash *byfqn, Hash *bypqn, Vector *depsets, Object *target, 
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
            recordDepElement(node, this, all_nodes, byfqn, bypqn, 
			     depsets, target, NULL, TRUE);
        }
    }
    else if (isDependency(depnode)) {
        if (deps = findNodeDep(depnode, byfqn, bypqn, node)) {
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
	deps = (Object *) (depset = dependencySetNew());
	vectorPush(depsets, deps);

	getFallbackForDepset(depset, depnode, byfqn, bypqn, 
			     all_nodes, depsets);

        while (this = nextDependency(depnode->children, this)) {
            recordDepElement(node, this, all_nodes, byfqn, bypqn, depsets,
			     (Object *) depset, &contains_exists, FALSE);
        }
	if (depset->deps->elems == 0) {
	    if (fallback = activateFallback(depset, all_nodes)) {
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
		addDepToTarget((Object *) fallback->mirror_node,
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
recordNodeDeps(DagNode *node, Vector *all_nodes, 
	       Hash *byfqn, Hash *bypqn, Vector *depsets)
{
    xmlNode *depnode = NULL;
    while (depnode = nextDependency(node->dbobject->children, depnode)) {
	recordDepElement(node, depnode, all_nodes, byfqn, bypqn, 
			 depsets, (Object *) node, NULL, TRUE);
    }
}


static void
identifyDependencies(Vector *nodes, Hash *byfqn, Hash *bypqn, Vector *depsets)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (!node->deps) {
	    node->deps = vectorNew(10);
	    recordNodeDeps(node, nodes, byfqn, bypqn, depsets);
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
transformForDep(DagNode *node, Object *depobj)
{
    DepTransform transform;
    DagNode *dep;

    assert(depobj->type == OBJ_DEPENDENCY,
	  "transformForDep: unexpected depobj->type: %d.", depobj->type);
    dep = ((Dependency *) depobj)->dep;

    if ((node->build_type == EXISTS_NODE) ||
	(node->build_type == INACTIVE_NODE) ||
	(dep->build_type == INACTIVE_NODE) ||
	(dep->build_type == EXISTS_NODE)) 
    {
	transform = DROP;
    }
    else {
	transform = 
	  transform_for_build_types[node->build_type][dep->build_type];

	if (transform == ERROR) {
	    dbgSexp(node);
	    dbgSexp(depobj);
	    RAISE(NOT_IMPLEMENTED_ERROR, 
		  newstr("transformForEndDep: no handler for deps "
			 "from build_type %d to build_type %d", 
			 node->build_type, dep->build_type));
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

static void
tmpifyNodeDep(DagNode *node, Object *depobj)
{
    Dependency *dep = (Dependency *) depobj;
    DependencySet *depset;
    int i;

    if (dep->type == OBJ_DEPENDENCY) {
	if ((dep->direction == FORWARDS) ||
	    (dep->direction == BOTH_DIRECTIONS)) 
	{
	    myVectorPush(&(node->tmp_deps), (Object *) dep);
	}
	else {
	    objectFree((Object *) dep, TRUE);
	}
    }
    else if (dep->type == OBJ_DEPENDENCYSET) {
	depset = (DependencySet *) depobj;
	EACH(depset->deps, i) {
	    tmpifyNodeDep(node, ELEM(depset->deps, i));
	}
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("tmpifyNodeDep: no handler for deps of type %d",
		     dep->type));
    }
}

static void
dropNodeDep(DagNode *node, Object *depobj)
{
    Dependency *dep = (Dependency *) depobj;

    if (dep->type == OBJ_DEPENDENCY) {
	objectFree((Object *) dep, TRUE);
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("dropNodeDep: no handler for deps of type %d",
		     dep->type));
    }
}

static void
invertNodeDep(DagNode *node, Object *depobj)
{
    Dependency *dep = (Dependency *) depobj;
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

static void
dupAndInvertNodeDep(DagNode *node, Object *depobj)
{
    Dependency *dep = (Dependency *) depobj;
    Dependency *dup;

    if (dep->type == OBJ_DEPENDENCY) {
	dup = dependencyNew(mirrorOrThis(dep->dep));
	dup->direction = dep->direction;
	tmpifyNodeDep(node, depobj);
	invertNodeDep(node->mirror_node, (Object *) dup);
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("dupAndInvertNodeDep: no handler for deps of type %d",
		     dep->type));
    }
}

static void
redirectNodeDeps(DagNode *node)
{
    Object *depobj;
    int i;
    DepTransform transform;

    EACH(node->deps, i) {
	depobj = ELEM(node->deps, i);
	transform = transformForDep(node, depobj);
	switch (transform) {
	case RETAIN: 
	    tmpifyNodeDep(node, depobj);
	    break;
	case INVERT:
	    invertNodeDep(node, depobj);
	    break;
	case DUPANDINVERT:
	    dupAndInvertNodeDep(node, depobj);
	    break;
	case DROP:
	    dropNodeDep(node, depobj);
	    break;
	default: 
	    dbgSexp(node);
	    dbgSexp(depobj);
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
redirectDependencies(Vector *nodes)
{
    int i;
    DagNode *node;
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	redirectNodeDeps(node);
    }
    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
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

		if (dep->depset) {
		    if (dep == dep->depset->chosen_dep) {
			/* For deps in depsets only add the dependency
			 * if it was chosen by resolveNodes(). */
			setPush(newvec, (Object *) dep->dep);
		    }
		}
		else {
		    setPush(newvec, (Object *) dep->dep);
		}
		objectFree((Object *) dep, TRUE);
	    }
	    objectFree((Object *) node->deps, FALSE);
	    node->deps = newvec;
	}
    }
}

typedef enum {
    ALL_IS_WELL = 0,
    CYCLIC_EXCEPTION
} resolver_status;

typedef struct resolver_info {
    DagNode         *cycle_node;
    char            *errmsg;
} resolver_info;

static resolver_status
resolveNode(DagNode *node, Vector *all_nodes, resolver_info *info);

static resolver_status
resolveDeps(DagNode *node, Vector *all_nodes, resolver_info *info)
{
    int i;
    Dependency *dep;
    DependencySet *depset;
    resolver_status status;

    EACH (node->deps, i) {
	dep = (Dependency *) ELEM(node->deps, i);
	if (depset = dep->depset) {
	    if (!depset->chosen_dep) {
		/* Try this dep. */
		status = resolveNode(dep->dep, all_nodes, info);
		if (status == CYCLIC_EXCEPTION) {
		    /* We will attempt to find other deps in this
		     * depset, or maybe use the fallback to resolve the
		     * cycle. */
		    skfree(info->errmsg);
		    info->errmsg = NULL;
		    info->cycle_node = NULL;
		    status = ALL_IS_WELL;
		}
		else if (status == ALL_IS_WELL) {
		    depset->chosen_dep = dep;
		}
	    }
	}
	else {
	    status = resolveNode(dep->dep, all_nodes, info);
	}
	if (status) {
	    return status;
	}
    }
    return ALL_IS_WELL;
}

static resolver_status
resolveNode(DagNode *node, Vector *all_nodes, resolver_info *info)
{
    resolver_status status;
    char *tmp;
    switch (node->status) {
    case VISITED:
	/* This node has already been resolved, so nohing more to do. */
	return ALL_IS_WELL;
    case VISITING:
	info->errmsg = newstr("%s", node->fqn->value);
	info->cycle_node = node;
	return CYCLIC_EXCEPTION;
    case UNVISITED:
	node->status = VISITING;
	status = resolveDeps(node, all_nodes, info);
	if (status == CYCLIC_EXCEPTION) {
	    /* Add more information to info->errmsg. */
	    tmp = newstr("%s->%s", node->fqn->value, info->errmsg);
	    skfree(info->errmsg);
	    info->errmsg = tmp;
	}
	node->status = VISITED;
	return status;
    default: 
	RAISE(TSORT_ERROR, 
	      newstr("Unexpected node status (%d) for node %s", 
		     node->status, node->fqn->value));
	return ALL_IS_WELL;
    }
}

/* This traverses the DAG, picking one dep from each depset (
 * optionally creating fallbacks), and adding cycle breakers when
 * necessary.
 */
static void
resolveNodes(Vector *nodes)
{
    int i;
    DagNode *node;
    resolver_info info = {NULL, NULL};
    resolver_status status;

    EACH(nodes, i) {
	node = (DagNode *) ELEM(nodes, i);
	if (status = resolveNode(node, nodes, &info)) {
	    fprintf(stderr, "ERROR: %s\n", info.errmsg);
	    RAISE(NOT_IMPLEMENTED_ERROR,
		  newstr("resolveNodes: handling of errors."));
	}
    }
 
    convertDependencies(nodes);
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

/*
 * Create a Dag from the supplied doc, returning it as a vector of DocNodes.
 * See the file header comment for a more detailed description of what
 * this does.
 */
Vector *
dagFromDoc(Document *doc)
{
    Vector *volatile nodes = dagNodesFromDoc(doc);
    Hash *volatile byfqn = NULL;
    Hash *volatile bypqn = NULL;
    Vector *volatile depsets = vectorNew(nodes->elems * 2);

    BEGIN {
	byfqn = hashByFqn(nodes);
	bypqn = hashByPqn(nodes);
	identifyDependencies(nodes, byfqn, bypqn, depsets);
	makeMirrors(nodes);

	//showVectorDeps(nodes);
	//fprintf(stderr, "---------------------\n\n");

	// TODO: promote rebuilds; remove deps involving an EXISTS_NODE
	redirectDependencies(nodes);
	resolveNodes(nodes);
    }
    EXCEPTION(ex) {
	freeDeps(nodes);
	objectFree((Object *) depsets, TRUE);
	objectFree((Object *) nodes, TRUE);
	objectFree((Object *) byfqn, FALSE);
	objectFree((Object *) bypqn, TRUE);
    }
    END;

    objectFree((Object *) depsets, TRUE);
    objectFree((Object *) bypqn, TRUE);
    objectFree((Object *) byfqn, FALSE);
    return nodes;
}
