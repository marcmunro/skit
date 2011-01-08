/**
 * @file   diff.c
 * \code
 *     Copyright (c) 2009, 2010 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Read and process xmlfiles
 *
 */

#include <stdio.h>
#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"

static void
readDocs(Document **p_doc1, Document **p_doc2)
{
    addDeps();     /* Ensure next doc on stack has deps */
    *p_doc1 = docStackPop();
    addDeps();     /* Ensure next doc on stack has deps */
    *p_doc2 = docStackPop();
}

static boolean
isDepNode(xmlNode *node)
{
    return streq("dependencies", (char *) node->name);
}

static boolean
isContextNode(xmlNode *node)
{
    return streq("context", (char *) node->name);
}

static void
readDbobjectRule(xmlNode *node, Object *rules)
{
    String *type = nodeAttribute(node, "type");
    String *key = nodeAttribute(node, "key");
    Node *rulenode = nodeNew(node);
    Cons *rule = consNew((Object *) key, (Object *) rulenode);
    /* For now, the rule info is simply a cons cell containing the match
     *  attribute and the node.  This will be expanded as the need
     *  becomes clear.
     */
    hashAdd((Hash *) rules, (Object *) type, (Object *) rule);
}

typedef void (NodeOp)(xmlNode *node, Object *obj);

static xmlNode *
getElement(xmlNode *node) 
{
    while (node && (node->type != XML_ELEMENT_NODE)) {
	node = node->next;
    }
    return node;
}

static void
eachDbobject(xmlNode *node, Object *obj, NodeOp fn)
{
    xmlNode *this = getElement(node);

    while (this) {
	if (streq("dbobject", (char *) this->name)) {
	    fn(this, obj);
	}
	if (this->children) {
	    eachDbobject(this->children, obj, fn);
	}
	this = getElement(this->next);
    }
}

static Hash *
loadDiffRules(String *diffrules)
{
    Document *rulesdoc = NULL;
    Hash *rules = hashNew(TRUE);
    xmlNode *root;
    String *rulesdoc_key = stringNew("RULESDOC");
    rulesdoc = findDoc(diffrules);
    root = xmlDocGetRootElement(rulesdoc->doc);
    /* The rules document may not be freed until we have finished using
     * all of the rules that it describes.  This is most easily achieved
     * by placing the document into the hash that contains the rules.
     * This means the document will be freed at the same time as the
     * rules hash. */

    eachDbobject(root, (Object *) rules, readDbobjectRule);
    hashAdd(rules, (Object *) rulesdoc_key, (Object *) rulesdoc);
    return rules;
}

/* NOTE: Consumes type */
static Cons *
addNodeForLevel(xmlNode *dbobject, Cons *alist, String *type, String *key)
{
    Hash *hash = (Hash *) alistGet(alist, (Object *) type);
    Cons *alist_entry;
    Node *existing_entry;
    Node *node = nodeNew(dbobject);
    char *errmsg;
    char *str1;
    char *str2;

    if (!hash) {
	hash = hashNew(TRUE);
	alist_entry = consNew((Object *) type, (Object *) hash);
	alist = consNew((Object *) alist_entry, (Object *) alist);
    }
    else {
	objectFree((Object *) type, TRUE);
    }
    existing_entry = (Node *) hashAdd(hash, (Object *) key, (Object *) node);
    if (existing_entry) {
	str1 = nodestr(existing_entry->node);
	str2 = nodestr(node->node);
	errmsg = newstr("Unexpected collision of node keys: %s with %s",
			str1, str2);
	skfree(str1);
	skfree(str2);
	objectFree((Object *) existing_entry, TRUE);
	RAISE(XML_PROCESSING_ERROR, errmsg);
    }
    return alist;
}

static xmlNode *
getDbobject(xmlNode *here)
{
    xmlNode *result = here;
    while (result) {
	if (streq("dbobject", (char *) result->name)) {
	    return result;
	}
	result = result->next;
    }
    return result;
}

static Cons *
allDbobjects(xmlNode *node, Hash *rules)
{
    Cons *alist = NULL;
    String *type;
    char *keyattr;
    String *key;
    Cons *rule;
    xmlNode *this = node;

    while (this = getDbobject(this)) {
	type = nodeAttribute(this, "type");
	if (rule = (Cons *) hashGet(rules, (Object *) type)) {
	    keyattr = ((String *) rule->car)->value;
	}
	else {
	    keyattr = "fqn";
	}
	key = nodeAttribute(this, keyattr);
	alist = addNodeForLevel(this, alist, type, key);
	this = getElement(this->next);
    }
    return alist;
}


static xmlNode *
getMatch(xmlNode *node, Cons *alist, Hash *rules)
{
    String *type = nodeAttribute(node, "type");
    Cons *rule = (Cons *) hashGet(rules, (Object *) type);
    String *key = NULL;
    String *key_attr;
    Hash *candidates;
    Node *match = NULL;
    xmlNode *result = NULL;
    BEGIN {
	if (rule) {
	    key_attr = (String *) rule->car;
	    key = nodeAttribute(node, key_attr->value);
	}
	else {
	    /* No rule, so we match by fqn */
	    key = nodeAttribute(node, "fqn");	    
	}

	candidates = (Hash *) alistGet(alist, (Object *) type);
	if (match = (Node *) hashDel(candidates, (Object *) key)) {
	    result = match->node;
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) type, TRUE);
	objectFree((Object *) key, TRUE);
	objectFree((Object *) match, TRUE);
    }
    END;

    return result;
}

#define addSibling(first, prev, this)			\
    if (this) {						\
        if (first) {					\
	    prev->next = this;				\
	}						\
	else {						\
	    first = this;				\
	}						\
	prev = this;					\
    }

static void
setDiff(xmlNode *node, DiffType difftype)
{
    xmlChar *type;
    switch (difftype) {
    case IS_NEW: type = "New"; break;
    case IS_GONE: type = "Gone"; break;
    case IS_DIFF: type = "Diff"; break;
    case IS_SAME: type = "None"; break;
    case IS_UNKNOWN: type = "UNKNOWN"; break;
    case HAS_DIFFKIDS: type = "DifKids"; break;
    }

    (void) xmlNewProp(node, (const xmlChar *) "diff", type);
}

/* Copy an object's dependency records */
static xmlNode *
copyDeps(xmlNode *node)
{
    xmlNode *this = getElement(node->children);
    xmlNode *result = NULL;
    
    while (this && !isDepNode(this)) {
	this = getElement(this->next);
    }
    if (this) {
	result = xmlCopyNode(this, 1);
    }
    return result;
}

/* Copy an object's context record */
static xmlNode *
copyContext(xmlNode *node)
{
    xmlNode *this = getElement(node->children);
    xmlNode *result = NULL;
    
    while (this && !streq("context", (char *) this->name)) {
	this = getElement(this->next);
    }
    if (this) {
	result = xmlCopyNode(this, 1);
    }
    return result;
}

/* Create a dependency element for a dependency that exists in the old
 * version of an object but not the new. */
static xmlNode *
makeOldDep(char *type, String *qn)
{
    xmlNode *dep = xmlNewNode(NULL, BAD_CAST "dependency");

    (void) xmlNewProp(dep, (const xmlChar *) type, (xmlChar *) qn->value);
    (void) xmlNewProp(dep, (const xmlChar *) "old", (xmlChar *) "yes");

    return dep;
}

/* Identify any deps in node1, that are not in node2, and return them as
 * a list of siblings, marked with an "old" attribute. */
static xmlNode *
getOldDeps(xmlNode *node1, xmlNode *node2)
{
    Hash *fqns = hashNew(TRUE);
    Hash *pqns = hashNew(TRUE);
    String *qn = NULL;
    xmlNode *result = NULL;
    xmlNode *prev;
    xmlNode *new;
    xmlNode *this = NULL;
    Symbol *t = symbolGet("t");
    Object *found;

    BEGIN {
	/* Build hashes for fqn and pqn */
	this = getElement(node2->children);
	while (this && !isDepNode(this)) {
	    this = getElement(this->next);
	}
	if (this) {
	    this = getElement(this->children);
	    while (this) {
		if (qn = nodeAttribute(this, "fqn")) {
		    hashAdd(fqns, (Object *) qn, (Object *) t);
		}
		else if (qn = nodeAttribute(this, "pqn")) {
		    hashAdd(pqns, (Object *) qn, (Object *) t);
		}
		this = this->next;
	    }
	}
	/* Check for items from node1, that are not present in the
	 * hashes */
	this = getElement(node1->children);
	while (this && !isDepNode(this)) {
	    this = getElement(this->next);
	}
	if (this) {
	    this = getElement(this->children);
	    while (this) {
		if (qn = nodeAttribute(this, "fqn")) {
		    if (!(found = hashGet(fqns, (Object *) qn))) {
			new = makeOldDep("fqn", qn);
		    }
		}		
		else if (qn = nodeAttribute(this, "pqn")) {
		    if (!(found = hashGet(pqns, (Object *) qn))) {
			new = makeOldDep("pqn", qn);
		    }
		}
		if (!found) {
		    addSibling(result, prev, new);
		}
		objectFree((Object *) qn, TRUE);
		this = this->next;
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) fqns, TRUE);
	objectFree((Object *) pqns, TRUE);
    }
    END;
    return result;
}

/* Return the child of a dbobject that contains the actual content. */
static xmlNode *
skipToContents(xmlNode *node)
{
    xmlNode *this = getElement(node->children);
    while (this && (isDepNode(this) || isContextNode(this))) { 
	this = this->next;
    }
    return this;
}

/* Return a diff node describing any difference between the attributes
 * described by rule, or NULL, if there are no differences. */
static xmlNode *
check_attribute(xmlNode *content1, xmlNode *content2, xmlNode *rule)
{
    xmlNode *diff = NULL;
    String *attr_name = nodeAttribute(rule, "name");
    String *attr1 = nodeAttribute(content1, attr_name->value);
    String *attr2 = nodeAttribute(content2, attr_name->value);
    String *old = NULL;
    String *new = NULL;
    xmlChar *status = NULL;
    String *fail = NULL;
    
    if (attr1) {
	if (attr2) {
	    if (!streq(attr1->value, attr2->value)) {
		status = "Diff";
		old = attr1;
		new = attr2;
		attr1 = attr2 = NULL;
	    }
	}
	else {
	    status = "Gone";
	    old = attr1;
	    attr1 = NULL;
	}
    }
    else {
	if (attr2) {
	    status = "New";
	    new = attr2;
	    attr2 = NULL;
	}
    }
    BEGIN {
	if (status) {
	    /* There is a difference.  Check if that must cause a failure */
	    if (fail = nodeAttribute(rule, "fail")) {
		String *tmp1 = nodeAttribute(content1, attr_name->value);
		String *tmp2 = nodeAttribute(content2, attr_name->value);
		String *msg = nodeAttribute(rule, "msg");
		char *formatted_msg = newstr(msg->value, 
					     tmp1? tmp1->value: "<null>",
					     tmp2? tmp2->value: "<null>");
		objectFree((Object *) tmp1, TRUE);
		objectFree((Object *) tmp2, TRUE);
		objectFree((Object *) msg, TRUE);

		RAISE(XML_PROCESSING_ERROR, formatted_msg);
	    }

	    diff = xmlNewNode(NULL, BAD_CAST "attribute");
	    (void) xmlNewProp(diff, (const xmlChar *) "name", 
			      (xmlChar *) attr_name->value);
	    (void) xmlNewProp(diff, (const xmlChar *) "status", status);
	    if (old) {
		(void) xmlNewProp(diff, (const xmlChar *) "old", 
				  (xmlChar *) old->value);
	    }
	    if (new) {
		(void) xmlNewProp(diff, (const xmlChar *) "new", 
				  (xmlChar *) new->value);
	    }
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) attr_name, TRUE);
	objectFree((Object *) old, TRUE);
	objectFree((Object *) new, TRUE);
	objectFree((Object *) attr1, TRUE);
	objectFree((Object *) attr2, TRUE);
	objectFree((Object *) fail, TRUE);
    }
    END;

    return diff;
}

static xmlNode *
check_element(xmlNode *content1, xmlNode *content2, xmlNode *rule)
{
/*  RAISE(NOT_IMPLEMENTED_ERROR, 
	  newstr("check_element is not yet implemented"));
*/  return NULL;
}



/* Check the differences between the content parts of 2 dbobject nodes,
 * returning a list of diffs if any exist. */
static xmlNode *
objectDiffs(xmlNode *content1, xmlNode *content2, 
	     Hash *rules, boolean *has_diffs)
{
    xmlNode *diffs = NULL;
    xmlNode *diff;
    xmlNode *prev = NULL;
    String *type = stringNew(content1->name);
    Cons *rule_entry = (Cons *) hashGet(rules, (Object *) type);
    xmlNode *rule;
    xmlNode *this = NULL;

    BEGIN {
	if (rule_entry) {
	    rule = ((Node *) rule_entry->cdr)->node;
	    this = getElement(rule->children);
	}
	else {
	    *has_diffs = TRUE;  /* Overload the has_diffs param to 
				 * indicate that we don't know if
				 * there are diffs or not. */
	}
	while (this) {
	    if (streq(this->name, "attribute")) {
		diff = check_attribute(content1, content2, this);
	    }
	    else if (streq(this->name, "element")) {
		diff = check_element(content1, content2, this);
	    }
	    else {
		diff = NULL;
	    }

	    if (diff) {
		*has_diffs = TRUE;
		if (prev) {
		    prev->next = diff;
		}
		else {
		    diffs = xmlNewNode(NULL, BAD_CAST "diffs");
		    diffs->children = diff;
		}
		prev = diff;
	    }
	    this = getElement(this->next);
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) type, TRUE);
    }
    END;
    return diffs;
}

static void
diffdebug()
{
    fprintf(stderr, "DEBUG\n");
}

static xmlNode *processDiffs(xmlNode *node1,  xmlNode *node2, 
			     Hash *rules, boolean *diffs);

/* Copy the contents of a dbobject, and then recurse into other
 * objects. */
static xmlNode *
copyAndRecurse(xmlNode *node1, xmlNode *node2, 
	       Hash *rules, boolean *has_diffs)
{
    xmlNode *from1 = getElement(node1);
    xmlNode *from2 = getElement(node2);
    xmlNode *copy;
    xmlNode *new;
    xmlNode *prev = NULL;
    xmlNode *diffs;
    *has_diffs = FALSE;

    if (from2) {
	copy = xmlCopyNode(from2, 2);
	from2 = getElement(from2->children);
	while (from2 && !(streq("dbobject", (char *) from2->name))) {
	    new = xmlCopyNode(from2, 1);
	    addSibling(copy->children, prev, new);
	    from2 = getElement(from2->next);
	}
	if (from2) {
	    /* We must be at a dbobject.  This is where we need to
	     * recurse. */
	    if (from1) {
		from1 = getElement(from1->children);
	    }
	    diffs = processDiffs(from1, from2, rules, has_diffs);
	    addSibling(copy->children, prev, diffs);
	}
    }
    else {
	diffdebug();
	RAISE(NOT_IMPLEMENTED_ERROR, newstr("ARGGG"));
    }
    return copy;
}

/* Figure out the diffs for a single dbobject.  */
static xmlNode *
dbobjectDiff(xmlNode *node1, xmlNode *node2, 
	     Hash *rules, boolean *diffs)
{
    xmlNode *copy_from = node2? node2: node1;
    xmlNode *new_dbobj = xmlCopyNode(copy_from, 2);
    xmlNode *deps = copyDeps(copy_from);
    xmlNode *context = copyContext(copy_from);
    xmlNode *this;
    xmlNode *contents1 = NULL;
    xmlNode *contents2 = NULL;
    xmlNode *new_contents;
    xmlNode *old_deps;
    xmlNode *last = NULL;
    xmlNode *diffnodes = NULL;
    DiffType difftype = IS_SAME;
    boolean  has_diffs = FALSE;

    if (node1) {
	contents1 = skipToContents(node1);
	//dbgNode(contents1);
	if (node2) {
	    contents2 = skipToContents(node2);
	    //dbgNode(contents2);
	    if (old_deps = getOldDeps(node1, node2)) {
		if (!deps) {
		    deps = xmlNewNode(NULL, BAD_CAST "dependencies");
		}
		if (this = deps->children) {
		    while (this->next) {
			this = this->next;
		    }
		    this->next = old_deps;
		}
		else {
		    deps->children = old_deps;
		}
	    }
	    diffnodes = objectDiffs(contents1, contents2, rules, &has_diffs);
	    if (has_diffs) {
		if (diffnodes) {
		    difftype = IS_DIFF;
		}
		else {
		    difftype = IS_UNKNOWN;
		}
		*diffs = TRUE;
	    }
	}
	else {
	    difftype = IS_GONE;
	    *diffs = TRUE;
	}
    }
    else {
	contents2 = skipToContents(node2);
	difftype = IS_NEW;
	*diffs = TRUE;
    }

    /* Add any dependencies to our dbobject result */
    if (deps) {
	new_dbobj->children = deps;
	last = deps;
    }

    /* Add any context to our dbobject result */
    // TODO: Deal with change of context!!!!!!!!!!!
    if (context) {
	if (last) {
	    last->next = context;
	}
	else {
	    new_dbobj->children = context;
	}
	last = context;
    }

    /* Add any diffnodes to our dbobject result */
    if (diffnodes) {
	if (last) {
	    last->next = diffnodes;
	}
	else {
	    new_dbobj->children = diffnodes;
	}
	last = diffnodes;
    }
    //dbgNode(contents1);
    //dbgNode(contents2);
    /* Add the object contents, and its descendents to our dbobject result */
    if (new_contents = copyAndRecurse(contents1, contents2, 
				      rules, &has_diffs)) {
	if (last) {
	    last->next = new_contents;
	}
	else {
	    new_dbobj->children = new_contents;
	}
	if (has_diffs && (difftype == IS_SAME)) {
	    difftype = HAS_DIFFKIDS;
	}
    }

    setDiff(new_dbobj, difftype);
    return new_dbobj;
}


static Object *
recordNewObj(Object *obj, Object *param)
{
    Object *elem = ((Cons *) obj)->cdr;
    Hash *rules = (Hash *) ((Triple *) param)->obj1;
    Node *first = (Node *) ((Triple *) param)->obj2;
    Node *prev = (Node *) ((Triple *) param)->obj3;
    boolean diffs;
    xmlNode *new;
    new = dbobjectDiff(((Node *) elem)->node, NULL, rules, &diffs);
    addSibling(first->node, prev->node, new);
    return elem;
}


/* Process any unmatched objects for the current level from doc1.  These
 * are objects in doc1 for which there are no matches in doc2, ie dropped
 * objects. */ 
static xmlNode *
processRemaining(Cons *remaining, Hash *rules, boolean *diffs)
{
    Cons *next = remaining;
    Cons *entry;
    Hash *hash;
    Node first = {OBJ_XMLNODE, NULL};
    Node prev = {OBJ_XMLNODE, NULL};
    Triple params = {OBJ_TRIPLE, (Object *) rules, 
		     (Object *) &first, (Object *) &prev};

    while (next) {
	entry = (Cons *) next->car;
	hash = (Hash *) entry->cdr;
	next = (Cons *) next->cdr;
	hashEach(hash, recordNewObj, (Object *) &params);
    }
    if (first.node) {
	*diffs = TRUE;
    }
    return first.node;
}

static xmlNode *
processDiffs(
    xmlNode *node1, 
    xmlNode *node2, 
    Hash *rules,
    boolean *diffs)
{
    xmlNode *dbobj2 = getElement(node2);
    Cons *node1objects = NULL;
    xmlNode *match;
    xmlNode *prev;
    xmlNode *next = NULL;
    xmlNode *result = NULL;

    BEGIN {
	node1objects = allDbobjects(node1, rules);
	while (dbobj2 = getDbobject(dbobj2)) {
	    match = getMatch(dbobj2, node1objects, rules);
	    next = dbobjectDiff(match, dbobj2, rules, diffs);
	    addSibling(result, prev, next);
	    dbobj2 = dbobj2->next;
	}
	next = processRemaining(node1objects, rules, diffs);
	addSibling(result, prev, next);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) node1objects, TRUE);
    }
    END;

    return result;
}

/* This will handle the 2 root dump nodes and one of the params nodes.
 */
static xmlNode *
processDiffRoot(xmlNode *root1, xmlNode *root2, Hash *rules)
{
    xmlNode *dump1 = getElement(root1);
    xmlNode *dump2 = getElement(root2);
    xmlNode *result = xmlCopyNode(dump1, 2);
    String *dbname2 = nodeAttribute(dump2, "dbname");
    String *time2 = nodeAttribute(dump2, "time");
    xmlAttrPtr attr;
    xmlNode *diffs;
    boolean has_diffs;

    attr = xmlNewProp(result, "dbname2", dbname2->value);
    attr = xmlNewProp(result, "time2", time2->value);
    objectFree((Object *) dbname2, TRUE);
    objectFree((Object *) time2, TRUE);

    diffs = processDiffs(dump1->children, dump2->children, rules, 
			 &has_diffs);
    xmlAddChild(result, diffs);
    return result;
}


xmlNode *
doDiff(String *diffrules, boolean swap)
{
    Document *doc1 = NULL;
    Document *doc2 = NULL;
    xmlNode *result = NULL;
    Hash *rules = NULL;
    BEGIN {
	if (swap) {
	    readDocs(&doc2, &doc1);
	}
	else {
	    readDocs(&doc1, &doc2);
	}
	rules = loadDiffRules(diffrules);
	result = processDiffRoot(xmlDocGetRootElement(doc1->doc), 
				 xmlDocGetRootElement(doc2->doc), rules);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) rules, TRUE);
	objectFree((Object *) doc1, TRUE);
	objectFree((Object *) doc2, TRUE);
    }
    END;

    return result;
}
