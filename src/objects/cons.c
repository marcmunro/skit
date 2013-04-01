/**
 * @file   cons.c
 * \code
 *     Copyright (c) 2009, 2010, 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating cons cells.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../skit_lib.h"
#include "../exceptions.h"

/* Predicate to check whether an object is really a cons 
 * QUESTION: would the arg be better as an (Object *)? */
boolean
isCons(Cons *obj)
{
    return obj && (obj->type == OBJ_CONS);
}

/* Create a new cons containing car and cdr */
Cons *
consNew(Object *car, Object *cdr)
{
    Cons *cons = (Cons *) skalloc(sizeof(Cons));

    cons->type = OBJ_CONS;
    cons->car = car;
    cons->cdr = cdr;
    return cons;
}

/* Free a cons, optionally freeing the contents */
void
consFree(Cons *cons, boolean free_contents)
{
    //fprintf(stderr, "cons: %p  ", cons);
    //dbgSexp(cons);
    if (free_contents) {
	objectFree(cons->car, free_contents);
	objectFree(cons->cdr, free_contents);
    }
    skfree((void *) cons);
}

/* Using a list as a stack, push an object onto the head. */
Object *
consPush(Cons **head, Object *obj)
{
    Cons *cons;

    if (*head) {
	assert((*head)->type == OBJ_CONS, 
	       "consPush: head is not a cons cell pointer");
    }
    cons = consNew(obj, (Object *) *head);
    *head = cons;
    return obj;
}

/* Using a list as a stack, pop the top item off the stack and return
 * it. */
Object *
consPop(Cons **head)
{
    Cons *cons;
    Object *obj;

    if (*head) {
	assert((*head)->type == OBJ_CONS, 
	       "consPop: head is not a cons cell pointer");
    }
    cons = *head;
    obj = cons->car;
    *head = (Cons *) cons->cdr;
    consFree(cons, FALSE);
    return obj;
}


/* Read a cons cell using objectRead.  We have already read the opening
 * token at this point, so closer describes the expected closing
 * token.  Note that this is recursive and that exceptions may be
 * raised.   This differs from checkedConsRead below in that this may
 * be called in the middle of a list, so '.' is allowed.  */
Cons *
consRead(Object *closer, TokenStr *sexp)
{
    Cons *volatile cons;
    Object *obj;
    Object *cdr;
    obj = objectRead(sexp);
    if (obj) {
	if (obj->type >= OBJ_NOTOBJECT) {
	    /* ObjectRead has returned us a control object: ie one that
	     * provides syntax. */
	    if (obj->type == OBJ_DOT) {
		/* Instead of reading the rest of a list, we are reading
		 * an alist entry.  Return the control object back to
		 * the caller, where it will deal with it. */
		return (Cons *) obj;
	    }
	    if (obj == closer) {
		/* We have just read the token that closes this list.
		 * Return an empty list (NULL) to the caller. */
		return NULL;
	    }
	    RAISE(LIST_ERROR, newstr("Invalid closing token at end of list"));
	}
    }
    BEGIN {
	cons = consNew(NULL, NULL);
	setCar(cons, obj);
	cdr = (Object *) consRead(closer, sexp);
	if (cdr &&(cdr->type == OBJ_DOT)) {
	    char *tok;
	    /* We are reading alist syntax.  The cdr of this cons cell will
	     * not be another cons but a directly referenced  object. */
	    cdr = objectRead(sexp);
	    /* And now we should have a closing paren token.  Ensure that we
	     * have.  */
	    tok = sexpTok(sexp);
	    if (!streq(tok, ")")) {
		objectFree((Object *) cdr, TRUE);
		RAISE(LIST_ERROR, newstr(
			  "consRead: Unexpected objects after cdr of alist"));
	    }
	}
	setCdr(cons, cdr);
	RETURN(cons);
    }
    EXCEPTION(e) {
	objectFree((Object *) cons, TRUE);
	RAISE();
    }
    END;
}


/* Read a cons cell using consRead.  As described in consRead above, we
 * have already read the opening token at this point, so closer
 * describes the expected closing token.  This function is called at
 * the start of a list, so the first token may not be a '.'.  */
Object *
checkedConsRead(Object *closer, TokenStr *sexp)
{
    Object *obj = (Object *) consRead(closer, sexp);
    if (obj && (obj->type >= OBJ_NOTOBJECT)) {
	if (obj->type == OBJ_DOT) {
	    RAISE(LIST_ERROR, newstr("checkedConsRead: list beginning with "
				     "\".\" not permitted"));

	}
	RAISE(LIST_ERROR, newstr("checkedConsRead: unexpected object type: %d", 
				 obj->type));
    }
    return (obj);
}

/* Convert the contents of a list into a dynamically allocated C
 * string.  Note that this deals only with the contents of a list and
 * not with the containing parentheses, which are dealt with in consStr
 * below.  */
static char *
listStr(Cons *cons)
{
    char *result;
    char *carstr;
    Object *cdr;

    assert(cons && (cons->type == OBJ_CONS), 
	   "listStr arg is not a cons cell");
    
    carstr = objectSexp(cons->car);
    cdr = cons->cdr;

    if (cdr) {
	char *cdrstr;
	if (cdr->type == OBJ_CONS) {
	    cdrstr = listStr((Cons *) cdr);
	    result = newstr("%s %s", carstr, cdrstr);
	}
	else {
	    cdrstr = objectSexp(cdr);
	    result = newstr("%s . %s", carstr, cdrstr);
	}
	skfree(cdrstr);
	skfree(carstr);
	return result;
    }
    else {
	return carstr;
    }
}

// Put the () around the contents returned by listStr.
char *
consStr(Cons *cons)
{
    char *str = listStr(cons);
    char *result;
    result = newstr("(%s)", str);
    skfree(str);
    return result;
}

Object *
setCar(Cons *cons, Object *obj)
{
    assert(cons && (cons->type == OBJ_CONS), "setCar arg1 is not a cons cell");
    return cons->car = obj;
}

Object *
setCdr(Cons *cons, Object *obj)
{
    assert(cons->type == OBJ_CONS, "setCdr arg1 is not a cons cell");
    return cons->cdr = obj;
}

Object *
getCar(Cons *cons)
{
    assert(cons->type == OBJ_CONS, "getCar arg is not a cons cell");
    return cons->car;
}

Object *
getCdr(Cons *cons)
{
    assert(cons->type == OBJ_CONS, "getCdr arg is not a cons cell");
    return cons->cdr;
}

// Fire out the length of a list, being kinda forgiving about what
// cosntitutes a list (ie do not abort if it does not look right).
int 
consLen(Cons *cons)
{
    int len = 0;
    if (cons) {
	assert(cons->type == OBJ_CONS, "consLen arg is not a cons cell");
    }

    while (cons && (cons->type == OBJ_CONS)) {
	len++;
	cons = (Cons *) cons->cdr;
    }
    return len;
}

/* Predicate to check whther a cons contains an alist.  */
boolean
consIsAlist(Cons *cons)
{
    Object *car;

    if (cons) {
	assert(cons->type == OBJ_CONS, "consIsAlist arg is not a cons cell");
    }

    while (cons) {
	if (cons->type != OBJ_CONS) {
	    return FALSE;
	}
	car = cons->car;
	if (!(car && (car->type == OBJ_CONS))) {
	    return FALSE;
	}
	cons = (Cons *) cons->cdr;
     }
    return TRUE;
}

// Compare 2 lists using strcmp style semantics.
int
consCmp(Cons *cons1, Cons *cons2)
{
    int result;
    assert(cons1 && cons1->type == OBJ_CONS, "consCmp: cons1 is not cons");
    assert(cons2, "consCmp: cons2 is NULL");

    if (cons2->type != OBJ_CONS) {
	objectCmpFail((Object *) cons1, (Object *) cons2);
    }

    if ((result = objectCmp(cons1->car, cons2->car)) == 0) {
	result = objectCmp(cons1->cdr, cons2->cdr);
    } 
    return result;
}

/* Get the contents of alist that match key.  */
Object *
alistGet(Cons *alist, Object *key)
{
    Cons *head = alist;
    Cons *entry;
    assert(consIsAlist(alist), "alistGet arg1 is not an alist");

    while (head) {
	entry = (Cons *) head->car;
	if (objectCmp(entry->car, key) == 0) {
	    return entry->cdr;
	}
	head = (Cons *) head->cdr;
    }
    return NULL;
}

Cons *
alistExtract(Cons **p_alist, Object *key)
{
    Cons **p_this = p_alist;
    Cons *this;
    Cons *entry;
    assert(consIsAlist(*p_alist), "alistGet arg1 is not an alist");

    while (this = *p_this) {
	entry = (Cons *) this->car;
	if (objectCmp(entry->car, key) == 0) {
	    /* This entry matches key. */
	    *p_this = (Cons *) this->cdr;
	    objectFree((Object *) this, FALSE);
	    return entry;
	}
	p_this = (Cons **) &(this->cdr);
    }
    return NULL;
}


Object *
consNth(Cons *list, int n)
{
    list = (Cons *) dereference((Object *) list);

    while ((n > 0) && list) {
	assert(isCons(list), "consNth: list is not cons");
	list = (Cons *) list->cdr;
	n--;
    }
    return list? list->car: NULL;
}

Object *
consGet(Cons *list, Object *key)
{
    Object *elem;
    Object *result;
    Int4 *newkey;
    if (key->type == OBJ_INT4) {
	return consNth(list, ((Int4 *) key)->value);
    }
    else if (key->type == OBJ_STRING) {
	newkey = stringToInt4((String *) key);
	result = consNth(list, newkey->value);
	objectFree((Object *) newkey, TRUE);
	return result;
    }
    else if (key->type == OBJ_REGEXP) {
	while (list) {
	    elem = list->car;
	    if (elem->type != OBJ_STRING) {
		RAISE(LIST_ERROR, 
		      newstr("consGet: invalid element type (%s) "
			     "for regexp search of list", objTypeName(key)));
	    }
	    if (regexpMatch((Regexp *) key, (String *) elem)) {
		return elem;
	    }
	    list = (Cons *) list->cdr;
	}
	return NULL;
    }
    else {
	RAISE(LIST_ERROR, newstr("consGet: invalid type (%s) for key",
				 objTypeName(key)));
    }
}

/* Iterator function for lists */
Object *
consNext(Cons *list, Object **p_placeholder)
{
    ObjReference *placeholder = (ObjReference *) *p_placeholder;
    Cons *position;
    Object *result;
    if (!placeholder) {
	/* This is the first iteration so set up the placeholder which
	 * will keep track of where we are.  When this is set to null, 
	 * we will have finished iterating over the list. */
	placeholder = objRefNew((Object *) list);
	*p_placeholder = (Object *) placeholder;
    }

    position = (Cons *) placeholder->obj;

    if (position) {
	result = position->car;
	placeholder->obj = position->cdr;
	return (Object *) objRefNew(result);
    }
    objectFree(*p_placeholder, TRUE);
    *p_placeholder = NULL;
    return NULL;
}

Cons *
consAppend(Cons *list, Object *item)
{
    Cons *prev = list;
    Cons *next = list;
    Cons *new = consNew(item, NULL);
    while (next) {
	prev = next;
	next = (Cons *) prev->cdr;
    }
    if (prev) {
	prev->cdr = (Object *) new;
	return list;
    }
    else {
	return new;
    }
}

/* Append a list or an element to a list.  Either parameter may be
 * NULL. */
Cons *
consConcat(Cons *list, Object *list2)
{
    Cons *prev = list;
    Cons *next;
    if (list2 && (list2->type != OBJ_CONS)) {
	list2 = (Object *) consNew(list2, NULL);
    }

    if (prev) {
	while (next = (Cons *) prev->cdr) {
	    prev = next;
	}
	prev->cdr = list2;
	return list;
    }
    return (Cons *) list2;
}

boolean
consIn(Cons *cons, Object *obj)
{
    Cons *next = cons;
    while (next) {
	if (obj == dereference(next->car)) {
	    return TRUE;
	}
	next = (Cons *) next->cdr;
    }
    return FALSE;
}

boolean 
checkCons(Cons *cons, void *chunk)
{
    boolean found;
    if (found = checkObj(cons->car, chunk)) {
	printSexp(stderr, "...within car of ", (Object *) cons);
    }
    if (checkObj(cons->cdr, chunk)) {
 	printSexp(stderr, "...within cdr of ", (Object *) cons);
	found = TRUE;
    }
    return checkChunk(cons, chunk) || found;
}

Cons *
consRemove(Cons *cons, Cons *remove)
{
    Cons *start = cons;
    Cons *cur = cons;
    Cons *tmp;
    Cons *prev = NULL;
    while (cur) {
	if (cur == remove) {
	    tmp = cur;
	    cur = (Cons *) cur->cdr;
	    if (prev) {
		/* This is not the first element - unlink from prev */
		prev->cdr = (Object *) cur;
	    }
	    else {
		/* This is the first element - skip it*/
		start = cur;
	    }
	    tmp->cdr = NULL;
	    objectFree((Object *) tmp, TRUE);
	    return start;
	}
	else {
	    prev = cur;
	    cur = (Cons *) cur->cdr;
	}
    }
    return start;
}

/* Create a new list as a copy of the old, with each element in the list
 * represented by an ObjReference.  This is so that the new list may be
 * unconditionally freed in safety. */
Cons *
consCopy(Cons *list)
{
    Cons *from = list;
    Cons *to;
    Cons *new;
    Cons *result = NULL;

    while (from) {
	new = consNew((Object *) objRefNew((Object *) from->car), NULL);
	if (result) {
	    to->cdr = (Object *) new;
	}
	else {
	    result = new;
	}

	to = new;
	from = (Cons *) from->cdr;
    }
    return result;
}
