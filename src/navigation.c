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
    String *visit = nodeAttribute(node, "visit");
    if (visit) {
	objectFree((Object *) visit, TRUE);
	return TRUE;
    }
    return FALSE;
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
getContexts(DagNode *node)
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
arriveContextNode(String *name, String *value)
{
    xmlNode *dbobject = dbobjectNode(name->value, value->value);
    return dagNodeNew(dbobject, ARRIVE_NODE);
}

static DagNode *
departContextNode(String *name, String *value)
{
    xmlNode *dbobject = dbobjectNode(name->value, value->value);
    return dagNodeNew(dbobject, DEPART_NODE);
}

static void
addArriveContext(Vector *vec, Cons *context)
{
    String *name = (String *) context->car;
    Cons *cell2 = (Cons *) context->cdr;
    DagNode *context_node;

    /* Do not close the context, if it is the default. */
    if (objectCmp(cell2->car, cell2->cdr) != 0) {
	context_node = arriveContextNode(name, (String *) cell2->car);
	vectorPush(vec, (Object *) context_node);
    }
}

static void
addDepartContext(Vector *vec, Cons *context)
{
    String *name = (String *) context->car;
    Cons *cell2 = (Cons *) context->cdr;
    DagNode *context_node;

    /* Do not close the context, if it is the default. */
    if (objectCmp(cell2->car, cell2->cdr) != 0) {
	context_node = departContextNode(name, (String *) cell2->car);
	vectorPush(vec, (Object *) context_node);
    }
}

static Cons *
getContextNavigation(DagNode *from, DagNode *target)
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
		addDepartContext(departures, match);
		addArriveContext(arrivals, this);
	    }
	    objectFree((Object *) match, TRUE);
	}
	else {
	    /* This is a new context. */
	    addArriveContext(arrivals, this);
	}
	objectFree((Object *) this, TRUE);
    }
    while (from_contexts && (this = (Cons *) consPop(&from_contexts))) {
	/* Close the final contexts.  Unless we are in a default
	 * context. */ 
	addDepartContext(departures, this);
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
    DagNode *next = NULL;
    DagNode *common_root = NULL;
    DagNode *navigation = NULL;
    Symbol *ignore_contexts = symbolGet("ignore-contexts");
    boolean handling_context = (ignore_contexts == NULL);
    int i;
    BEGIN {
	if (handling_context) {
	    context_nav = getContextNavigation(start, target);

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


