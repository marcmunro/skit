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
    return node && (node->type == XML_ELEMENT_NODE) && 
	streq((char *) node->name, name);
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
getContexts(xmlNode *node)
{
    xmlNode *context_node;
    Cons *cell;
    Cons *contexts = NULL;
    String *type;
    String *value;
    String *dflt;

    if (node) {
	for (context_node = findFirstChild(node, "context");
	     context_node;
	     context_node = findNextSibling(context_node, "context")) {
	    type = nodeAttribute(context_node, "type");
	    assert(type, "Missing type attribute for context");
	    value = nodeAttribute(context_node, "value");
	    dflt = nodeAttribute(context_node, "default");
	    cell = consNew((Object *) type, 
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

static Node *
newContextNode(String *name, String *value)
{
    xmlNode *dbobject = dbobjectNode(name->value, value->value);
    return nodeNew(dbobject);
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
getContextNavigation(xmlNode *from, xmlNode *target)
{
    Cons *from_contexts;
    Cons *target_contexts;
    Cons *this;
    Cons *this2;
    Cons *match;
    Cons *match2;
    String *type;
    Vector *departures = vectorNew(10);
    Vector *arrivals = vectorNew(10);
    Cons *result = consNew((Object *) departures, (Object *) arrivals);

    /* Contexts are lists of the form: (type value default) */
    from_contexts = getContexts(from);
    target_contexts = getContexts(target);

    while (target_contexts && (this = (Cons *) consPop(&target_contexts))) {
	type = (String *) this->car;
	if (from_contexts &&
	    (match = (Cons *) alistExtract(&from_contexts, 
					   (Object *) type))) {
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
	    context_nav = getContextNavigation(start->dbobject, 
					       target->dbobject);

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

static boolean
nodesMatch(Node *node1, Node *node2)
{
    String *node1_fqn;
    String *node2_fqn;
    boolean result = FALSE;
    if (node1 && node2) {
	node1_fqn = nodeAttribute(node1->node, "fqn"); 
	node2_fqn = nodeAttribute(node2->node, "fqn");
	result = streq(node1_fqn->value, node2_fqn->value);
	objectFree((Object *) node1_fqn, TRUE);
	objectFree((Object *) node2_fqn, TRUE);
    }
    return result;
}

/* Add departure navigation nodes to the departures vector. */
static void
departFrom(Node *nav_from, Node *ancestor, Vector *departures)
{
    Node *this_node;
    xmlNode *new;
    Node *nav_node;
    String *action = nodeAttribute(nav_from->node, "action");

    if (streq(action->value, "drop")) {
	/* We do not depart from a node we are dropping. */
	this_node = nav_from->parent;
	
    }
    else {
	this_node = nav_from;
    }
    while (this_node && !nodesMatch(this_node, ancestor)) {
	/* Add a departure from this into our departures vector. */
	new = xmlCopyNode(this_node->node, 2);
	nav_node = nodeNew(new);
	vectorPush(departures, (Object *) nav_node);

	this_node = this_node->parent;
    }
    objectFree((Object *) action, TRUE);
}

/* Add arrival navigation nodes to the arrivals vector. */
static void
arriveAt(Node *nav_to, Node *ancestor, Vector *arrivals)
{
    Node *this_node;
    xmlNode *new;
    Node *nav_node;

    this_node = nav_to->parent;
    while (this_node && !nodesMatch(this_node, ancestor)) {
	/* Add a departure from this into our departures vector. */
	new = xmlCopyNode(this_node->node, 2);
	nav_node = nodeNew(new);
	vectorInsert(arrivals, (Object *) nav_node, 0);

	this_node = this_node->parent;
    }
}

static Vector *
ancestryStack(Node *node)
{
    Vector *result = vectorNew(10);
    while (node) {
	vectorPush(result, (Object *) node);
	node = node->parent;
    }
    return result;
}

static Node *
findMatchingNodeInStack(Node *node, Vector *stack)
{
    Node *this;
    int i;
    for (i = stack->elems - 1; i >= 0; i--) {
	this = (Node *) ELEM(stack, i);
	if (nodesMatch(node, this)) {
	    return this;
	}
    }
    return NULL;
}

static Node*
commonAncestor(Node *nav_from, Node *nav_to)
{
    Node *result = NULL;
    Vector *from_stack;
    Vector *to_stack;
    Node *ancestor;
    if (nav_from && nav_to) {
	from_stack = ancestryStack(nav_from);
	to_stack = ancestryStack(nav_to);
	
	while (ancestor = (Node *) vectorPop(from_stack)) {
	    if (findMatchingNodeInStack(ancestor, to_stack)) {
		result = ancestor;
		break;
	    }
	}

	objectFree((Object *) from_stack, FALSE);
	objectFree((Object *) to_stack, FALSE);
    }
    return result;
}

static void
getNodeNavigation(
    Node *nav_from, 
    Node *nav_to, 
    Vector *arrivals,
    Vector *departures)
{
    Node *common_ancestor = commonAncestor(nav_from, nav_to);
    if (nav_from && !nodesMatch(nav_from, common_ancestor)) {
	departFrom(nav_from, common_ancestor, departures);
    }

    if (nav_to && !nodesMatch(nav_to, common_ancestor)) {
	arriveAt(nav_to, common_ancestor, arrivals);
    }
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
    Node *nav_from, 
    Node *nav_to)
{
    Cons *context_nav;
    Vector *volatile departures = NULL;
    Vector *volatile arrivals = NULL;
    Node *nav;
    xmlNode *new;
    int i;

    context_nav = getContextNavigation(nav_from? nav_from->node: NULL, 
				       nav_to? nav_to->node: NULL);
    departures = (Vector *) context_nav->car;
    arrivals = (Vector *) context_nav->cdr;
    objectFree((Object *) context_nav, FALSE);

    //fprintf(stderr, "\n\n");
    //dbgSexp(nav_from);
    //dbgSexp(nav_to);

    getNodeNavigation(nav_from, nav_to, arrivals, departures);

    //dbgSexp(departures);
    //dbgSexp(arrivals);
    if (nav_from) {
	EACH(departures, i) {
	    nav = (Node *) ELEM(departures, i);
	    new = nav->node;
	    xmlSetProp(new, (xmlChar *) "action", (xmlChar *) "depart");
	    xmlAddChild(parent, new);
	    nav->node = NULL;
	}
    }

    if (nav_to) {
	EACH(arrivals, i) {
	    nav = (Node *) ELEM(arrivals, i);
	    new = nav->node;
	    xmlSetProp(new, (xmlChar *) "action", (xmlChar *) "arrive");
	    xmlAddChild(parent, new);
	    nav->node = NULL;
	}
	xmlAddChild(parent, nav_to->node);
    }

    objectFree((Object *) departures, TRUE);
    objectFree((Object *) arrivals, TRUE);
}

static boolean
nodeHasPrintElement(xmlNode *node)
{
    while (node = getNextNode(node)) {
	if (streq((char *) node->name, "print")) {
	    return TRUE;
	}
	if (nodeHasPrintElement(node->children)) {
	    return TRUE;
	}
	node = node->next;
    }
    return FALSE;
}

/* Find a dbobject node by doing a depth first traversal of nodes. */
static xmlNode *
findDbobject(xmlNode *node)
{
    xmlNode *this = getElement(node);
    xmlNode *kid;

    while (this) {
	if (streq((char *) this->name, "dbobject")) {
	    return this;
	}
	if (kid = findDbobject(this->children)) {
	    return kid;
	}
	this = getElement(this->next);
    }
    return NULL;
}

static Vector *
vectorFromDoc(xmlNode *parent)
{
    xmlNode *dbobject;
    Vector *volatile vec = vectorNew(100);
    Hash *volatile by_fqn = hashNew(TRUE);
    String *fqn;
    String *parent_fqn;
    Node *node;
    int i;

    dbobject = findDbobject(parent);

    BEGIN {
	/* First pass, add each dbobject into the results vector, and
	   into the by_fqn hash. */
	while (dbobject) {
	    if (fqn = nodeAttribute(dbobject, "fqn")) {
		node = nodeNew(dbobject);
		vectorPush(vec, (Object *) node);
		(void) hashAdd(by_fqn, (Object *) fqn, (Object *) node);
	    }
	    else {
		RAISE(XML_PROCESSING_ERROR, newstr("No FQN found for node"));
	    }
	    dbobject = findDbobject(dbobject->next);
	}
	
	/* Second pass, remove each dbobject from the doc and add
	 * parents to the nodes vector. */
	EACH(vec, i) {
	    node = (Node *) ELEM(vec, i);
	    if (parent_fqn = nodeAttribute(node->node, "parent")) {
		node->parent = (Node *) hashGet(by_fqn, (Object *) parent_fqn);
		objectFree((Object *) parent_fqn, TRUE);
	    }
	    else {
		node->parent = NULL;
	    }
	    xmlUnlinkNode(node->node);
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) vec, TRUE);
	objectFree((Object *) by_fqn, FALSE);
    }
    END;

    objectFree((Object *) by_fqn, FALSE);
    return vec;
}

void
addNavigationToDoc(xmlNode *parent_node)
{
    Node *this;
    Node *node_from;
    Node *node_to = NULL;
    int i;
    Vector *nodes;
    Hash *byfqn;

    nodes = vectorFromDoc(parent_node);
    byfqn = makeNodesHash(nodes);
    EACH(nodes, i) {
	this = (Node *) ELEM(nodes, i);
	
	if (nodeHasPrintElement(this->node)) {
	    /* If this node contains no <print> elements, there is
	     * no work to be done for it, so no point in adding any
	     * navigation. */
	    node_from = node_to;
	    node_to = this;

	    if (node_from) {
		char *tmp = nodestr(node_from->node);
		if (streq(tmp, "<dbobject type=\"role\" name=\"lose\" "
			  "qname=\"lose\" fqn=\"role.lose\" "
			  "parent=\"database.regressdb\" diff=\"gone\" "
			  "action=\"drop\">")) 
		{
		    dbgSexp(node_from);
		    dbgSexp(node_to);
		}
		skfree(tmp);
	    }
	    doAddNavigation(parent_node, node_from, node_to);
	}
    }
	
    if (node_to) {
	doAddNavigation(parent_node, node_to, NULL);
    }
    //dNode(parent_node);
    objectFree((Object *) nodes, TRUE);
    objectFree((Object *) byfqn, FALSE);
}

