/**
 * @file   string.c
 * \code
 *     Copyright (c) 2008 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides functions for manipulating String objects
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../skit_lib.h"
#include "../exceptions.h"
#include <regex.h>

String *
stringNewByRef(char *value)
{
    String *obj = (String *) skalloc(sizeof(String));
    obj->type = OBJ_STRING;
    obj->value = value;
    return obj;
}

String *
stringNew(const char *value)
{
    return stringNewByRef(newstr(value));
}

String *
stringDup(String *src)
{
    assert(src->type == OBJ_STRING,
	   "stringDup: src must be a String");

    return stringNew(src->value);
}

void
stringFree(String *obj, boolean free_contents)
{
    assert(obj->type == OBJ_STRING,
	   "stringFree: obj must be a String");
    if (free_contents) {
	skfree((void *) obj->value);
    }
    skfree((void *) obj);
}

String *
stringLower(String *str)
{
    int i;
    String *result;
    assert(str && (str->type == OBJ_STRING),
	   "stringLower: obj must be a String");
    result = stringDup(str);
    i = strlen(result->value) - 1;
    for (; i >= 0; i--) {
	result->value[i] = tolower(result->value[i]);
    }
    return result;
}

void
stringLowerOld(String *str)
{
    int i;
    assert(str->type == OBJ_STRING,
	   "stringLower: obj must be a String");
    i = strlen(str->value) - 1;
    for (; i >= 0; i--) {
	str->value[i] = tolower(str->value[i]);
    }
}

// Compare two String objects using the qsort/bsearch/hash sort 
// comparison interface
int 
stringCmp4Hash(const void *item1, const void *item2)
{
    Object *obj1 = *((Object **) item1);
    Object *obj2 = *((Object **) item2);
    assert(obj1 && obj1->type == OBJ_STRING,
	   newstr("stringCmp: obj1 is not a string (%d)", obj1->type));
    assert(obj2 && obj2->type == OBJ_STRING,
	   newstr("stringCmp: obj2 is not a string (%d)", obj2->type));
    return strcmp(((String *) obj1)->value, ((String *) obj2)->value);
}

// Compare two string objects for objectCmp purposes.
int 
stringCmp(String *str1, String *str2)
{
    assert(str1 && str1->type == OBJ_STRING, "stringCmp: str1 is not String");
    assert(str2, "stringCmp: str2 is NULL");

    if (str2->type == OBJ_INT4) {
	// Do integer comparison.  Note that failed integer comparison
	// automatically causes a string comparison to be done with
	// 2 real string objects.
	return int4Cmp((Int4 *) str2, (Int4 *) str1) * -1;
    }
    else if (str2->type == OBJ_STRING) {
	return strcmp(str1->value, str2->value);
    }
    else {
	char *objstr = objectSexp((Object *) str2);
	int  result = strcmp(str1->value, objstr);
	skfree(objstr);
	return result;
    }
}

boolean
stringMatch(String *str, char *expr)
{
    regex_t regex;
    int errcode;
    boolean result;
    assert(str && str->type == OBJ_STRING,
	   "stringMatch: str must be a String");

    if (errcode = regcomp(&regex, expr, REG_EXTENDED)) {
	// Something went wrong with the regexp compilation
	char errmsg[200];
	(void) regerror(errcode, &regex, errmsg, 199);
	skitFail(newstr("stringMatch: regexp compilation failure: %s", errmsg));
    }

    result = regexec(&regex, str->value, 0, NULL, 0) == 0;
    regfree(&regex);
    return result;
}

String *
nextTok(char *instr, char *separators, char **p_placeholder)
{
    size_t len;
    char *result;
    if (!(*p_placeholder)) {
	*p_placeholder = instr;
    }
    if (!(**p_placeholder)) {
	return NULL;
    }
    len = strcspn(*p_placeholder, separators);
    result = skalloc(len + 1);
    strncpy(result, *p_placeholder, len);
    result[len] = '\0';
    *p_placeholder += len;
    if ((**p_placeholder) != '\0') {
	/* Not looking at the end of the string */
	(*p_placeholder)++;
    }
    return stringNewByRef(result);
}

Cons *
stringSplit(String *instr, String *split)
{
    Cons *cons;
    Cons *prev = NULL;
    Cons *head;
    char *mycopy;
    char *placeholder = NULL;
    String *entry;

    assert(instr && instr->type == OBJ_STRING,
	   "stringSplit: instr must be a String");
    assert(split&& split->type == OBJ_STRING,
	   "stringSplit: split must be a String");

    mycopy = newstr(instr->value);

    while (entry = nextTok(mycopy, split->value, &placeholder)) {
	cons = consNew((Object *) entry, NULL);
	if (prev) {
	    prev->cdr = (Object *) cons;
	    prev = cons;
	}
	else {
	    head = prev = cons;
	}
    }
    skfree(mycopy);
    return head;
}

Int4 *
stringToInt4(String *str)
{
    assert(str && str->type == OBJ_STRING,
	   newstr("stringToInt4: str must be a String %d", str->type));

    if (stringMatch(str, "^[ ]*[+-]?[0-9][0-9]*[ ]*$")) {
	return int4New(atoi(str->value));
    }
    RAISE(TYPE_MISMATCH, newstr("Cannot convert %s to integer", str->value));
}

String *
stringNext(String *str, Object **p_placeholder)
{
    Int4 *placeholder = *((Int4 **) p_placeholder);
    int len = strlen(str->value);
    char *result;

    if (!placeholder) {
	/* This is the first iteration. */
	placeholder = int4New(0);
	*p_placeholder = (Object *) placeholder;
    }
    if (placeholder->value < len) {
	result = newstr("%c", str->value[placeholder->value]);
	placeholder->value++;
	return stringNewByRef(result);
    }
    /* OK, we have returned the last character. */
    objectFree(*p_placeholder, TRUE);
    *p_placeholder = NULL;
    return NULL;
}
