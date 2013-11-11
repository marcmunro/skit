/**
 * @file   navigation.c
 * \code
 *     Copyright (c) 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for navigating between topologically sorted nodes.
 */

#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"


/* Predicate identifying whether a node is of a specific type.
 */
static boolean
xmlnodeMatch(xmlNode *node, char *name)
{
    return node && (node->type == XML_ELEMENT_NODE) && streq(node->name, name);
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
    if (parent) {
	if (xmlnodeMatch(parent->children, name)) {
	    return parent->children;
	}
	return findNextSibling(parent->children, name);
    }
    return NULL;
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
    return nodeHasAttribute(node, "visit");
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

static DagNode *
departNode(DagNode *current)
{
    DagNode *navigation = NULL;
    xmlNode *newnode;

    if (requiresNavigation(current->dbobject)) {
	newnode = copyObjectNode(current->dbobject);
	navigation = dagNodeNew(newnode, DEPART_NODE);
    }
    return navigation;
}

static DagNode *
arriveNode(DagNode *target)
{
    DagNode *navigation = NULL;
    xmlNode *newnode;

    if (requiresNavigation(target->dbobject)) {
	newnode = copyObjectNode(target->dbobject);
	navigation = dagNodeNew(newnode, ARRIVE_NODE);
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
getContextsOld(DagNode *node)
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

static Cons *
getContexts(xmlNode *node)
{
    xmlNode *context_node;
    Cons *cell;
    Cons *contexts = NULL;
    String *name;
    String *value;
    String *dflt;

    if (node) {
	for (context_node = findFirstChild(node, "context");
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
arriveContextNodeOld(String *name, String *value)
{
    xmlNode *dbobject = dbobjectNode(name->value, value->value);
    return dagNodeNew(dbobject, ARRIVE_NODE);
}

static Node *
newContextNode(String *name, String *value)
{
    xmlNode *dbobject = dbobjectNode(name->value, value->value);
    return nodeNew(dbobject);
}

static DagNode *
departContextNodeOld(String *name, String *value)
{
    xmlNode *dbobject = dbobjectNode(name->value, value->value);
    return dagNodeNew(dbobject, DEPART_NODE);
}

static void
addArriveContextOld(Vector *vec, Cons *context)
{
    String *name = (String *) context->car;
    Cons *cell2 = (Cons *) context->cdr;
    DagNode *context_node;

    /* Do not close the context, if it is the default. */
    if (objectCmp(cell2->car, cell2->cdr) != 0) {
	context_node = arriveContextNodeOld(name, (String *) cell2->car);
	vectorPush(vec, (Object *) context_node);
    }
}

static void
addDepartContextOld(Vector *vec, Cons *context)
{
    String *name = (String *) context->car;
    Cons *cell2 = (Cons *) context->cdr;
    DagNode *context_node;

    /* Do not close the context, if it is the default. */
    if (objectCmp(cell2->car, cell2->cdr) != 0) {
	context_node = departContextNodeOld(name, (String *) cell2->car);
	vectorPush(vec, (Object *) context_node);
    }
}

static void
addContext(Vector *vec, Cons *context)
{
    String *name = (String *) context->car;
    Cons *cell2 = (Cons *) context->cdr;
    Node *context_node;

    /* Do not close the context, if it is the default. */
    if (objectCmp(cell2->car, cell2->cdr) != 0) {
	context_node = newContextNode(name, (String *) cell2->car);
	vectorPush(vec, (Object *) context_node);
    }
}

static Cons *
getContextNavigationOld(DagNode *from, DagNode *target)
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
    from_contexts = getContextsOld(from);
    target_contexts = getContextsOld(target);

    while (target_contexts && (this = (Cons *) consPop(&target_contexts))) {
	name = (String *) this->car;
	if (from_contexts &&
	    (match = (Cons *) alistExtract(&from_contexts, 
					   (Object *) name))) {
	    /* We have the same context for both DagNodes. */
	    this2 = (Cons *) this->cdr;
	    match2 = (Cons *) match->cdr;
	    if (objectCmp(this2->car, match2->car) != 0) {
		/* Depart the old context, and arrive at the new. */
		addDepartContextOld(departures, match);
		addArriveContextOld(arrivals, this);
	    }
	    objectFree((Object *) match, TRUE);
	}
	else {
	    /* This is a new context. */
	    addArriveContextOld(arrivals, this);
	}
	objectFree((Object *) this, TRUE);
    }
    while (from_contexts && (this = (Cons *) consPop(&from_contexts))) {
	/* Close the final contexts.  Unless we are in a default
	 * context. */ 
	addDepartContextOld(departures, this);
	objectFree((Object *) this, TRUE);
    }
    return result;
}

static Cons *
getContextNavigation(xmlNode *from, xmlNode *target)
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
	    /* We have the same context for both DagNodes. */
	    this2 = (Cons *) this->cdr;
	    match2 = (Cons *) match->cdr;
	    if (objectCmp(this2->car, match2->car) != 0) {
		/* Depart the old context, and arrive at the new. */
		addContext(departures, match);
		addContext(arrivals, this);
	    }
	    objectFree((Object *) match, TRUE);
	}
	else {
	    /* This is a new context. */
	    addContext(arrivals, this);
	}
	objectFree((Object *) this, TRUE);
    }
    while (from_contexts && (this = (Cons *) consPop(&from_contexts))) {
	/* Close the final contexts.  Unless we are in a default
	 * context. */ 
	addContext(departures, this);
	objectFree((Object *) this, TRUE);
    }
    return result;
}


/* Return a vector of DagNodes containing the navigation to get from
 * start to target.  All of the dbojects returned in the DagNode
 * Vectors must be orphans so that they can be safely added to the
 * appropriate parent node. */
/* Return a vector of DagNodes containing the navigation to get from
 * start to target.  All of the dbojects returned in the DagNode
 * Vectors must be orphans so that they can be safely added to the
 * appropriate parent node. */
Vector *
navigationToNode(DagNode *start, DagNode *target)
{
    Cons *context_nav;
    Vector *volatile results;
    Vector *context_arrivals = NULL;
    Object *elem;
    DagNode *current = NULL;
    DagNode *common_root = NULL;
    DagNode *navigation = NULL;
    Symbol *ignore_contexts = symbolGet("ignore-contexts");
    boolean handling_context = (ignore_contexts == NULL);

    BEGIN {
	if (handling_context) {
	    context_nav = getContextNavigationOld(start, target);

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
    return results;
}

/* Find the parent dboject node for a given dbobject.
 */
static Node *
getParent(xmlNode *dbobject, Hash *byfqn)
{
    Object *volatile key = NULL;
    Node *result = NULL;

    if (key = (Object *) nodeAttribute(dbobject, "parent")) {
	BEGIN {
	    result = (Node *) hashGet(byfqn, key);
	    assert(result, "No parent dbobject found for %s", 
		   ((String *) key)->value);
	}
	EXCEPTION(ex);
	FINALLY {
	    objectFree(key, TRUE);
	}
	END;
    }
    return result;
}

/* Return a vector of fqns (Strings) describing the ancestry of
 * dbobject, starting from the node and working up.
 */
static Vector *
getAncestry(xmlNode *dbobject, Hash *byfqn)
{
    Vector *result = vectorNew(10);
    String *fqn;
    Node *ancestor;

    BEGIN {
	while (dbobject) {
	    fqn = nodeAttribute(dbobject, "fqn");
	    assert(fqn, "assumed dbobject has no fqn");
	    vectorPush(result, (Object *) fqn);

	    if (ancestor = getParent(dbobject, byfqn)) {
		dbobject = ancestor->node;
		assert(dbobject, "missing xmlnode in Node");
	    }
	    else {
		break;
	    }
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) result, TRUE);
    }
    END;
    return result;
}

/* Identify the nearest common ancestor from 2 vectors of fqn Strings. */
static String *
commonAncestor(Vector *vec1, Vector *vec2)
{
    String *result = NULL;
    int i1 = vec1->elems;
    int i2 = vec2->elems;

    while (i1 && i2) {
	i1--, i2--;
	if (!stringeq((String *) ELEM(vec1, i1), (String *) ELEM(vec2, i2))) {
	    return result;
	}
	result = (String *) ELEM(vec1, i1);
    }
    return result;
}

static boolean
fqnEq(xmlNode *node1, xmlNode *node2)
{
    xmlChar *fqn1 = xmlGetProp(node1, "fqn");
    xmlChar *fqn2 = xmlGetProp(node2, "fqn");
    boolean result = streq(fqn1, fqn2);
    xmlFree(fqn1);
    xmlFree(fqn2);
    return result;
}


/* Add departure navigation nodes to the departures vector. */
static void
departFrom(
    xmlNode *nav_from,
    Vector *ancestry, 
    String *ancestor, 
    Vector *departures,
    Hash *byfqn)
{
    int i;
    String *this;
    Node *depart_from;
    Node *nav_node;
    xmlNode *new;

    EACH(ancestry, i) {
	this = (String *) ELEM(ancestry, i);
	if (ancestor && stringeq(this, ancestor)) {
	    /* We are done, we have reached our ancestor. */
	    break;
	}
	/* Add a departure from this into our departures vector. */
	depart_from = (Node *) hashGet(byfqn, (Object *) this);
	
	if (! fqnEq(nav_from, depart_from->node)) {
	    /* If this is the nav_from node we don't need to depart from
	       it.  The node's own action should do that for us. */
	    new = xmlCopyNode(depart_from->node, 2);
	    nav_node = nodeNew(new);
	    vectorPush(departures, (Object *) nav_node);
	}
    }
}

/* Add arrival navigation nodes to the arrivals vector. */
static void
arriveAt(
    xmlNode *nav_to,
    Vector *ancestry, 
    String *ancestor, 
    Vector *arrivals,
    Hash *byfqn)
{
    int i;
    String *this;
    Node *arrive_at;
    Node *nav_node;
    xmlNode *new;

    EACH(ancestry, i) {
	this = (String *) ELEM(ancestry, i);
	if (ancestor && stringeq(this, ancestor)) {
	    /* We are done, we have reached our ancestor. */
	    break;
	}
	/* Add an arrival to this into our arrivals vector. */
	arrive_at = (Node *) hashGet(byfqn, (Object *) this);
	if (! fqnEq(nav_to, arrive_at->node)) {
	    /* If this is the nav_to node we don't need to arrive at
	       it.  The node's own action should do that for us. */
	    new = xmlCopyNode(arrive_at->node, 2);
	    nav_node = nodeNew(new);
	    vectorInsert(arrivals, (Object *) nav_node, 0);
	}
    }
}


/* Given from = a.b.c and to = a.d.e.f, our navigation would be
 * departures = [b], arrivals = [d, e]
 */
static void
getNodeNavigation(
    xmlNode *nav_from, 
    xmlNode *nav_to, 
    Hash *byfqn,
    Vector *arrivals,
    Vector *departures)
{
    Vector *volatile from_ancestry = getAncestry(nav_from, byfqn);
    Vector *volatile to_ancestry = NULL;
    String *common_ancestor;

    //dbgNode(nav_from);
    //dbgNode(nav_to);
    //dbgSexp(from_ancestry);
    BEGIN {
	to_ancestry = getAncestry(nav_to, byfqn);
	//dbgSexp(to_ancestry);
	common_ancestor = commonAncestor(from_ancestry, to_ancestry);
	//dbgSexp(common_ancestor);
	departFrom(nav_from, from_ancestry, common_ancestor, departures, byfqn);
	arriveAt(nav_to, to_ancestry, common_ancestor, arrivals, byfqn);
	//dbgSexp(departures);
	//dbgSexp(arrivals);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) from_ancestry, TRUE);
	objectFree((Object *) to_ancestry, TRUE);
    }
    END
}


static Hash *
makeNodesHash(Vector *nodes)
{
    Hash *byfqn = hashNew(TRUE);
    String *fqn;
    Node *node;
    int i;
    EACH(nodes, i) {
	node = (Node *) ELEM(nodes, i);
	fqn = nodeAttribute(node->node, "fqn");
	hashAdd(byfqn, (Object *) fqn, (Object *) node);
   }
   return byfqn;
}

static void
doAddNavigation(
    xmlNode *parent, 
    xmlNode *nav_from, 
    xmlNode *nav_to, 
    Hash *byfqn,
    boolean handle_contexts)
{
    Cons *context_nav;
    Vector *volatile departures = NULL;
    Vector *volatile arrivals = NULL;
    Node *nav;
    xmlNode *new;
    int i;

    if (handle_contexts) {
	context_nav = getContextNavigation(nav_from, nav_to);
	departures = (Vector *) context_nav->car;
	arrivals = (Vector *) context_nav->cdr;
	objectFree((Object *) context_nav, FALSE);
    }
    else
    {
	arrivals = vectorNew(10);
	departures = vectorNew(10);
    }

    BEGIN {
	getNodeNavigation(nav_from, nav_to, byfqn, arrivals, departures);

	//fprintf(stderr, "-----------------------\n");
	if (nav_from) {
	    EACH(departures, i) {
		nav = (Node *) ELEM(departures, i);
		new = nav->node;
		xmlSetProp(new, "action", "depart");
		xmlAddChild(parent, new);
		nav->node = NULL;
		nav_from = new;
	    }
	}
	
	if (nav_to) {
	    EACH(arrivals, i) {
		nav = (Node *) ELEM(arrivals, i);
		new = nav->node;
		xmlSetProp(new, "action", "arrive");
		xmlAddChild(parent, new);
		nav->node = NULL;
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	xmlAddChild(parent, nav_to);
	objectFree((Object *) departures, TRUE);
	objectFree((Object *) arrivals, TRUE);
    }
    END;
}

static boolean
nodeHasPrintElement(xmlNode *node)
{
    while (node = getNextNode(node)) {
	if (streq(node->name, "print")) {
	    return TRUE;
	}
	if (nodeHasPrintElement(node->children)) {
	    return TRUE;
	}
	node = node->next;
    }
    return FALSE;
}

void
addNavigationToDoc(
    xmlNode *parent_node, 
    Vector *nodes,
    boolean handle_contexts)
{
    Node *this;
    xmlNode *nav_from;
    xmlNode *nav_to = NULL;
    int i;
    Hash *byfqn = makeNodesHash(nodes);

    BEGIN {
	EACH (nodes, i) {
	    this = (Node *) ELEM(nodes, i);

	    if (nodeHasPrintElement(this->node)) {
		/* If this node contains no <print> elements, there is
		 * no work to be done for it, so no point in adding any
		 * navigation. */
		nav_from = nav_to;
		nav_to = this->node;
		doAddNavigation(parent_node, nav_from, nav_to, 
				byfqn, handle_contexts);
	    }
	}

	if (nav_to) {
	    doAddNavigation(parent_node, nav_to, NULL, 
			    byfqn, handle_contexts);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) byfqn, FALSE);
    }
    END;
}
