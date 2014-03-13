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
    char *ruletext;
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
    rule = (Cons *) hashAdd((Hash *) rules, (Object *) type, (Object *) rule);
    if (rule) {
	ruletext = objectSexp((Object *) rule);
	errmsg = newstr("Duplicate diff rule found for \"%s\".  Rule = \"%s\"", 
			type->value, ruletext);
	skfree(ruletext);
	objectFree((Object *) rule, TRUE);
	RAISE(XML_PROCESSING_ERROR, errmsg);
    }
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
    Document *volatile rulesdoc = NULL;
    Hash *volatile rules = hashNew(TRUE);
    xmlNode *root;
    String *volatile rulesdoc_key = stringNew("RULESDOC");

    BEGIN {
	rulesdoc = findDoc(diffrules);
	root = xmlDocGetRootElement(rulesdoc->doc);

	/* The rules document may not be freed until we have finished
	 * using all of the rules that it describes.  This is most
	 * easily achieved by placing the document into the hash that
	 * contains the rules.  This means the document will be freed at
	 * the same time as the rules hash. */

	eachDbobject(root, (Object *) rules, readDbobjectRule);
	hashAdd(rules, (Object *) rulesdoc_key, (Object *) rulesdoc);
    }
    EXCEPTION(ex) {
	objectFree((Object *) rules, TRUE);
	objectFree((Object *) rulesdoc, TRUE);
	objectFree((Object *) rulesdoc_key, TRUE);
    }
    END;
    return rules;
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

static char *
keyattrForType(String *type, Hash *rules)
{
    Cons *rule;
    if (rule = (Cons *) hashGet(rules, (Object *) type)) {
	return ((String *) rule->car)->value;
    }
    else {
	return "fqn";
    }
}

/* Consumes key and type, generally as keys hash entries. */
static void
addNodeToHash(Hash *hash, String *type, String *key, xmlNode *dbobject)
{
    Hash *subhash = (Hash *) hashGet(hash, (Object *) type);
    Node *volatile node = nodeNew(dbobject);
    Object *prev;
    String *to_free = NULL;

    BEGIN {
	if (subhash) {
	    to_free = type;
	}
	else {
	    subhash = hashNew(TRUE);
	    prev = hashAdd(hash, (Object *) type, (Object *) subhash);
	    if (prev) {
		RAISE(XML_PROCESSING_ERROR, 
		      newstr("Dunno what's going on here\n")); 
	    }
	}

	if (prev = hashAdd(subhash, (Object *) key, (Object *) node)) {
	    node = NULL;  /* Do not free node in the exception handler. */
	    dbgSexp(prev);
	    dbgNode(dbobject);
	    RAISE(XML_PROCESSING_ERROR, 
		  newstr("Duplicate dbobject node type = %s, key = %s", 
			 type->value, key->value));
	}
	objectFree((Object*) to_free, TRUE);
    }
    EXCEPTION(ex) {
	objectFree((Object *) node, FALSE);
    }
    END;
}

static Hash *
allDbobjects(xmlNode *node, Hash *rules)
{
    Hash *volatile hash = hashNew(TRUE);
    String *type;
    char *keyattr;
    String *key;
    xmlNode *this = node;
    char *tmp;
    char *errmsg;
    BEGIN {
	while (this = getDbobject(this)) {
	    type = dbobjectType(this);
	    keyattr = keyattrForType(type, rules);
	    if (key = nodeAttribute(this, keyattr)) {
		addNodeToHash(hash, type, key, this);
		this = this->next;
	    }
	    else {
		objectFree((Object *) type, TRUE);
		tmp = nodestr(node);
		errmsg = newstr("Cannot find key field %s in node %s.", 
				keyattr, tmp);
		skfree(tmp);
		RAISE(XML_PROCESSING_ERROR, errmsg);
	    }
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) hash, TRUE);
    }
    END;

    return hash;
}

static xmlNode *
getMatch(xmlNode *node, Hash *objects, Hash *rules)
{
    String *type;
    char *keyattr;
    String *key;
    Hash *subhash;
    Node *match;
    xmlNode *result;

    type = dbobjectType(node);

    if (subhash = (Hash *) hashGet(objects, (Object *) type)) {
	keyattr = keyattrForType(type, rules);
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


static Hash *
getNodeDeps(xmlNode *node)
{
    Hash *deps = hashNew(TRUE);
    xmlNode *this = getElement(node->children);
    xmlNode *dep;
    // TODO: rewrite variable name, maybe simplify.
    String *condition;

    while (this) {
	if (isDependencies(this)) {
	    this = getElement(this->children);
	}
	else {
	    if (isDepNode(this)) {
		if (!(condition = directionForDep(this))) {
		    condition = stringNew("");
		}
		dep = xmlCopyNode(this, 1);
		(void) hashVectorAppend(deps, (Object *) condition, 
					(Object *) nodeNew(dep));
	    }
	    this = getElement(this->next);
	}
    }
    return deps;
}

static void
addNodeDeps(xmlNode *result_parent, xmlNode *node)
{
    xmlNode *this = getElement(node->children);
    xmlNode *dep = NULL;

    while (this) {
	if (isDepNode(this)) {
	    dep = xmlCopyNode(this, 1);
	    xmlAddChild(result_parent, dep);
	}
	this = getElement(this->next);
    }
}

/* Copy an object's context record */
static xmlNode *
copyContext(xmlNode *node, char *condition)
{
    // TODO: change parameter name to direction
    xmlNode *this = getElement(node->children);
    xmlNode *result = NULL;

    while (this && !streq("context", (char *) this->name)) {
	this = getElement(this->next);
    }
    if (this) {
	result = xmlCopyNode(this, 1);
	if (condition) {
	    (void) xmlNewProp(result, "direction", condition);
	}
        
    }
    return result;
}

/* Return the child of a dbobject that contains the actual content. */
static xmlNode *
skipToContents(xmlNode *node)
{
    xmlNode *this = NULL;
    String *type = nodeAttribute(node, "type");

    if (node) {
	this = getElement(node->children);
	while (this && !streq(this->name, type->value)) {
	    this = this->next;
	}
    }
    objectFree((Object *) type, TRUE);
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
	    if (nodeHasAttribute(rule, "fail")) {
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
check_text(xmlNode *content1, xmlNode *content2)
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
	    xmlAddSibling(results->node, diff);	    
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
	     xmlNode *ruleset, xmlNode **p_deps,
	     boolean *p_do_rebuild);

static String *
keyFromElement(xmlNode *elem, String *key_type)
{
    if (key_type) {
	return nodeAttribute(elem, key_type->value);
    }
    else {
	return stringNew(elem->name);
    }
}


static Hash *
elementHash(
    xmlNode *content,
    String *elem_type,
    String *key_type)
{
    Hash *volatile results = hashNew(TRUE);
    String *volatile key;
    Node *volatile node;
    Object *obj;
    xmlNode *elem = content->children;
    BEGIN {
	while (elem = next_elem_of_type(elem, elem_type)) {
	    key = keyFromElement(elem, key_type);
	    node = nodeNew(elem);
	    if (obj = hashAdd(results, (Object *) key, (Object *) node)) {
		/* There was already a matching element */
		objectFree(obj, TRUE);
		RAISE(XML_PROCESSING_ERROR, 
		      newstr("diff rule for element %s in <%s/> "
			     "has multiple matches for \"%s\"", 
			     elem_type->value, content->name, key->value));
	    }
	    elem = elem->next;
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) results, TRUE);
    }
    END;
    return results;
}

static xmlNode *
newDiffElement(
    xmlNode *element,
    xmlNode *prev_diffs,
    xmlNode *diff_details,
    String *type,
    char *status,
    String *key)
{
    xmlNode *new_diff = diffElement(element, type->value, status, 
				    key? key->value: NULL);
    xmlAddChild(new_diff, diff_details);
    if (prev_diffs) {
	xmlAddSibling(prev_diffs, new_diff);
	return prev_diffs;
    }
    return new_diff;
}

static xmlNode *
diffsForUnmatchedElements(Hash *elems, xmlNode *diffs, xmlNode *rule)
{
    Node *volatile rulenode = nodeNew(rule);
    Node *volatile diffnode = diffs? nodeNew(diffs): NULL;
    Cons rule_and_results = {OBJ_CONS, (Object *) rulenode, 
			     (Object *) diffnode};
    xmlNode *result = diffs;

    BEGIN {
	hashEach(elems, handleNewElem, (Object *) &rule_and_results);
	diffnode = (Node *) rule_and_results.cdr;
	if (diffnode) {
	    result = diffnode->node;
	}
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) rulenode, FALSE);
	objectFree((Object *) diffnode, FALSE);
    }
    END;
    return result;
}

/* Return a diff node describing any difference between the elements
 * described by rule, or NULL, if there are no differences. */
static xmlNode *
check_element(
    xmlNode *content1, 
    xmlNode *content2, 
    xmlNode *rule, 
    xmlNode **p_deps,
    boolean *p_do_rebuild)
{
    String *volatile elem_type = nodeAttribute(rule, "type");
    String *volatile key_type = nodeAttribute(rule, "key");
    Hash *volatile elems2 = NULL;
    xmlNode *elem1 = content1->children;
    xmlNode *elem2;
    xmlNode *element_diffs = NULL;
    xmlNode *diffs = NULL;
    String *key;
    Node *node;
    char *tmp;
    char *errmsg;

    BEGIN {
	if (!elem_type) {
	    tmp = nodestr(rule);
	    errmsg = newstr("rule node for element has no type attribute: %s",
			    tmp);
	    skfree(tmp);
	    RAISE(XML_PROCESSING_ERROR, errmsg);
	}
	elems2 = elementHash(content2, elem_type, key_type);
	/* Check each element of the given type from contents1 */
	while (elem1 = next_elem_of_type(elem1, elem_type)) {
	    key = keyFromElement(elem1, key_type);
	    node = (Node *) hashDel(elems2, (Object *) key);

	    if (node) {
		elem2 = node->node;
		objectFree((Object *) node, TRUE);
		if (element_diffs = elementDiffs(elem1, elem2, rule, 
						 p_deps, p_do_rebuild))
		{
		    diffs = newDiffElement(elem2, diffs, element_diffs,
					   elem_type, DIFFDIFF, key_type);
		    *p_do_rebuild |= nodeHasAttribute(rule, "rebuild");
		}
		
	    }
	    else {
		diffs = newDiffElement(elem1, diffs, NULL,
				       elem_type, DIFFGONE, key_type);
		*p_do_rebuild |= nodeHasAttribute(rule, "rebuild");
	    }
	    objectFree((Object *) key, TRUE);
	    elem1 = elem1->next;
	}

	diffs = diffsForUnmatchedElements(elems2, diffs, rule);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) elems2, TRUE);
	objectFree((Object *) elem_type, TRUE);
	objectFree((Object *) key_type, TRUE);
    }
    END;

    return diffs;
}

static char *
evalAttr(xmlChar *expr, xmlNode *content1, xmlNode *content2)
{
    char *result = newstr("");
    char *tmp;
    char *remaining = expr;
    char *brace;
    char *name;
    xmlChar *attr;
    boolean new;
    
    while (brace = strchr(remaining, '{')) {
	new = FALSE;
	*brace = '\0';
	tmp = result;
	result = newstr("%s%s", result, remaining);
	skfree(tmp);
	*brace = '{';
	
	remaining = strchr(brace, '}');
	if (!remaining) {
	    RAISE(XML_PROCESSING_ERROR,
		  newstr("Unbalanced braces in \"%s\"\n", expr));
	}
	*remaining = '\0';
	if (strncmp(brace + 1, "new.", 4) == 0) {
	    new = TRUE;
	}
	else if (strncmp(brace + 1, "old.", 4) != 0) {
	    *remaining = '}';
	    RAISE(XML_PROCESSING_ERROR,
		  newstr("Invalid braced expression: \"%s\"\n", expr));
	}
	name = brace + 5;
	if (new) {
	    attr = xmlGetProp(content2, name);
	}
	else {
	    attr = xmlGetProp(content1, name);
	}
	if (!attr) {
	    dbgNode(content1);
	    dbgNode(content2);
	    RAISE(XML_PROCESSING_ERROR,
		  newstr("Unable to find attribute %s (for %s)\n", name,
			 new? "new": "old"));
	}
	*remaining = '}';

	tmp = result;
	result = newstr("%s%s", result, attr);
	skfree(tmp);
	xmlFree(attr);
	remaining++;
    }

    tmp = result;
    result = newstr("%s%s", result, remaining);
    skfree(tmp);
    xmlFree(expr);
    return result;
}


static void
evalDiffDepProps(
    xmlNode *target,
    xmlNode *depnode, 
    xmlNode *content1, 
    xmlNode *content2)
{
    xmlAttr *attr;
    const xmlChar *name;
    xmlChar *expr;
    char *value;
    for (attr = depnode->properties; attr; attr = attr->next) {
	name = attr->name;
	expr = xmlGetProp(depnode, name);
	value = evalAttr(expr, content1, content2);
	(void) xmlNewProp(target, name, value);
	skfree(value);
    }
}


static xmlNode *
evalDiffDep(xmlNode *depnode, xmlNode *content1, xmlNode *content2)
{
    xmlNode *result = NULL;
    xmlNode *new;
    xmlNode *kids;
    xmlNode *cur = depnode;
    do {
	new = xmlCopyNode(depnode, 0);
	evalDiffDepProps(new, cur, content1, content2);

	if (cur->children) {
	    kids = evalDiffDep(getElement(cur->children), content1, content2);
	    xmlAddChildList(new, kids);
	}
	if (result) {
	    xmlAddSibling(result, new);
	}
	else {
	    result = new;
	}
    }
    while (cur = getElement(cur->next));
    return result;
}

/* Create dependencies based on the existence of a difference between
 * dbobjects.  The source deps may contain expressions of the form
 * {old.attr} or {new.attr}.  These will be replaced by the old or new
 * value of the appropriate attribute from the element containing a diff.
 */
static xmlNode *
diffDependency(
    xmlNode *rule, 
    xmlNode *content1, 
    xmlNode *content2)
{
    xmlNode *dep = getElement(rule->children);
    xmlNode *result = NULL;

    if (dep && isDepNode(dep)) {
	result = evalDiffDep(dep, content1, content2);
    }

    return result;
}

static void
addSiblingList(xmlNode *cur, xmlNode *list)
{
    xmlNode *prev = cur;
    xmlNode *this = list;
    xmlNode *next;
    while (this) {
	next = this->next;
	(void) xmlAddSibling(prev, this);
	prev = this;
	this = next;
    }
} 


/* Check the differences between two elements (these may be the
 * contents nodes of two dbobjects, or elements within such contents
 * nodes) returning a list of diffs if any exist. */
static xmlNode *
elementDiffs(xmlNode *content1, xmlNode *content2, 
	     xmlNode *ruleset, xmlNode **p_deps, 
	     boolean *p_do_rebuild)
{
    xmlNode *rule;
    xmlNode *result = NULL;
    xmlNode *diff = NULL;
    xmlNode *dep;

    if (ruleset) {
	rule = getElement(ruleset->children);

	while (rule) {
	    if (streq(rule->name, "attribute")) {
		diff = check_attribute(content1, content2, rule);
	    }
	    else if (streq(rule->name, "element")) {
		diff = check_element(content1, content2, rule, 
				     p_deps, p_do_rebuild);
	    }
	    else if (streq(rule->name, "text")) {
		diff = check_text(content1, content2);
	    }
	    else {
		diff = NULL;
	    }
	    if (diff) {
		if (nodeHasAttribute(rule, "rebuild")) {
		    *p_do_rebuild = TRUE;
		}

		if (dep = diffDependency(rule, content1, content2)) {
		    if (*p_deps) {
			addSiblingList(*p_deps, dep);
		    }
		    else {
			*p_deps = dep;
		    }
		}
		
		if (result) {
		    addSiblingList(result, diff);
		}
		else {
		    result = diff;
		}
	    }
	    rule = getElement(rule->next);
	}
    }
    return result;
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
	while (from) {
	    if (!(streq("dbobject", (char *) from->name))) {
		new = xmlCopyNode(from, 1);
		xmlAddChild(copy, new);
	    }
	    from = getElement(from->next);
	}
    }
    return copy;
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


static Object *
freeVectorDeps(Cons *entry, Object *param)
{
    Vector *vec = (Vector *) entry->cdr;
    int i;
    Node *node;
    UNUSED(param);

    EACH(vec, i) {
	node = (Node *) ELEM(vec, i);
	xmlFreeNode(node->node);
	node->node = NULL;
    }
    return (Object *) vec;
}

static void
freeDepsHash(Hash *hash)
{
    if (hash) {
	hashEach(hash, freeVectorDeps, NULL);
	objectFree((Object *) hash, TRUE);
    }
}

static xmlNode *
makeDependenciesNode(char *direction)
{
    xmlNode *deps = xmlNewNode(NULL, BAD_CAST "dependencies");
    if (direction) {
	(void) xmlNewProp(deps, "direction", direction);
    }
    return deps;
}

static void
addToDeps(xmlNode *deps, Vector *vec)
{
    Node *node;
    int i;
    if (vec) {
	EACH(vec, i) {
	    node = (Node *) ELEM(vec, i);
	    xmlAddChild(deps, node->node);
	    objectFree((Object *) node, FALSE);
	}
	objectFree((Object *) vec, FALSE);
    }
}


static void
addDepsForDiff(xmlNode *to_node, xmlNode *dbobject, boolean is_forwards)
{
    static String forwards_str = {OBJ_STRING, "forwards"};
    static String backwards_str = {OBJ_STRING, "backwards"};
    static String empty_str = {OBJ_STRING, ""};

    Hash *deps_hash = getNodeDeps(dbobject);
    xmlNode *deps;
    String *direction_str = is_forwards? &forwards_str: &backwards_str;

    deps = makeDependenciesNode(is_forwards? "forwards": "backwards");
    addToDeps(deps, (Vector *) hashDel(deps_hash, (Object *) &empty_str));
    addToDeps(deps, (Vector *) hashDel(deps_hash, (Object *) direction_str));
    freeDepsHash(deps_hash);
    xmlAddChild(to_node, deps);
}

static xmlNode *
diffPair(xmlNode *dbobject1, xmlNode *dbobject2, 
	 Hash *rules, boolean *diffs)
{
    xmlNode *contents1 = skipToContents(dbobject1);
    xmlNode *contents2 = skipToContents(dbobject2);
    xmlNode *volatile result = NULL;
    xmlNode *diffdeps = NULL;
    xmlNode *difflist = NULL;
    xmlNode *ruleset;
    xmlNode *context;
    xmlNode *content;
    boolean kids_differ = FALSE;
    DiffType difftype;
    boolean  do_rebuild = FALSE;

    if (ruleset = rulesetForNode(dbobject1, rules)) {
	difflist = elementDiffs(contents1, contents2, 
				ruleset, &diffdeps, &do_rebuild);
    }

    BEGIN {
	result = xmlCopyNode(dbobject2, 2);
	addDepsForDiff(result, dbobject1, FALSE);
	addDepsForDiff(result, dbobject2, TRUE);

	if (diffdeps) {
	    xmlAddChildList(result, diffdeps);
	}

	context = copyContext(dbobject1, "backwards");
	(void) xmlAddChildList(result, context);

	context = copyContext(dbobject2, "forwards");
	(void) xmlAddChildList(result, context);

	(void) xmlAddChildList(result, difflist);

	content = copyContents(contents2);
	(void) xmlAddChildList(result, content);

	processDiffs(contents1? contents1->children: NULL, 
		     contents2? contents2->children: NULL, 
		     rules, result, &kids_differ);
    }
    EXCEPTION(ex) {
	xmlFreeNode(result);
    }
    END;

    /* Update diff status and type. */
    if (difflist) {
	*diffs = TRUE;
	if (do_rebuild || nodeHasAttribute(ruleset, "rebuild")) {
	    difftype = IS_REBUILD;
	}
	else {
	    difftype = IS_DIFF;
	}
    }
    else if (!ruleset) {
	*diffs = TRUE;
	difftype = IS_UNKNOWN;
    }
    else if (kids_differ) {
	*diffs = TRUE;
	difftype = HAS_DIFFKIDS;
    }
    else {
	difftype = IS_SAME;
    }
    setDiff(result, difftype);

    return result;
}

static xmlNode *
diffSingle(xmlNode *dbobject, DiffType difftype,
	   Hash *rules, boolean *diffs)
{
    xmlNode *contents;
    xmlNode *volatile result = NULL;
    xmlNode *context;
    xmlNode *content;
    boolean ignore_this;

    *diffs = TRUE;
    BEGIN {
	result = xmlCopyNode(dbobject, 2);
	addNodeDeps(result, dbobject); 
	context = copyContext(dbobject, NULL);
	(void) xmlAddChildList(result, context);

	if (contents = skipToContents(dbobject)) {
	    content = copyContents(contents);
	    (void) xmlAddChildList(result, content);
	    if (difftype == IS_GONE) {
		processDiffs(contents->children, NULL, rules, 
			     result, &ignore_this);
	    }
	    else {
		processDiffs(NULL, contents->children, rules, 
			     result, &ignore_this);
	    }
	}
	setDiff(result, difftype);
    }
    EXCEPTION(ex) {
	xmlFreeNode(result);
    }
    END;

    return result;
}

static xmlNode *
dbobjectDiff(xmlNode *dbobject1, xmlNode *dbobject2, 
	     Hash *rules, boolean *diffs)
{
    if (dbobject1) {
	if (dbobject2) {
	    return diffPair(dbobject1, dbobject2, rules, diffs);
	}
	else {
	    return diffSingle(dbobject1, IS_GONE, rules, diffs);
	}
    }
    else if (dbobject2) {
	return diffSingle(dbobject2, IS_NEW, rules, diffs);
    }
    return NULL;
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
	node1objects = allDbobjects(node1, rules);

	while (dbobj2 = getDbobject(dbobj2)) {
	    diffs = FALSE;
	    match = getMatch(dbobj2, node1objects, rules);

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
    boolean has_diffs;

    (void) xmlNewProp(result, "dbname2", dbname2->value);
    (void) xmlNewProp(result, "time2", time2->value);
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
	//dbgSexp(doc2);
	rules = loadDiffRules(diffrules);
	result = processDiffRoot(xmlDocGetRootElement(doc1->doc), 
				 xmlDocGetRootElement(doc2->doc), rules);
	//dNode(result);
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

