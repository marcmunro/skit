/**
 * @file   object.c
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating objects.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../skit.h"
#include "../exceptions.h"

static Object dot = {OBJ_DOT};
static Object close_paren = {OBJ_CLOSE_PAREN};
static Object close_bracket = {OBJ_CLOSE_BRACKET};
static Object close_angle = {OBJ_CLOSE_ANGLE};

char * 
typeName(ObjType type)
{
    switch (type) {
    case OBJ_UNDEFINED: return "OBJ_UNDEFINED";
    case OBJ_INT4: return "OBJ_INT4"; 
    case OBJ_STRING: return "OBJ_STRING";
    case OBJ_CONS: return "OBJ_CONS";
    case OBJ_EXCEPTION: return "OBJ_EXCEPTION";
    case OBJ_VARRAY: return "OBJ_VARRAY";
    case OBJ_VECTOR: return "OBJ_VECTOR";
    case OBJ_HASH: return "OBJ_HASH"; 
    case OBJ_SYMBOL: return "OBJ_SYMBOL";
    case OBJ_OPTIONLIST: return "OBJ_OPTIONLIST";
    case OBJ_DOCUMENT: return "OBJ_DOCUMENT";
    case OBJ_XMLNODE: return "OBJ_XMLNODE";
    case OBJ_FN_REFERENCE: return "OBJ_FN_REFERENCE";
    case OBJ_OBJ_REFERENCE: return "OBJ_OBJ_REFERENCE";
    case OBJ_REGEXP: return "OBJ_REGEXP";
    case OBJ_CONNECTION: return "OBJ_CONNECTION";
    case OBJ_CURSOR: return "OBJ_CURSOR";
    case OBJ_TUPLE: return "OBJ_TUPLE";
    case OBJ_MISC: return "OBJ_MISC";
    case OBJ_DAGNODE: return "OBJ_DAGNODE";
    case OBJ_DEPENDENCY: return "OBJ_DEPENDENCY";
    case OBJ_DEPENDENCYSET: return "OBJ_DEPENDENCYSET";
    case OBJ_CONTEXT: return "OBJ_CONTEXT";
    default: return "UNKNOWN_OBJECT_TYPE";
    }
}

char *
objTypeName(Object *obj)
{
    if (obj) {
	return typeName(obj->type);
    }
    return "NULL";
}

Tuple *
tupleNew(Cursor *cursor)
{
    Tuple *tuple = (Tuple *) skalloc(sizeof(Tuple));
    tuple->type = OBJ_TUPLE;
    tuple->cursor = cursor;
    tuple->dynamic = TRUE;
    tuple->rownum = 0;
    return tuple;
}

Regexp *
regexpNew(char *str)
{
    Regexp *result = (Regexp *) skalloc(sizeof(Regexp));
    int err;
    int len;
    char *msg;
    result->type = OBJ_REGEXP;
    err = regcomp(&(result->regexp), str, REG_NEWLINE + REG_EXTENDED);
    if (err) {
	len = regerror(err, &(result->regexp), NULL, 0);
	msg = skalloc(len);
	(void) regerror(err, &(result->regexp), msg, len);
	RAISE(REGEXP_ERROR, msg);
    }
    result->src_str = newstr("%s", str);
    return result;
}

static Regexp *
regexpCopy(Regexp *regex)
{
    Regexp *result = (Regexp *) skalloc(sizeof(Regexp));
    int err;
    int len;
    char *msg;

    result->type = OBJ_REGEXP;
    result->src_str = newstr("%s", regex->src_str);
    err = regcomp(&(result->regexp), regex->src_str, 
		  REG_NEWLINE + REG_EXTENDED);
    if (err) {
	len = regerror(err, &(result->regexp), NULL, 0);
	msg = skalloc(len);
	(void) regerror(err, &(result->regexp), msg, len);
	RAISE(REGEXP_ERROR, msg);
    }
    return result;
}

static void
regexpFree(Regexp *re)
{
    regfree(&(re->regexp));
    skfree(re->src_str);
    skfree(re);
}


/* Predicate identifying whether the parameter is really an object. */
boolean
isObject(Object *obj)
{
    return (obj != NULL) && (obj->type > OBJ_UNDEFINED) && 
	(obj->type < OBJ_NOTOBJECT);
}


/* Create a new int4 object */
Int4 *
int4New(int value)
{
    Int4 *obj = (Int4 *) skalloc(sizeof(Int4));
    obj->type = OBJ_INT4;
    obj->value = value;
    return obj;
}

/* Create a new int4 object as a copy of an existing one. */
static Int4 *
int4Dup(Int4 *in)
{
    return int4New(in->value);
}

/* Free a dynamically allocate int4 object. */
static void
int4Free(Int4 *obj)
{
    skfree((void *) obj);
}

/* Compare two objects that appear to be integers using strcmp
 * semantics.  Note that the objects may be true int4 objects or may be
 * string objects that contain a number. */
int
int4Cmp(Int4 *p1, Int4 *p2)
{
    int result = 0;
    Int4 *c2;

    assert(p1 && p1->type == OBJ_INT4, "int4Cmp: p1 is not Int4");
    assert(p2, "int4Cmp: p2 is NULL");

    /* p2 must be an integer or a string.  If it is a string we try to
     * convert it to an integer.  If that fails we convert p1 to a
     * string and do a string comparison. */

    switch (p2->type) {
    case OBJ_INT4:
	result = p1->value - p2->value;
	break;
    case OBJ_STRING:
	BEGIN {
	    c2 = stringToInt4((String *) p2);
	    result = p1->value - c2->value;
	    objectFree((Object *) c2, TRUE);
	}
	EXCEPTION(ex);
	WHEN(TYPE_MISMATCH) {
	    char *tmpstr = newstr("%d", p1->value);
	    String *str1 = stringNew(tmpstr);
	    skfree(tmpstr);
	    result = stringCmp(str1, (String *) p2);
	    objectFree((Object *) str1, TRUE);
	}
	END;
	break;
    default:
	objectCmpFail((Object *) p1, (Object *) p2);
    }
    return result;
}

FnReference *
fnRefNew(void *fn)
{
    FnReference *result = (FnReference *) skalloc(sizeof(FnReference));
    result->type = OBJ_FN_REFERENCE;
    result->fn = fn;
    return result;
}

static void
fnRefFree(FnReference *ref)
{
    skfree(ref);
}

/* Return a reference to an object.  This is so that the reference can
 * be freed without the object that it references being freed.  Compound
 * objects such as hashes should always return references to their
 * objects when searched.  Manipulating a reference should be equivalent
 * to directly manipulating the referenced object.  TODO: make the
 * previous sentence true.
 * Note that attemping to create a reference to a reference, results in
 * a new reference to the underlying object rather than to the original
 * reference.
 */
ObjReference *
objRefNew(Object *obj)
{
    ObjReference *result = (ObjReference *) skalloc(sizeof(ObjReference));
    result->type = OBJ_OBJ_REFERENCE;
    
    if (obj && (obj->type == OBJ_OBJ_REFERENCE)) {
	result->obj = ((ObjReference *) obj)->obj;
    }
    else {
	result->obj = obj;
    }
    return result;
}

static void
objRefFree(ObjReference *ref)
{
    skfree(ref);
}


/* If a token appears to be a symbol, ensure it is recorded in the
 * symbol table and return the symbol object.  We treat the symbol nil
 * as a special case, returning a NULL pointer. */
static Symbol *
readSymbol(char *tok)
{
    if (streq(tok, "nil")) {
	return NULL;
    }
    return symbolNew(tok);
}

/* Copy a string token, handling escapes as we go */
static char *
escapedStrEval(char *tok)
{
    char *tmp = strEval(tok);
    char *src = tmp;
    char *result = skalloc(strlen(tmp) + 1);
    char *target = result;
    char c;
    while (c = *src++) {
	if (c == '\\') {
	    c = *src++;
	    switch(c) {
	    case 'n': c = '\n'; break;
	    case 'r': c = '\r'; break;
	    case 't': c = '\t'; break;
	    }
	}
	*target++ = c;
    }
    *target = '\0';
    skfree(tmp);
    return result;
}

/* Create an object from a string expression */
Object *
objectRead(TokenStr *sexp)
{
    char *tok;
    char *fails;
    TokenType ttype;
    Object *obj;
    char *tmp;

    for (tok = sexpTok(sexp); *tok; tok = sexpTok(sexp)) {
	ttype = tokenType(tok);
	switch (ttype) {
	case TOKEN_OPEN_BRACKET: 
	    obj = (Object *) consRead(&close_bracket, sexp);
	    return (Object *) toVector((Cons *) obj);
	case TOKEN_OPEN_ANGLE: 
	    obj = (Object *) consRead(&close_angle, sexp);
	    return (Object *) toHash((Cons *) obj);
	case TOKEN_OPEN_PAREN: 
	    return checkedConsRead(&close_paren, sexp);
	case TOKEN_CLOSE_PAREN: 
	    return &close_paren;
	case TOKEN_CLOSE_BRACKET: 
	    return &close_bracket;
	case TOKEN_CLOSE_ANGLE: 
	    return &close_angle;
	case TOKEN_NUMBER: 
	    return (Object *) int4New(atoi(tok));
	case TOKEN_QUOTE_STR: 
	    return (Object *) stringNewByRef(strEval(tok));
	case TOKEN_DBLQUOTE_STR: 
	    return (Object *) stringNewByRef(escapedStrEval(tok));
	case TOKEN_REGEXP: 
	    tmp = escapedStrEval(tok);
	    obj = (Object *) regexpNew(tmp);
	    skfree(tmp);
	    return obj;
	case TOKEN_DOT: 
	    return &dot;
	case TOKEN_SYMBOL: 
	    return (Object *) readSymbol(tok);
	default: 
	    fails = newstr("objectRead: Unhandled token %s (%d) in \"%s\"\n", 
			   tokenTypeName(ttype), ttype, tok);
	    RAISE(UNKNOWN_TOKEN, fails);
	}
    }
    RAISE(LIST_ERROR, newstr(
	      "objectRead: premature end of expression"));
    return NULL;
}

static Object *
trappedObjectRead(TokenStr *sexp)
{
    Object *obj = NULL;
    BEGIN {
        obj = objectRead(sexp);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	RAISE(ex->signal, newstr("%s while reading \"%s\"", 
				 ex->text, sexp->instr));
    }
    END;
    return obj;
}


/* Raise a TYPE_MISMATCH exception on behalf of a caller.  */
void
objectCmpFail(Object *obj1, Object *obj2)
{
    char *obj1s = objectSexp(obj1);
    char *obj2s = objectSexp(obj2);
    char *fails = newstr("Cannot compare %s with %s\n", obj1s, obj2s);
    skfree(obj1s);
    skfree(obj2s);
    RAISE(TYPE_MISMATCH, fails);
}

/* Compare two objects using strcmp semantics.  Note that the objects
 * must be of compatible types for the comparison.*/
int
objectCmp(Object *obj1, Object *obj2)
{
    if (obj1) {
	if (!obj2) {
	    return 1;
	}
    }
    else {
	return obj2? -1: 0;
    }
    assert(isObject(obj1), "objectCmp: obj1 is not an object");
    assert(isObject(obj2), "objectCmp: obj2 is not an object");

    //fprintf(stderr, "Objectcmp: %s %s\n", objectSexp(obj1), objectSexp(obj2));
    switch(obj1->type) {
	case OBJ_CONS: 
	    return consCmp((Cons *) obj1, (Cons *) obj2);
	case OBJ_INT4: 
	    return int4Cmp((Int4 *) obj1, (Int4 *) obj2);
	case OBJ_STRING: 
	    return stringCmp((String *) obj1, (String *) obj2);
	default: 
	    objectCmpFail(obj1, obj2);
	    return 0;
    }
}

//#define DEBUG_FREE
#ifdef DEBUG_FREE
static int free_depth = 0;

void
indent()
{
    int i;
    for (i = 0; i < free_depth; i++) {
	putc(' ', stderr);
    }
}

void
startFree(Object *obj)
{
    char *str = "";
    indent();
    if (obj->type == OBJ_SYMBOL) {
	str = ((Symbol *) obj)->name;
    }
    else if (obj->type == OBJ_STRING) {
	str = ((String *) obj)->value;
    }
    fprintf(stderr, "Free %s (%p) %s\n", objTypeName(obj), obj, str);
    free_depth++;
}

void
endFree(Object *obj)
{
    free_depth--;
    indent();
    fprintf(stderr, "Done (%p)\n", obj);
}
#else
#define startFree(x)
#define endFree(x)
#endif

Context *
contextNew(String *context_type, String *value, String *dflt)
{
    Context *new = skalloc(sizeof(Context));
    new->type = OBJ_CONTEXT;
    new->context_type = context_type;
    new->value = value;
    new->dflt = dflt;
    return new;
}

static DagNode *
basicDagNode()
{
    DagNode *new = skalloc(sizeof(DagNode));
    new->type = OBJ_DAGNODE;
    new->fqn = NULL;
    new->dbobject = NULL;
    new->build_type = UNSPECIFIED_NODE;
    new->status = UNVISITED;
    new->deps = NULL;
    
    return new;
}

DagNode *
dagNodeNew(xmlNode *node, DagNodeBuildType build_type)
{
    DagNode *new = basicDagNode();
    String *fqn;

    assert(node, "dagnodeNew: node not provided");
    fqn = nodeAttribute(node, "fqn");
    new->fqn = fqn;
    new->dbobject = node;
    new->status = UNVISITED;
    new->build_type = build_type;
    new->deps = NULL;
    new->tmp_deps = NULL;
    new->cur_dep = 0;
    new->is_fallback = FALSE;
    new->parent = NULL;
    new->mirror_node = NULL;
    return new;
}

DependencySet *
dependencySetNew(DagNode *definition_node)
{
    DependencySet *new = skalloc(sizeof(DependencySet));
    static int id = 1;
    new->type = OBJ_DEPENDENCYSET;
    new->id = id++;
    new->priority = 100;
    new->chosen_dep = 0;
    new->cycles = 0;
    new->direction_is_forwards = TRUE;
    new->has_fallback = FALSE;
    new->definition_node = definition_node;
    new->fallback_expr = NULL;
    new->fallback_parent = NULL;
    new->condition = NULL;
    new->deactivated = FALSE;
    new->deps = vectorNew(10);
    return new;
}

Dependency *
dependencyNew(String *qn, boolean qn_is_full, boolean is_forwards)
{
    Dependency *new = skalloc(sizeof(Dependency));
    static int id = 1;
    new->type = OBJ_DEPENDENCY;
    new->id = id++;
    new->qn = qn;
    new->qn_is_full = qn_is_full;
    new->is_forwards = is_forwards;
    new->propagate_mirror = FALSE;
    new->dep = NULL;
    new->depset = NULL;
    new->from = NULL;
    new->condition = NULL;
    new->endfallback = NULL;
    return new;
}


static void
dagNodeFree(DagNode *node)
{
    objectFree((Object *) node->fqn, TRUE);
    objectFree((Object *) node->deps, FALSE);
    skfree(node);
}

static void
contextFree(Context *ctx, boolean free_contents)
{
    if (free_contents) {
	objectFree((Object *) ctx->context_type, TRUE);
	objectFree((Object *) ctx->value, TRUE);
	objectFree((Object *) ctx->dflt, TRUE);
    }
    skfree(ctx);
}

static void
dependencySetFree(DependencySet *depset, boolean free_contents)
{
    if (free_contents) {
	objectFree((Object *) depset->deps, TRUE);
	objectFree((Object *) depset->fallback_expr, TRUE);
	objectFree((Object *) depset->fallback_parent, TRUE);
	objectFree((Object *) depset->condition, TRUE);
    }
    skfree(depset);
}

static void
dependencyFree(Dependency *dep, boolean free_contents)
{
    if (free_contents) {
	objectFree((Object *) dep->qn, TRUE);
	objectFree((Object *) dep->condition, TRUE);
    }
    skfree((Object *) dep);
}

/* Free a dynamically allocated object. */
void
objectFree(Object *obj, boolean free_contents)
{
    char *fails;
    if (obj) {
	startFree(obj);
	switch (obj->type) {
	case OBJ_CONS: 
	    consFree((Cons *) obj, free_contents); break;
	case OBJ_INT4: 
	    int4Free((Int4 *) obj); break;
	case OBJ_STRING: 
	    stringFree((String *) obj, free_contents); break;
	case OBJ_VECTOR:
	    vectorFree((Vector *) obj, free_contents); break;
	case OBJ_HASH:
	    hashFree((Hash *) obj, free_contents); break;
	case OBJ_SYMBOL:
	    /* Note that symbolFree does not free symbols that are in
	     * the symbol table.  We maybe shouldn't bother with
	     * symbolFree at all since all symbols should be in the
	     * table but I guess it does no harm. */
	    symbolFree((Symbol *) obj, free_contents); break;
	case OBJ_DOCUMENT:
	    documentFree((Document *) obj, free_contents); break;
	case OBJ_XMLNODE:
	    skfree(obj); break;
	case OBJ_FN_REFERENCE:
	    fnRefFree((FnReference *) obj); break;
	case OBJ_OBJ_REFERENCE:
	    objRefFree((ObjReference *) obj); break;
	case OBJ_REGEXP:
	    regexpFree((Regexp *) obj); break;
	case OBJ_CONNECTION:
	    connectionFree((Connection *) obj); break;
	case OBJ_CURSOR:
	    cursorFree((Cursor *) obj); break;
	case OBJ_DAGNODE:
	    dagNodeFree((DagNode *) obj); break;
	case OBJ_DEPENDENCY:
	    dependencyFree((Dependency *) obj, free_contents); break;
	case OBJ_DEPENDENCYSET:
	    dependencySetFree((DependencySet *) obj, free_contents); break;
	case OBJ_CONTEXT:
	    contextFree((Context*) obj, free_contents); break;
	case OBJ_TUPLE:
	    if (((Tuple *) obj)->dynamic) {
		skfree(obj);
	    }
	    break;
	default: 
	    chunkInfo(obj);
	    fails = newstr("objectFree: Unhandled type: %d in %p\n", 
			   obj->type, obj);
	    RAISE(UNHANDLED_OBJECT_TYPE, fails);
	}
	endFree(obj);
    }
}

char *
nameForBuildType(DagNodeBuildType build_type)
{
    switch (build_type) {
    case BUILD_NODE: return "build";
    case DROP_NODE: return "drop";
    case DIFF_NODE: return "diff";
    case ARRIVE_NODE: return "arrive";
    case DEPART_NODE: return "depart";
    case EXISTS_NODE: return "exists";
    case REBUILD_NODE: return "rebuild";
    case DIFFPREP_NODE: return "diffprep";
    case OPTIONAL_NODE: return "optional";
    case BUILD_AND_DROP_NODE: return "build and drop";
    case FALLBACK_NODE: return "fallback";
    case DSFALLBACK_NODE: return "dsfallback";
    case ENDFALLBACK_NODE: return "endfallback";
    case DSENDFALLBACK_NODE: return "dsendfallback";
    case DEACTIVATED_NODE: return "deactivated";
    default: return "UNKNOWNBUILDTYPE";
    }
}

boolean
depIsActive(Dependency *dep)
{
    /* DO NOT ATTEMPT TO DEBUG THIS USING dbgSexp, printSexp, or
     * objectSexp, directly or indirectly, as it is called from
     * dependencySexp and will recurse uncontrollbaly. */

    Dependency *cur_dep;

    assertDependency(dep);
    if (!dep->dep) {
	return FALSE;
    }

    if (dep->depset) {
	assertDependencySet(dep->depset);
	assertDagNode(dep->dep);
	if (dep->depset->deactivated) {
	    return FALSE;
	}

	if (dep->depset->chosen_dep >= dep->depset->deps->elems) {
	    return FALSE;
	}
	if (cur_dep = (Dependency *) dereference(
		ELEM(dep->depset->deps, dep->depset->chosen_dep)))
	{
	    //assertDependency(cur_dep);
	    if (cur_dep->endfallback == dep) {
		return TRUE;
	    }
	}

	return (cur_dep == dep);
    }
    return TRUE;
}

static char *
dependencySexp(Dependency *dep)
{
    char *depset_indicator = dep->depset? 
	(depIsActive(dep)? "*": "-"): "";
    char *direction = dep->is_forwards? "-->": "<--";
    char *tmp;
    char *name;
    char *result;

    if (dep->dep) {
	tmp = objectSexp((Object *) dep->dep);
	name = newstr("dep: %s", tmp);
	skfree(tmp);
    }
    else if (dep->qn_is_full) {
	name = newstr("fqn: \"%s\"", dep->qn->value);
    }
    else {
	name = newstr("pqn: \"%s\"", dep->qn->value);
    }
    
    result = newstr("<%s(%d)%s %s%s>", objTypeName((Object *) dep), 
		    dep->id, depset_indicator, direction, name);
    skfree(name);
    return result;
}

static char *
dependencySetSexp(DependencySet *depset)
{
    char *fqn;
    char *tmp;
    char *tmp2;

    if (depset->definition_node) {
	fqn = depset->definition_node->fqn->value;
    }
    else {
	fqn = "??";
    }
    tmp = objectSexp((Object *) depset->deps);
    tmp2 = newstr("<%s(%d) for (%s) %s %s>", objTypeName((Object *) depset), 
		  depset->id, 
		  nameForBuildType(depset->definition_node->build_type),
		  fqn, tmp);
    skfree(tmp);
    return tmp2;
}


/* Return dynamically-created string representation of object. 
 * The full argument allows more information to be returned about the
 * object, for debugging purposes. */
char *
objectSexp(Object *obj)
{
    char *fails;
    char *tmp;
    char *tmp2;
    Context *context;

    if (!obj) {
	return newstr("nil");
    }

    switch(obj->type) {
    case OBJ_INT4:
	return newstr("%d", ((Int4 *)obj)->value);
    case OBJ_STRING: 
	return newstr("'%s'", ((String *)obj)->value);
    case OBJ_CONS: 
	return consStr((Cons *) obj);
    case OBJ_VECTOR: 
	return vectorStr((Vector *) obj);
    case OBJ_HASH: 
	return hashStr((Hash *) obj);
    case OBJ_SYMBOL: 
	return symbolStr((Symbol *) obj);
    case OBJ_DOCUMENT: 
	return documentStr((Document *) obj);
    case OBJ_OBJ_REFERENCE:
	return objectSexp((Object *) ((ObjReference *) obj)->obj);
    case OBJ_EXCEPTION: 
	return newstr("<Exception: %s>", ((Exception *) obj)->backtrace);
    case OBJ_FN_REFERENCE:
	return newstr("<%s %p>", objTypeName(obj), 
		      ((FnReference *) obj)->fn);
    case OBJ_CONNECTION:
	return newstr("<%s %p>", objTypeName(obj), 
		      ((Connection *) obj)->conn);
    case OBJ_XMLNODE:
	tmp = nodestr(((Node *)obj)->node);
	tmp2 = newstr("<%s (%s)>", objTypeName(obj), tmp);
	skfree(tmp);
	return tmp2;
    case OBJ_MISC:
	return newstr("<%s %p>", objTypeName(obj), obj);
    case OBJ_DEPENDENCY:
	return dependencySexp((Dependency *) obj);
    case OBJ_DEPENDENCYSET:
	return dependencySetSexp((DependencySet *) obj);
    case OBJ_DAGNODE:
	return newstr("<%s (%s) %s>", objTypeName(obj), 
		      nameForBuildType(((DagNode *) obj)->build_type), 
		      ((DagNode *) obj)->fqn->value); 
    case OBJ_CURSOR:
	return cursorStr((Cursor *) obj);
    case OBJ_TUPLE:
	return tupleStr((Tuple *) obj);
    case OBJ_REGEXP:
	return newstr("/%s/", ((Regexp *) obj)->src_str);
    case OBJ_CONTEXT:
	context = (Context *) obj;

	return newstr("<Context %s, %s, %s>", 
		      context->context_type->value, context->value->value,
		      context->dflt->value);
    default: 
	// TODO: improve this string.
	return newstr("{BROKEN OBJECT: %p}", obj);
	
	fails = newstr("objectSexp: Unhandled type: %d\n", obj->type);
	RAISE(UNHANDLED_OBJECT_TYPE, fails);
    }
}

/* Create a deep copy of a cons cell. */
static Cons *
consDup(Cons *cons)
{
    Object *volatile carcopy = NULL;
    Object *volatile cdrcopy = NULL;
    Cons *result = NULL;

    assert(cons && (cons->type == OBJ_CONS), "consDup arg is not a cons cell");

    BEGIN {
	carcopy = objectCopy(cons->car);
	cdrcopy = objectCopy(cons->cdr);
	result = consNew(carcopy, cdrcopy);
    }
    EXCEPTION(ex) {
	objectFree(carcopy, TRUE);
	objectFree(cdrcopy, TRUE);
    }
    END;
    return result;
}


/* Execute a cons cell as a lisp expression.  */
static Object *
consEval(Cons *cons)
{
    assert(cons && (cons->type == OBJ_CONS), "consEval arg is not a cons cell");
    return symbolExec((Symbol *) cons->car, cons->cdr);
}

/* Create a deep copy of obj. */
Object *
objectCopy(Object *obj)
{
    char *fails;
    if (!obj) {
	return NULL;
    }
    obj = dereference(obj);
    switch(obj->type) {
    case OBJ_CONS: 
	return (Object *) consDup((Cons *) obj);
    case OBJ_INT4: 
	return (Object *) int4Dup((Int4 *) obj);
    case OBJ_STRING: 
	return (Object *) stringDup((String *) obj);
    case OBJ_SYMBOL: 
	return (Object *) symbolCopy((Symbol *) obj);
    case OBJ_REGEXP: 
	return (Object *) regexpCopy((Regexp *) obj);
    case OBJ_HASH: 
	return (Object *) hashCopy((Hash *) obj);
    default: 
	fails = newstr("objectCopy: Unhandled type: %d in %p\n", 
			obj->type, obj);
	RAISE(UNHANDLED_OBJECT_TYPE, fails);
    }
    return NULL;
}


Object *
dereference(Object *obj)
{
    //printSexp(stderr, "PARAM: ", obj);
    while (obj && (obj->type == OBJ_OBJ_REFERENCE)) {
	obj = (Object *) ((ObjReference *) obj)->obj;
	//printSexp(stderr, "OBJ: ", obj);
    }
    return obj;
}

int
objType(Object *obj)
{
    if (obj = dereference(obj)) {
	return obj->type;
    }
    return 0;
}


/* Evaluates obj, returning a new evaluated obj.  The object may a
 * symbol, in which case its value as a variable is returned, or a
 * list, in which case it is evaluated as a lisp expression.  */
Object *
objectEval(Object *obj)
{
    Object *result = NULL;
    if (obj) {
	switch (obj->type) {
	case OBJ_CONS: 
	    result = (Object *) consEval((Cons *) obj);
	    return result;
	case OBJ_SYMBOL:
	    result = (Object *) symbolEval((Symbol *) obj);
	    return result;
	default:
	    result = objectCopy(obj);
	    return result;
	}
    }

    return NULL;
}


/* As objectEval but with a nicely formatted exception message.
 */
Object *
trappedObjectEval(Object *obj)
{
    BEGIN {
	RETURN(objectEval(obj));
    }
    EXCEPTION(ex);
    WHEN(LIST_ERROR) {
	char *sexp = objectSexp(obj);
	char *msg = newstr("%s in expr \"%s\"", ex->text, sexp);
	//fprintf(stderr, "TRAPPED: %s\n", ex->text);
	skfree(sexp);
	RAISE(ex->signal, msg);
    }
    END;

    return NULL;
}

/* Evaluate a string expression. */
Object *
evalSexp(char *str)
{
    Object *volatile obj;
    Object *result = NULL;
    TokenStr token_str = {str, '\0', NULL};

    if (obj = trappedObjectRead(&token_str)) {
	BEGIN {
	    result = trappedObjectEval(obj);
	}
	EXCEPTION(ex);
	FINALLY {
	    if (result != obj) {
		objectFree(obj, TRUE);
	    }
	}
	END;
    }
    return result;
}

/* Print a string expression from object to the specified stream, and
 * starting with the specified prefix.  This is for debugging. */
void
printSexp(void *stream, char *prefix, Object *obj)
{
    char *tmp = objectSexp(obj);
    fprintf(stream, "%s%s\n", prefix, tmp);
    skfree(tmp);
}

void pSexp(Object *obj)
{
    printSexp(stderr, "", obj);
}

Object *
objSelect(Object *collection, Object *key)
{
    Object *result = NULL;
    collection = dereference(collection);
    switch (collection->type) {
    case OBJ_HASH: result = hashGet((Hash *) collection, key); break;
    case OBJ_CONS: result = consGet((Cons *) collection, key); break;
    case OBJ_VECTOR: result = vectorGet((Vector *) collection, key); break;
    case OBJ_TUPLE: return (Object *) tupleGet((Tuple *) collection, key);
    case OBJ_CURSOR: return (Object *) cursorGet((Cursor *) collection, key);
    default: 
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("objSelect: no method for selecting from %s",
		     objTypeName(collection)));
    }
    return (Object *) objRefNew(result);
}

Object *
objNext(Object *collection, Object **p_placeholder)
{
    collection = dereference(collection);
    switch (collection->type) {
    case OBJ_HASH: 
	return hashNext((Hash *) collection, p_placeholder); 
    case OBJ_CONS: 
	return consNext((Cons *) collection, p_placeholder); 
    case OBJ_CURSOR: 
	return cursorNext((Cursor *) collection, p_placeholder); 
    case OBJ_STRING: 
	return (Object *) stringNext((String *) collection, p_placeholder); 
    default: 
	RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("objNext: no method for selecting from %s",
		     objTypeName(collection)));
    }
    return NULL;
}

boolean
isCollection(Object *object)
{
    object = dereference(object);
    switch (object->type) {
    case OBJ_CONS: 
    case OBJ_CURSOR:
    case OBJ_STRING:
    case OBJ_HASH:
	return TRUE;
    default: return FALSE;
    }
}

Object *
objectFromStr(char *instr)
{
    char *volatile str = newstr("%s", instr);
    Object *obj = NULL;
    TokenStr token_str = {str, '\0', NULL};

    BEGIN {
	obj = objectRead(&token_str);
    }
    EXCEPTION(e);
    WHEN_OTHERS {
	skfree(str);
	RAISE();
    }
    END;
    skfree(str);
    return obj;
}

/*
boolean
checkDagnode(DagNode *node, void *chunk)
{
    boolean found;
    if (found = checkString(node->fqn, chunk)) {
	printSexp(stderr, "...within fqn of ", (Object *) node);
    }
    if (checkVector(node->dependencies, chunk)) {
	printSexp(stderr, "...within dependencies of ", (Object *) node);
	found = TRUE;
    }
    if (checkVector(node->dependents, chunk)) {
	printSexp(stderr, "...within dependents of ", (Object *) node);
	found = TRUE;
    }
    if (checkChunk(node, chunk)) {
	printSexp(stderr, "...within ", (Object *) node);
	found = TRUE;
    }
    return found;
}
*/

boolean
checkObj(Object *obj, void *chunk)
{
    if (obj) {
	switch (obj->type) {
	case OBJ_HASH: return checkHash((Hash *) obj, chunk);
	case OBJ_CONS: return checkCons((Cons *) obj, chunk);
	case OBJ_STRING: return checkString((String *) obj, chunk);
	case OBJ_SYMBOL: return checkSymbol((Symbol *) obj, chunk);
	case OBJ_MISC: /* MISC objects are not skalloc'd */ return FALSE;
	    //case OBJ_DAGNODE: return checkDagnode((DagNode *) obj, chunk);
	case OBJ_INT4: 
	    if (checkChunk(obj, chunk)) {
		printSexp(stderr, "...within Int4 ", obj);
		return TRUE;
	    }
	    return FALSE;
	case OBJ_OBJ_REFERENCE: 
	    if (checkChunk(obj, chunk)) {
		fprintf(stderr, "...within OBJReference\n");
		return TRUE;
	    }
	    return FALSE;
	case OBJ_VECTOR: 
	case OBJ_EXCEPTION: 
	case OBJ_VARRAY: 
	case OBJ_OPTIONLIST: 
	case OBJ_DOCUMENT: 
	case OBJ_XMLNODE: 
	case OBJ_FN_REFERENCE: 
	case OBJ_REGEXP: 
	case OBJ_CONNECTION: 
	case OBJ_CURSOR: 
	case OBJ_TUPLE: 
	case OBJ_DEPENDENCY:
	    RAISE(NOT_IMPLEMENTED_ERROR, 
	      newstr("checkObj: no check yet for objects of type %s",
		     objTypeName(obj)));
	default:
	    RAISE(UNHANDLED_OBJECT_TYPE, 
		  newstr("checkOBJ: cannot check objects of type %d", 
			 obj->type));
	}
    }
    return FALSE;
}
