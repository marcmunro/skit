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

#ifdef wurblyburby

static void
addUnhandled(Hash *unhandled, String *type)
{
    Int4 *count = (Int4 *) hashGet(unhandled, (Object *) type);
    String *key;
    if (count) {
	count->value++;
    }
    else {
	count = int4New(1);
	key = stringNew(type->value);
	hashAdd(unhandled, (Object *) key, (Object *) count);
    }
}

#if 0

static void
applyDiffRule(Node *this, Hash *objects2, Hash *rules,
	      Vector *results, Hash *unhandled)
{
    String *type = nodeAttribute(this->node, "type");
    Cons *diffrule = (Cons *) hashGet(rules, (Object *) type);
    if (diffrule) {
	fprintf(stderr, "Handling type %s\n", type->value);
    }
    else {
	addUnhandled(unhandled, type);
    }
    objectFree((Object *) type, TRUE);
}

static void
addNodeToList(xmlNode *node, Object *list)
{
    Node *dbobjectnode = nodeNew(node);
    (void) vectorPush((Vector *) list, (Object *) dbobjectnode);
}

static Vector *
dbobjectListFromDoc(Document *doc)
{
    Vector *list = vectorNew(1000);   /* As good a starting point as any */
    xmlNode *root = xmlDocGetRootElement(doc->doc);
    eachDbobject(root, (Object *) list, addNodeToList);
    return list;
}

static Document *
processDiff(Document *doc1, Document *doc2, Hash *rules)
{
    Vector *objects1 = dbobjectListFromDoc(doc1);
    Hash *objects2 = dbobjectHashFromDoc(doc1);
    Vector *results = vectorNew(objects1->elems * 1.5);
    Hash *unhandled = hashNew(TRUE);
    int i;
    for (i = 0; i < objects1->elems; i++) {
	applyDiffRule((Node *) objects1->contents->vector[i], objects2,
		      rules, results, unhandled);
    }
    // TODO: PROCESS REMAINING ELEMENTS IN OBJECTS2 AS NEW OBJECTS

    hashEach(unhandled, listUnhandled, NULL);
    objectFree((Object *) results, TRUE);
    objectFree((Object *) unhandled, TRUE);
    objectFree((Object *) objects1, TRUE);
    objectFree((Object *) objects2, TRUE);
    return NULL;
}

static xmlNode *
copyTreeAsDiff(xmlNode *node, DiffType difftype)
{
    xmlNode *next = node;
    xmlNode *result = NULL;
    xmlNode *prev = NULL;
    xmlNode *this;

    while (next) {
	this = xmlCopyNode(node, 2);
	if (prev) {
	    prev->next = this;
	}
	else if (!result) {
	    result = this;
	}
	prev = this;
	if (streq("dbobject", (char *) this->name)) {
	    setDiff(this, difftype);
	}
	if (next->children) {
	    this->children = copyTreeAsDiff(next->children, difftype);
	}
	next = next->next;
    }
    return result;
}

static xmlNode *
nodeDiffs(xmlNode *node, Node *match)
{
    return NULL;
}

static xmlNode *
dbojectDiff(xmlNode *node, Hash *others, Hash *rules, Hash *unhandled)
{
    String *type = nodeAttribute(node, "type");
    String *match_fqn;
    Node *rule = getRule(rules, type);
    Node *match;
    xmlNode *result;
    if (!rule) {
	addUnhandled(unhandled, type);
	result = copyTreeAsDiff(node, IS_NEW);
    }
    else if (match = getDiffMatch(node, rule, others)) {
	result = nodeDiffs(node, match);
	objectFree((Object *) match, TRUE);
    }
    else {
	result = copyTreeAsDiff(node, IS_NEW);
    }
    objectFree((Object *) type, TRUE);
    return result;
}

xmlNode *
dbobjectDiffsAtLevel(xmlNode *node, Hash *others, Hash *rules, Hash *unhandled)
{
    xmlNode *this = node;
    xmlNode *first = NULL;
    xmlNode *prev = NULL;
    xmlNode *new;
    String *fqn;
    String *type;
    while (this) {
	if (streq("dbobject", (char *) this->name)) {
	    type = nodeAttribute(this, "type");
	    fqn = nodeAttribute(this, "fqn");
	    dbgSexp(type);
	    dbgSexp(fqn);
	    objectFree((Object *) type, TRUE);
	    objectFree((Object *) fqn, TRUE);
	    new = dbojectDiff(this, others, rules, unhandled);
	    if (prev) {
		prev->next = new;
	    }
	    else if (!first) {
		first = new;
	    }
	    prev = new;
	}
	else {
	    RAISE(XML_PROCESSING_ERROR,
		  newstr("EXPECTING DBOBJECT, GOT %s", this->name));
	}
	this = this->next;
    }
    return first;
}

#endif

static Cons *
getRule(Hash *rules, String *type)
{
    return (Cons *) hashGet(rules, (Object *) type);
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
    case HAS_DIFFKIDS: type = "DifKids"; break;
    }

    (void) xmlNewProp(node, (const xmlChar *) "diff", type);
}

static Object *
listUnhandled(Object *obj, Object *ignore)
{
    String *type = (String *) ((Cons *) obj)->car;
    Int4 *count = (Int4 *) ((Cons *) obj)->cdr;
    fprintf(stderr, "UNHANDLED: %d instance%s of %s\n", count->value,
	    count->value == 1? "": "s", type->value);
    return (Object *) count;
}

static Node *
getDiffMatch(xmlNode *node, Cons *rule, Hash *others)
{
    String *key = NULL;
    //DOING STUFF HERE
    dbgSexp(rule);
    return NULL;
}

static xmlNode *
diffDbobject(
    xmlNode *node, 
    Hash *rules, 
    Hash *dbobjects2)
{
    xmlNode *result = xmlCopyNode(node, 2);
    String *type = nodeAttribute(node, "type");
    Cons *rule = getRule(rules, type);
    Node *other;

    dbgNode(node);
    dbgSexp(rule);
    if (rule) {
	other = getDiffMatch(node, rule, dbobjects2);
    }
    //else {
    //	addUnhandled(unhandled, type);
    //}

    objectFree((Object *) type, TRUE);
    return result;
}

static Object *
newDbobjectFromHash(Object *obj, Object *resultptr)
{
    Node *node= (Node*) ((Cons *) obj)->cdr;
    xmlNode **p_result = (xmlNode **) resultptr;
    xmlNode *prev = *p_result;
    xmlNode *new = xmlCopyNode(node->node, 2); 
    setDiff(new, IS_NEW);
    while (prev && prev->next) {
	prev = prev->next;
    }
    if (prev) {
	prev->next = new;
    }
    else {
	*p_result = new;
    }
    return (Object *) node;
}

#endif

static void
addNodeToHash(xmlNode *node, Hash *hash)
{
    Node *dbobjectnode = nodeNew(node);
    String *fqn = nodeAttribute(node, "fqn");
    hashAdd((Hash *) hash, (Object *) fqn, (Object *) dbobjectnode);
}

static xmlNode *
skipToMatchingnode(xmlNode *node, xmlNode *to_match)
{
    xmlNode *this = getElement(node);
    while (this) {
	if (streq((char *) this->name, (char *) to_match->name)) {
	    return this;
	}
	this = getElement(this->next);
    }
    return this;
}

static Cons *
addNodeForLevel(xmlNode *dbobject, Cons *alist, String *type, String *key)
{
    dbgSexp(alist);
    Cons *type_entry = (Cons *) alistGet(alist, (Object *) type);
    Hash *hash;
    Node *existing_entry;
    Node *node = nodeNew(dbobject);
    char *errmsg;
    char *str1;
    char *str2;
    if (type_entry) {
	objectFree((Object *) type, TRUE);
	hash = (Hash *) type_entry->cdr;
    }
    else {
	hash = hashNew(TRUE);
	type_entry = consNew((Object *) type, (Object *) hash);
	alist = consNew((Object *) type_entry, (Object *) alist);
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

static Cons *
dbobjectsFromLevel(xmlNode *node, Hash *rules)
{
    Cons *alist = NULL;
    String *type;
    char *keyattr;
    String *key;
    Cons *rule;
    xmlNode *this = node;

    while (this) {
	if (streq("dbobject", (char *) this->name)) {
	    type = nodeAttribute(this, "type");
	    if (rule = (Cons *) hashGet(rules, (Object *) type)) {
		keyattr = ((String *) rule->car)->value;
	    }
	    else {
		keyattr = "fqn";
	    }
	    key = nodeAttribute(this, keyattr);
	    alist = addNodeForLevel(this, alist, type, key);
	}
	this = getElement(this->next);
    }
    return alist;
}

static xmlNode *
processDiffs(
    xmlNode *node1, 
    xmlNode *node2, 
    Hash *rules,
    int level)
{
    xmlNode *next1 = getElement(node1);
    xmlNode *next2 = getElement(node2);
    xmlNode *result = NULL;
    xmlNode *new = NULL;
    xmlNode *prev = NULL;
    Cons *dbobjects2 = NULL;

    while (next1) {
	dbgNode(next1);
	dbgNode(next2);
	if (streq("dbobject", (char *) next1->name)) {
	    if (!dbobjects2) {
		/* Create an alist keyed by dbobject type of
		 * hashes of nodes.  The hashes will be indexed
		 * according to the rules for the given dbobject type
		 * or by fqn if no rule can be found. */
		dbobjects2 = dbobjectsFromLevel(node2, rules);
		dbgSexp(dbobjects2);
	    }
# HERE.  Next we implement diffDboject.  This should
# create and return a new node with some diff info
# the types will be New, Dropped, Identical, Diff, DiffKids,
# norule_orig, norule, new
# We also need to remove the recursion stopping level parameter 
# once everything is stable.
	    new = NULL;
	    //new = diffDbobject(next1, rules, dbobjects2);
	}
	else {
	    /* Copy this node and process the kids. */ 
	    new = xmlCopyNode(next1, 2);
	    next2 = skipToMatchingnode(next2, next1);
	    if (next1->children && (level > 1)) {
		new->children = processDiffs(next1->children,
					     next2? next2->children: NULL, 
					     rules, level - 1);
	    }
	}
	if (!result) {
	    result = new;
	}
	if (prev) {
	    prev->next = new;
	}
	prev = new;
	next1 = getElement(next1->next);
    }

    if (dbobjects2) {
	dbgSexp(dbobjects2);
#ifdef wib
	hashEach(dbobjects2, newDbobjectFromHash, (Object *) &result);
#endif
	objectFree((Object *) dbobjects2, TRUE);
    }
    return result;
}

static xmlNode *
processDiff(Document *doc1, Document *doc2, Hash *rules)
{
    xmlNode *root1 = xmlDocGetRootElement(doc1->doc);
    xmlNode *root2 = xmlDocGetRootElement(doc1->doc);
    xmlNode *diffroot;
    //Hash *unhandled = hashNew(TRUE);

    diffroot = processDiffs(root1, root2, rules, 3);
    //hashEach(unhandled, listUnhandled, NULL);
    //objectFree((Object *) unhandled, TRUE);
    return diffroot;
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
	dbgSexp(rules);
	result = processDiff(doc1, doc2, rules);
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
