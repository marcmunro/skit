/**
 * @file   diff.c
 * \code
 *     Copyright (c) 2009, 2010, 2011 Marc Munro
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
    Cons *rule;
    char *errmsg;
    /* For now, the rule info is simply a cons cell containing the match
     *  attribute and the node.  This will be expanded as the need
     *  becomes clear.
     */
    if (!key) {
	errmsg = newstr("diff rule for %s has no key", type->value);
	objectFree((Object *) type, TRUE);
	RAISE(XML_PROCESSING_ERROR, errmsg);
    }
    rule = consNew((Object *) key, (Object *) rulenode);
    hashAdd((Hash *) rules, (Object *) type, (Object *) rule);
}

typedef void (NodeOp)(xmlNode *node, Object *obj);

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

static String *
dbobjectType(xmlNode *dbobject)
{
    String *type;

    assert(dbobject, "dbobjectType: No dbobject provided");
    assert(streq("dbobject", (char *) dbobject->name),
	   "dbobjectType: param is not a dboject");
    type = nodeAttribute(dbobject, "type");
    if (!type) {
	RAISE(XML_PROCESSING_ERROR, newstr("dbject has no type attribute"));
    }
    
    return type;
}

static String *
keyattrForType(String *type, Hash *rules)
{
    Cons *rule;
    if (rule = (Cons *) hashGet(rules, (Object *) type)) {
	return stringDup((String *) rule->car);
    }
    else {
	return stringNew("fqn");
    }
}

static char *
keyattrForType2(String *type, Hash *rules)
{
    Cons *rule;
    if (rule = (Cons *) hashGet(rules, (Object *) type)) {
	return ((String *) rule->car)->value;
    }
    else {
	return "fqn";
    }
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
	type = dbobjectType(this);
	if (rule = (Cons *) hashGet(rules, (Object *) type)) {
	    keyattr = ((String *) rule->car)->value;
	}
	else {
	    keyattr = "fqn";
	}
	key = nodeAttribute(this, keyattr);
	alist = addNodeForLevel(this, alist, type, key);
	this = this->next;
    }
    return alist;
}

static void
addNodeToHash(Hash *hash, String *type, String *key, xmlNode *dbobject)
{
    Hash *subhash = (Hash *) hashGet(hash, (Object *) type);
    Node *node = nodeNew(dbobject);
    Object *prev;

    if (subhash) {
	objectFree((Object*) type, TRUE);
    }
    else {
	subhash = hashNew(TRUE);
	hashAdd(hash, (Object *) type, (Object *) subhash);
    }

    if (prev = hashAdd(subhash, (Object *) key, (Object *) node)) {
	RAISE(XML_PROCESSING_ERROR, 
	      newstr("Duplicate dbobject node type = %s, key = %s", 
		     type->value, key->value));
    }
}

static xmlNode *
extractMatch(Hash *hash, xmlNode *dbobject, Hash *rules)
{
    String *type;
    String *keyattr;
    Node *node;
    Hash *subhash;
    xmlNode *match;

    type = dbobjectType(dbobject);
    if (subhash = (Hash *) hashGet(hash, (Object *) type)) {
	objectFree((Object *) type, TRUE);
	keyattr = keyattrForType(type, rules);
	if (node = (Node *) hashDel(subhash, (Object *) keyattr)) {
	    match = node->node;
	    objectFree((Object *) node, FALSE);
	    return match;
	}
	/* No node found */
	objectFree((Object *) keyattr, TRUE);
	return NULL;
    }
    /* No subhash found. */
    objectFree((Object *) type, TRUE);
    return NULL;
}

static Hash *
allDbobjects2(xmlNode *node, Hash *rules)
{
    Hash *hash = hashNew(TRUE);
    String *type;
    char *keyattr;
    String *key;
    xmlNode *this = node;

    while (this = getDbobject(this)) {
	type = dbobjectType(this);
	keyattr = keyattrForType2(type, rules);
	key = nodeAttribute(this, keyattr);
	addNodeToHash(hash, type, key, this);
	this = this->next;
    }

    return hash;
}

static xmlNode *
getMatch2(xmlNode *node, Hash *objects, Hash *rules)
{
    String *type;
    char *keyattr;
    String *key;
    Hash *subhash;
    Node *match;
    xmlNode *result;

    type = dbobjectType(node);

    if (subhash = (Hash *) hashGet(objects, (Object *) type)) {
	keyattr = keyattrForType2(type, rules);
	objectFree((Object *) type, TRUE);
	key = nodeAttribute(node, keyattr);
	if (match = (Node *) hashDel(subhash, (Object *) key)) {
	    objectFree((Object *) key, TRUE);
	    result = match->node;
	    objectFree((Object *) match, FALSE);
	    return result;
	}
	objectFree((Object *) key, TRUE);
	return NULL;
    }
    objectFree((Object *) type, TRUE);
    return NULL;
}


static xmlNode *
getMatch(xmlNode *node, Cons *alist, Hash *rules)
{
    String *volatile key = NULL;
    Node *volatile match = NULL;
    String *volatile type = nodeAttribute(node, "type");
    Cons *rule = (Cons *) hashGet(rules, (Object *) type);
    String *key_attr;
    Hash *candidates;
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

	if (candidates = (Hash *) alistGet(alist, (Object *) type)) {
	    if (match = (Node *) hashDel(candidates, (Object *) key)) {
		result = match->node;
	    }
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
    case IS_NEW: type = DIFFNEW; break;
    case IS_GONE: type = DIFFGONE; break;
    case IS_DIFF: type = DIFFDIFF; break;
    case IS_REBUILD: type = ACTIONREBUILD; break;
    case IS_SAME: type = DIFFSAME; break;
    case IS_UNKNOWN: type = DIFFUNKNOWN; break;
    case HAS_DIFFKIDS: type = DIFFKIDS; break;
    }

    (void) xmlNewProp(node, (const xmlChar *) "diff", type);
}

static void
addNodeDeps(xmlNode *result_parent, xmlNode *node, char *rstrict)
{
    xmlNode *this = getElement(node->children);
    xmlNode *dep = NULL;

    while (this) {
	if (isDepNode(this)) {
	    dep = xmlCopyNode(this, 1);
	    xmlAddChild(result_parent, dep);
	    if (rstrict) {
		(void) xmlNewProp(dep, "condition", (xmlChar *) rstrict);
	    }
	}
	this = getElement(this->next);
    }
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

/* Return the child of a dbobject that contains the actual content. */
static xmlNode *
skipToContents(xmlNode *node)
{
    xmlNode *this = NULL;
    if (node) {
	this = getElement(node->children);
	while (this && (isDepNode(this) || isContextNode(this))) { 
	    this = this->next;
	}
    }
    return this;
}

/* Return a diff node describing any difference between the attributes
 * described by rule, or NULL, if there are no differences. */
static xmlNode *
check_attribute(xmlNode *content1, xmlNode *content2, xmlNode *rule)
{
    String *volatile attr_name = nodeAttribute(rule, "name");
    String *volatile attr1 = nodeAttribute(content1, attr_name->value);
    String *volatile attr2 = nodeAttribute(content2, attr_name->value);
    String *volatile old = NULL;
    String *volatile new = NULL;
    String *volatile fail = NULL;
    xmlChar *status = NULL;
    xmlNode *diff = NULL;
    
    if (attr1) {
	if (attr2) {
	    if (!streq(attr1->value, attr2->value)) {
		status = DIFFDIFF;
		old = attr1;
		new = attr2;
		attr1 = attr2 = NULL;
	    }
	}
	else {
	    status = DIFFGONE;
	    old = attr1;
	    attr1 = NULL;
	}
    }
    else {
	if (attr2) {
	    status = DIFFNEW;
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
text_diff(xmlChar *str1, xmlChar *str2)
{
    xmlNode *old;
    xmlNode *new;
    xmlNode *diffs;
    xmlNode *textnode;
    xmlNodePtr text;

    if (streq(str1, str2)) {
	return NULL;
    }

    old = xmlNewNode(NULL, BAD_CAST "old");
    text =  xmlNewText(str1);
    xmlAddChild(old, text);

    new = xmlNewNode(NULL, BAD_CAST "new");
    text =  xmlNewText(str2);
    xmlAddChild(new, text);

    textnode = xmlNewNode(NULL, BAD_CAST "text");
    xmlAddChild(textnode, old);
    xmlAddChild(textnode, new);

    diffs = xmlNewNode(NULL, BAD_CAST "diffs");
    xmlAddChild(diffs, textnode);
    return diffs;
}

static xmlNode *
check_text(xmlNode *content1, xmlNode *content2, xmlNode *rule)
{
    xmlNode *text1 = getText(content1->children);
    xmlNode *text2 = getText(content2->children);
    xmlChar *str1  = text1? xmlNodeGetContent(text1): NULL;
    xmlChar *str2  = text2? xmlNodeGetContent(text2): NULL;
    xmlNode *diff = NULL;
    if (str1) {
	if (str2)
	{
	    diff = text_diff(str1, str2);
	    xmlFree(str2);
	}
	else {
	    diff = text_diff(str1, "");
	}
	xmlFree(str1);
    }
    else if (str2) {
	    diff = text_diff("", str2);
	    xmlFree(str2);
    }
    return diff;
}

static xmlNode *
next_elem_of_type(xmlNode *node, String *elem_type)
{
    while (node = getElement(node)) {
	if (streq(node->name, elem_type->value)) {
	    return node;
	}
	node = node->next;
    }
    return NULL;
}

static xmlNode *
diffElement(xmlNode *source, char *type, char *status, char *key)
{
    xmlNode *diff = xmlNewNode(NULL, BAD_CAST "element");
    (void) xmlNewProp(diff, (const xmlChar *) "status", status);
    (void) xmlNewProp(diff, (const xmlChar *) "type", type);
    if (key) {
	(void) xmlNewProp(diff, (const xmlChar *) "key", key);
    }
    xmlAddChild(diff, xmlCopyNode(source, 1));
    return diff;
}

static Object *
handleNewElem(Cons *entry, Object *param)
{
    Node *source = (Node *) entry->cdr;
    Node *rule = (Node *) ((Cons *) param)->car;
    Node *results = (Node *) ((Cons *) param)->cdr;
    String *type = nodeAttribute(rule->node, "type");
    String *key = nodeAttribute(rule->node, "key");
    xmlNode *diff = diffElement(source->node, type->value, 
				DIFFNEW, key? key->value: NULL);
    if (diff) {
	if (results) {
	    diff->next = results->node;
	    results->node = diff;
 	}
	else {
	    results = nodeNew(diff);
	    ((Cons *) param)->cdr = (Object *) results;
	}
    }

    objectFree((Object *) type, TRUE);
    objectFree((Object *) key, TRUE);
    return (Object *) source;
}

static xmlNode *
elementDiffs(xmlNode *content1, xmlNode *content2, 
	     xmlNode *ruleset);

/* Return a diff node describing any difference between the elements
 * described by rule, or NULL, if there are no differences. */
static xmlNode *
check_element(xmlNode *content1, xmlNode *content2, xmlNode *rule)
{
    String *volatile elem_type = nodeAttribute(rule, "type");
    String *volatile key_type = nodeAttribute(rule, "key");
    Node *volatile rulenode = nodeNew(rule);
    Hash *volatile elems2 = hashNew(TRUE);
    xmlNode *diff = NULL;
    xmlNode *prev = NULL;
    xmlNode *kid;
    String *key;
    xmlNode *elem1 = content1->children;
    xmlNode *elem2 = content2->children;
    xmlNode *elem_diffs;
    Node *node;
    Object *obj;
    Cons results = {OBJ_CONS, (Object *) rulenode, NULL};

    BEGIN {
	/* Build a hash of file2 elements, that we can match against
	 * file1 elements. */
	while (elem2 = next_elem_of_type(elem2, elem_type)) {
	    if (key_type) {
		key = nodeAttribute(elem2, key_type->value);
	    }
	    else {
		key = stringNew(elem2->name);
	    }
	    node = nodeNew(elem2);
	    if (obj = hashAdd(elems2, (Object *) key, (Object *) node)) {
		/* There was already a matching element */
		objectFree(obj, TRUE);
		RAISE(XML_PROCESSING_ERROR, 
		      newstr("diff rule for element %s in <%s/> "
			     "has multiple matches for \"%s\"", 
			     elem_type->value, content2->name, key->value));
	    }
	    elem2 = elem2->next;
	}

	/* Now handle each element of the given type from file1 */
	while (elem1 = next_elem_of_type(elem1, elem_type)) {
	    if (key_type) {
		key = nodeAttribute(elem1, key_type->value);
	    }
	    else {
		key = stringNew(elem1->name);
	    }

	    node = (Node *) hashDel(elems2, (Object *) key);
	    objectFree((Object *) key, TRUE);

	    if (node) {
		elem2 = node->node;
		//dbgNode(elem1);
		//dbgNode(elem2);
		objectFree((Object *) node, TRUE);
		elem_diffs = elementDiffs(elem1, elem2, rule);
		if (elem_diffs) {
		    //dbgNode(elem_diffs);
		    diff = diffElement(elem2, elem_type->value, DIFFDIFF, 
				       key_type? key_type->value: NULL);
		    xmlAddChild(diff, elem_diffs);
		}
		else {
		    diff = NULL;
		}
	    }
	    else { /* No match for elem1 in content2 */
		diff = diffElement(elem1, elem_type->value, 
				   DIFFGONE, key_type? key_type->value: NULL);
	    }
	    if (diff) {
		diff->next = prev;
		prev = diff;
	    }
	    elem1 = elem1->next;
	}

	if (prev) {
	    results.cdr = (Object *) nodeNew(prev);
	}
	/* Finally handle any unmatched elements from file2 */
	hashEach(elems2, handleNewElem, (Object *) &results);

    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) rulenode, TRUE);
	objectFree((Object *) elem_type, TRUE);
	objectFree((Object *) key_type, TRUE);
	objectFree((Object *) elems2, TRUE);
    }
    END;

    if (results.cdr) {
	node = (Node *) results.cdr;
	diff = node->node;
	objectFree((Object *) node, TRUE);
	
	return diff;
    }

    return NULL;
}


printList(char *name, xmlNode *node)
{
    char *tmp = newstr("%s elem: ", name);
    while (node) {
	printNode(stderr, tmp, node);
	node=node->next;
    }
    skfree(tmp);
}
/* Check the differences between two elements (these may be the
 * contents nodes of two dbobjects, or elements within such contents
 * nodes) returning a list of diffs if any exist. */
static xmlNode *
elementDiffs(xmlNode *content1, xmlNode *content2, 
	     xmlNode *ruleset)
{
    xmlNode *rule;
    xmlNode *result = NULL;
    xmlNode *diff = NULL;
    xmlNode *prev = NULL;

    if (ruleset) {
	rule = getElement(ruleset->children);

	while (rule) {
	    if (streq(rule->name, "attribute")) {
		diff = check_attribute(content1, content2, rule);
	    }
	    else if (streq(rule->name, "element")) {
		diff = check_element(content1, content2, rule);
	    }
	    else if (streq(rule->name, "text")) {
		diff = check_text(content1, content2, rule);
	    }
	    else {
		diff = NULL;
	    }
	    if (diff) {
		if (prev) {
		    xmlAddNextSibling(prev, diff);
		}
		else {
		    result = diff;
		}
		prev = diff;
	    }
	    rule = getElement(rule->next);
	}
    }
    return result;
}

static void
diffdebug()
{
    fprintf(stderr, "DEBUG\n");
}


static xmlNode *
copyContents(xmlNode *next)
{
    xmlNode *from = next;
    xmlNode *copy = NULL;
    xmlNode *new;
    
    if (from) {
	copy = xmlCopyNode(from, 2);
	from = getElement(from->children);
	while (from && !(streq("dbobject", (char *) from->name))) {
	    new = xmlCopyNode(from, 1);
	    xmlAddChild(copy, new);
	    from = getElement(from->next);
	}
    }
    return copy;
}

static xmlNode *
skipToDbobject(xmlNode *from)
{
    from = getElement(from->children);
    while (from && !(streq("dbobject", (char *) from->name))) {
	from = getElement(from->next);
    }
    return from;
}



static xmlNode *
dbobjectDiff(xmlNode *dbobject1, xmlNode *node2, 
	     Hash *rules, boolean *diffs);

static void processDiffs(xmlNode *node1,  xmlNode *node2, Hash *rules, 
			 xmlNode *result_parent, boolean *diffs);


static xmlNode *
rulesetForNode(xmlNode *node, Hash *rules)
{
    String *type = nodeAttribute(node, "type");
    Cons *rule_entry = (Cons *) hashGet(rules, (Object *) type);

    objectFree((Object *) type, TRUE);

    if (rule_entry) {
	return ((Node *) rule_entry->cdr)->node;
    }
    return NULL;
}

/* Figure out the diffs for a single dbobject.  */
static xmlNode *
dbobjectDiff(xmlNode *dbobject1, xmlNode *dbobject2, 
	     Hash *rules, boolean *diffs)
{
    xmlNode *contents1 = skipToContents(dbobject1);
    xmlNode *contents2 = skipToContents(dbobject2);
    xmlNode *ruleset;
    DiffType difftype;
    xmlNode *difflist = NULL;
    xmlNode *context;
    xmlNode *content;
    xmlNode *volatile result = NULL;
    boolean kids_differ = FALSE;
    String *volatile rebuild = NULL;

    assert((dbobject1 == NULL) || streq("dbobject", (char *) dbobject1->name),
	   "dbobjectDiff: dbobject1 must be a dbobject");
    assert((dbobject2 == NULL) || streq("dbobject", (char *) dbobject2->name),
	   "dbobjectDiff: dbobject2 must be a dbobject");

    BEGIN {
	if (dbobject2) {
	    result = xmlCopyNode(dbobject2, 2);
	    if (contents1) {
		if (ruleset = rulesetForNode(dbobject1, rules)) {
		    rebuild = nodeAttribute(ruleset, "rebuild");
		    if (difflist = elementDiffs(contents1, contents2, 
						ruleset)) 
		    {
			*diffs = TRUE;
			difftype = rebuild? IS_REBUILD: IS_DIFF;
		    }
		    else {
			difftype = IS_SAME;
		    }
		}
		else {
		    /* No rules found, so we don't know if there are diffs.
		     * Provide a cautious status for the diff. */
		    *diffs = TRUE;
		    difftype = IS_UNKNOWN;
		}
		addNodeDeps(result, dbobject1, "drop"); 
		addNodeDeps(result, dbobject2, "build"); 
		/* If there is an old context, we use that in preference
		 * to any new context.  */
		context = copyContext(dbobject1);
	    }
	    else {
		difftype = IS_NEW;
		*diffs = TRUE;
		addNodeDeps(result, dbobject2, NULL); 
		context = copyContext(dbobject2);
	    }
	    content = copyContents(contents2);
	}
	else {
	    difftype = IS_GONE;
	    *diffs = TRUE;
	    result = xmlCopyNode(dbobject1, 2);
	    addNodeDeps(result, dbobject1, NULL); 
	    context = copyContext(dbobject1);
	    content = copyContents(contents1);
	}
	(void) xmlAddChildList(result, context);
	(void) xmlAddChildList(result, difflist);
	(void) xmlAddChildList(result, content);

	processDiffs(contents1? contents1->children: NULL, 
		     contents2? contents2->children: NULL, 
		     rules, result, &kids_differ);

	if (kids_differ) {
	    *diffs = TRUE;
	    if (difftype == IS_SAME) {
		difftype = rebuild? IS_REBUILD: HAS_DIFFKIDS;
	    }
	}
	objectFree((Object *) rebuild, TRUE);
    }
    EXCEPTION(ex) {
	xmlFreeNode(result);
	objectFree((Object *) rebuild, TRUE);
    }
    END;
    setDiff(result, difftype);
    return result;
}


static Object *
recordDroppedObj(Cons *entry, Object *param)
{
    Object *elem = entry->cdr;
    Node *parent = (Node *) ((Cons *) param)->car;
    Hash *rules = (Hash *) ((Cons *) param)->cdr;
    boolean diffs;
    xmlNode *new;
    new = dbobjectDiff(((Node *) elem)->node, NULL, rules, &diffs);
    (void) xmlAddChild(parent->node, new);

    return elem;
}

static Object *
processSubHash(Cons *entry, Object *param)
{
    Object *elem = entry->cdr;
    hashEach((Hash *) elem, recordDroppedObj, param);
    return elem;
}


/* Process any unmatched objects for the current level from doc1.  These
 * are objects in doc1 for which there are no matches in doc2, ie dropped
 * objects. */ 
static void
processRemaining(Hash *remaining, Hash *rules, xmlNode *parent, boolean *diffs)
{
    Node parent_node = {OBJ_XMLNODE, parent};
    xmlNode *prev_lastkid = parent->last;
    Cons param = {OBJ_CONS, (Object *) &parent_node, (Object *) rules};

    hashEach(remaining, processSubHash, (Object *) &param);
    if (prev_lastkid != parent->last) {
	*diffs = TRUE;
    }
    objectFree((Object *) remaining, TRUE);
}

static void
processDiffs(
    xmlNode *node1, 
    xmlNode *node2, 
    Hash *rules,
    xmlNode *result_parent,
    boolean *has_diffs)
{
    Hash *volatile node1objects = NULL;
    xmlNode *dbobj2 = getElement(node2);
    xmlNode *match;
    xmlNode *difflist = NULL;
    boolean diffs;

    BEGIN {
	node1objects = allDbobjects2(node1, rules);
	while (dbobj2 = getDbobject(dbobj2)) {
	    diffs = FALSE;
	    match = getMatch2(dbobj2, node1objects, rules);
	    if (difflist = dbobjectDiff(match, dbobj2, rules, &diffs)) {
		xmlAddChildList(result_parent, difflist);
	    }
	    
	    if (diffs) {
		*has_diffs = TRUE;
	    }
	    dbobj2 = dbobj2->next;
	}
	processRemaining(node1objects, rules, result_parent, &diffs);

    }
    EXCEPTION(ex) {
	objectFree((Object *) node1objects, TRUE);
    }
    END;
}


/* This will handle the 2 root dump nodes and one of the params nodes.
 */
static xmlNode *
processDiffRoot(xmlNode *root1, xmlNode *root2, Hash *rules)
{
    xmlNode *dump1 = getElement(root1);
    xmlNode *dump2 = getElement(root2);
    xmlNode *volatile result = xmlCopyNode(dump1, 2);
    String *dbname2 = nodeAttribute(dump2, "dbname");
    String *time2 = nodeAttribute(dump2, "time");
    xmlAttrPtr attr;
    boolean has_diffs;

    attr = xmlNewProp(result, "dbname2", dbname2->value);
    attr = xmlNewProp(result, "time2", time2->value);
    objectFree((Object *) dbname2, TRUE);
    objectFree((Object *) time2, TRUE);
    BEGIN {
	has_diffs = FALSE;
	processDiffs(dump1->children, dump2->children, rules, 
		     result, &has_diffs);
    }
    EXCEPTION(ex) {
	xmlFreeNode(result);
    }
    END;
    return result;
}


xmlNode *
doDiff(String *diffrules, boolean swap)
{
    Document *volatile doc1 = NULL;
    Document *volatile doc2 = NULL;
    Hash *volatile rules = NULL;
    xmlNode *result = NULL;
    BEGIN {
	if (swap) {
	    readDocs((Document **) &doc1, (Document **) &doc2);
	}
	else {
	    readDocs((Document **) &doc2, (Document **) &doc1);
	}
	//dbgSexp(doc1);
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

