/**
 * @file   vector.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating vectors cells.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../skit_lib.h"
#include "../exceptions.h"



// Return a suggested size for a Vector based on the number of elems 
// it is expected to store.  Based on a WAG.
static int
entriesForVector(int elems)
{
    if (elems < 40) return 64;
    if (elems < 200) return 256;
    if (elems < 4000) return 4096;
    return elems + 1000;
}

static void
vectorExpand(Vector *vector)
{
    int newentries = entriesForVector(vector->size + 64);
    size_t newsize = (newentries * sizeof(Object *)) + sizeof(Varray);
    Varray *newvarray = (Varray *) skrealloc(vector->contents, newsize);

    if (newvarray) {
	vector->contents = newvarray;
	vector->size = newentries;
    }
    else {
	RAISE(GENERAL_ERROR, 
	      newstr("vectorPush: vector space exhausted"));
    }
}

Vector *
vectorNew(int elems)
{
    int entries = entriesForVector(elems);
    int varraysize = sizeof(Varray) + (sizeof(Object *) * entries);
    Vector *vector = (Vector *) skalloc(sizeof(Vector));
    Varray *varray = (Varray *) skalloc(varraysize);
    varray->type = OBJ_VARRAY;
    vector->type = OBJ_VECTOR;
    vector->elems = 0;
    vector->size = entries;
    vector->contents = varray;
    return vector;
}

Object *
vectorPush(Vector *vector, Object *obj)
{
    if (vector->elems >= vector->size) {
	vectorExpand(vector);
    }
    
    vector->contents->vector[vector->elems] = obj;
    vector->elems++;

    return obj;
}

Object *
vectorPop(Vector *vector)
{
    Object *obj;
    if (vector->elems) {
	return vector->contents->vector[--vector->elems];
    }
    return NULL;
}

// Create a vector from a Cons-cell list, moving the objects from the
// list into the vector, and destroying the list.
Vector *
toVector(Cons *cons)
{
    int elems;
    Vector *vector;
    Object *obj;
    elems = consLen(cons);
    vector = vectorNew(elems);
    while (cons) {
    	obj = consPop(&cons);
    	(void) vectorPush(vector, obj);
    }
    return vector;
}

// Return a string representation of the vector
char *
vectorStr(Vector *vector)
{
    char *objstr;
    char *discard;
    char *workstr = newstr("");
    int i;
    for (i = 0; i < vector->elems; i++) {
	objstr = objectSexp(vector->contents->vector[i]);
	discard = workstr;
	workstr = newstr("%s %s", workstr, objstr);
	skfree(objstr);
	skfree(discard);
    }
    discard = workstr;
    if (workstr[0]) {
	workstr++;  // Skip leading space if present
    }

    workstr = newstr("[%s]", workstr);
    skfree(discard);
    return workstr;
}

void 
vectorFree(Vector *vector, boolean free_contents)
{
    if (free_contents) {
	int i;
	Object *obj;
	for (i = 0; i < vector->elems; i++) {
	    if (obj = vector->contents->vector[i]) {
		objectFree(obj, free_contents);
	    }
	}
    }
    skfree(vector->contents);
    skfree(vector);
}

// Sort the contents of vector using stringCmp
void
vectorStringSort(Vector *vector)
{
    qsort((void *) vector->contents->vector,
	  vector->elems, sizeof(Object *),
	  stringCmp4Hash);
}

String *
vectorConcat(Vector *vector)
{
    long len = 0;
    int i;
    Object *elem;
    char *result;
    char *pos;

    for (i = 0; i < vector->elems; i++) {
	elem = vector->contents->vector[i];
	if (elem->type == OBJ_STRING) {
	    len += strlen(((String *) elem)->value);
	}
    }
    result = skalloc(len + 1);
    pos = result;
    for (i = 0; i < vector->elems; i++) {
	elem = vector->contents->vector[i];
	if (elem->type == OBJ_STRING) {
	    len = strlen(((String *) elem)->value);
	    strncpy(pos, ((String *) elem)->value, len);
	    pos += len;
	}
    }
    *pos = '\0';
    return stringNewByRef(result);
}

static Object *
vectorNth(Vector *vec, int n)
{
    if (n > vec->elems) {
	return NULL;
    }
    return vec->contents->vector[n];
}

Object *
vectorGet(Vector *vec, Object *key)
{
    Object *result;
    Int4 *newkey;
    if (key->type == OBJ_INT4) {
	return vectorNth(vec, ((Int4 *) key)->value);
    }
    else if (key->type == OBJ_STRING) {
	newkey = stringToInt4((String *) key);
	result = vectorNth(vec, newkey->value);
	objectFree((Object *) newkey, TRUE);
	return result;
    }
    else {
	RAISE(LIST_ERROR, newstr("vectorGet: invalid type (%s) for key",
				 objTypeName(key)));
    }
}

Object *
vectorRemove(Vector *vec, int index)
{
    Object *result;
    assert(vec->type == OBJ_VECTOR, 
	   "vectorRemove arg is not a vector");
    if (index >= vec->elems) {
	return NULL;
    }
    result = vec->contents->vector[index];
    vec->elems--;
    while (index < vec->elems) {
	vec->contents->vector[index] = vec->contents->vector[++index];
    }
    
    return result;
}

typedef int (basicCmpFn)(const void *, const void *);

void
vectorSort(Vector *vec, ComparatorFn *fn)
{
    qsort((void *) vec->contents->vector,
	  vec->elems, sizeof(Object *),
	  (basicCmpFn *) fn);
}

Object *
vectorDel(Vector *vec, Object *obj)
{
    Object *deref = dereference(obj);
    Object *this;
    int i;
    for (i = 0; i < vec->elems; i++) {
	this = dereference(vec->contents->vector[i]);
	if (this == obj) {
	    return vectorRemove(vec, i);
	}
    }
    return NULL;
}
