/**
 * @file   navigation.c
 * \code
 *     Copyright (c) 2011 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for navigating between topologically sorted nodes.
 */

#include <string.h>
#include "skit.h"
#include "exceptions.h"


/* If node is a dbobject node, find the first child node that is a
 * context node, otherwise, find the next sibling that is a context
 * node. 
 */
static xmlNode *
findContextNode(xmlNode *node)
{
    xmlNode *this;

    if (streq((char *) node->name, "dbobject")) {
	this = node->children;
    }
    else {
	this = node->next;
    }
    while (this) {
	this = getNextNode(this);
	if (this && streq((char *) this->name, "context")) {
	    break;
	}
	this = this->next;
    }
    return this;
}

/* This identifies which side of the DAG a particular dbobject node
 * falls on, based on its action attribute.
 */
static DependencyApplication
buildDirection(xmlNode *dbobject)
{
    String *action = nodeAttribute(dbobject, "action");
    DependencyApplication result = UNKNOWN_DIRECTION;

    assert(action, "Expected action attribute for dbobject node");
    if (streq(action->value, "diff") ||
	streq(action->value, "build")) 
    {
	result = FORWARDS;
    }
    else if (streq(action->value, "drop") ||
	     streq(action->value, "diffprep")) {
	result = BACKWARDS;
    }
    else if (streq(action->value, "fallback") ||
	     streq(action->value, "endfallback")) {
	result = UNKNOWN_DIRECTION;
    }
    else {
	RAISE(GENERAL_ERROR, newstr("Unknown action: %s", action->value));
    }
    objectFree((Object *) action, TRUE);
    return result;
}

static boolean
contextApplies(
    DependencyApplication build_direction,
    DependencyApplication context_direction)
{
    if (build_direction == FORWARDS) {
	if ((context_direction == FORWARDS) ||
	    (context_direction == BOTH_DIRECTIONS)) 
	{
	    return TRUE;
	}
    }
    else if (build_direction == BACKWARDS) {
	if ((context_direction == BACKWARDS) ||
	    (context_direction == BOTH_DIRECTIONS)) 
	{
	    return TRUE;
	}
    }
    return FALSE;
}

static Cons *
getContexts(xmlNode *node)
{
    xmlNode *context_node;
    Context *context;
    Cons *result = NULL;
    DependencyApplication build_direction;
    DependencyApplication context_direction;

    if (node) {
	build_direction = buildDirection(node);
	context_node = node;
	while (context_node = findContextNode(context_node)) {
	    context = contextNew(nodeAttribute(context_node, "type"),
				 nodeAttribute(context_node, "value"),
				 nodeAttribute(context_node, "default"));
	    assert(context->context_type, "Missing type attribute for context");

	    context_direction = dependencyApplicationForString(
		nodeAttribute(context_node, "applies"));
	    if (contextApplies(build_direction, context_direction)) {
		result = consNew((Object *) context, (Object *) result);
	    }
	    else {
		objectFree((Object *) context, TRUE);
	    }
	}
    }
    return result;
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
addContext(Vector *vec, Context *context)
{
    Node *context_node;

    /* Do not add the context if it is the default. */
    if (!streq(context->context_type->value, context->dflt->value)) {
	context_node = newContextNode(context->context_type, context->value);
	vectorPush(vec, (Object *) context_node);
    }
}

static boolean
contextMatch(Object *ctx1, Object *ctx2)
{
    return streq(((Context *) ctx1)->context_type->value, 
		 ((Context *) ctx1)->context_type->value);
}

static Cons *
getContextNavigation(xmlNode *from, xmlNode *target)
{
    Cons *from_contexts;
    Cons *target_contexts;
    Context *target_context;
    Context *from_context;
    Vector *departures = vectorNew(10);
    Vector *arrivals = vectorNew(10);
    Cons *result = consNew((Object *) departures, (Object *) arrivals);

    /* Contexts are lists of the form: (type value default) */
    from_contexts = getContexts(from);
    target_contexts = getContexts(target);
    while (target_contexts) {
	target_context = (Context *) consPop(&target_contexts);
	from_context = (Context *) listExtract(&from_contexts, 
					       (Object *) target_context,
					       &contextMatch);

	if (from_context) {
	    if (!streq(target_context->value->value, 
		       from_context->value->value))
	    {
		/* We have the same context type for both nodes but with
		 * different values, so we depart the old context and
		 * arrive at the new one.  */
		addContext(departures, from_context);
		addContext(arrivals, target_context);
	    }
	}
	else {
	    /* This is a new context. */
	    addContext(arrivals, target_context);
	}
	objectFree((Object *) from_context, TRUE);
	objectFree((Object *) target_context, TRUE);
    }

    /* Close the final contexts.  */
    while (from_contexts) {
	from_context = (Context *) consPop(&from_contexts);
	addContext(departures, from_context);
	objectFree((Object *) from_context, TRUE);
    }

    return result;
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
    String *action = nodeAttribute(nav_to->node, "action");

    if (streq(action->value, "build")) {
	/* We do not arrive at a node we are building. */
	this_node = nav_to->parent;
	
    }
    else {
	this_node = nav_to;
    }

    while (this_node && !nodesMatch(this_node, ancestor)) {
	/* Add an arrival to this into our arrivals vector. */
	new = xmlCopyNode(this_node->node, 2);
	nav_node = nodeNew(new);
	vectorInsert(arrivals, (Object *) nav_node, 0);

	this_node = this_node->parent;
    }
    objectFree((Object *) action, TRUE);
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

static Node*
commonAncestor(Node *nav_from, Node *nav_to)
{
    Node *result = NULL;
    Vector *from_stack;
    Vector *to_stack;
    Node *ancestor;
    Node *this;

    if (nav_from && nav_to) {
	from_stack = ancestryStack(nav_from);
	to_stack = ancestryStack(nav_to);
	
	while (ancestor = (Node *) vectorPop(from_stack)) {
	    this = (Node *) vectorPop(to_stack);
	    if (nodesMatch(ancestor, this)) {
		result = ancestor;
	    }
	    else {
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
nodeIsDisabled(xmlNode *node)
{
    String *disabled = nodeAttribute(node, "disabled");
    boolean result = FALSE;

    if (disabled) {
	result = streq(disabled->value, "yes");
	objectFree((Object *) disabled, TRUE);
    }

    return result;
}

/* Find a dbobject node by doing a depth first traversal of nodes. */
static xmlNode *
findDbobject(xmlNode *node)
{
    xmlNode *this = getNextNode(node);
    xmlNode *kid;

    while (this) {
	if (streq((char *) this->name, "dbobject")) {
	    return this;
	}
	if (kid = findDbobject(this->children)) {
	    return kid;
	}
	this = getNextNode(this->next);
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
	
	if (!nodeIsDisabled(this->node)) {
	    /* If this node is disabled, there is no work to be done for
	     * it, so no point in adding any navigation. */
	    node_from = node_to;
	    node_to = this;

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

