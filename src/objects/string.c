/**
 * @file   string.c
 * \code
 *     Copyright (c) 2009 - 2014 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating String objects
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../skit_lib.h"
#include "../exceptions.h"
#include <regex.h>

void
appendStr(String *str1, String *str2)
{
    char *str1val = str1->value;
    str1->value = newstr("%s%s", str1->value, str2->value);
    skfree(str1val);
}

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
    return stringNewByRef(newstr("%s", value));
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
stringLowerInPlace(String *str)
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
	RAISE(REGEXP_ERROR,
	      newstr("stringMatch: regexp compilation failure: %s", errmsg));
    }

    result = regexec(&regex, str->value, 0, NULL, 0) == 0;
    regfree(&regex);
    return result;
}

static String *
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

/* Get the last separator used by nextTok, returning it as a String. */
static String *
separatorTok(char *placeholder)
{
    char *str = skalloc(2);
    String *result;
    *str = *(placeholder - 1);
    *(str + 1) = '\0';
    result = stringNewByRef(str);
    return result;
}

/* The type, if any of quote used in a string. */
static char
quoteType(String *str)
{
    char *ch = str->value;
    while (*ch) {
	if (*ch == '\'' || *ch == '"') {
	    break;
	}
	ch++;
    }
    return *ch;
}

/* True if the string contains an even number of the given type of
 * quote. */
static boolean
quotesMatched(String *str, char quote)
{
    char result = TRUE;
    char *ch = str->value;
    while (*ch) {
	if (*ch == quote) {
	    result = !result;
	}
	ch++;
    }
    
    return result;
}

static void
rejoinStrings(String *prev, String *separator, String *next)
{
    appendStr(prev, separator);
    appendStr(prev, next);
    objectFree((Object *) separator, TRUE);
    objectFree((Object *) next, TRUE);
}

Cons *
stringSplit(String *instr, String *split, boolean match_quotes)
{
    Cons *cons;
    Cons *prev = NULL;
    Cons *head = NULL;
    char *mycopy;
    char *placeholder = NULL;
    String *separator;
    String *entry;
    char quote = '\0';
    boolean quotes_matched = TRUE;
    boolean append_to_prev = FALSE;

    assert(instr && instr->type == OBJ_STRING,
	   "stringSplit: instr must be a String");
    assert(split&& split->type == OBJ_STRING,
	   "stringSplit: split must be a String");

    mycopy = newstr("%s", instr->value);

    while (entry = nextTok(mycopy, split->value, &placeholder)) {
	if (match_quotes) {
	    if (!quote) {
		quote = quoteType(entry);
	    }
	    if (quote) {
		if (!quotesMatched(entry, quote)) {
		    quotes_matched = !quotes_matched;
		}
	    }
	}
	if (append_to_prev) {
	    rejoinStrings((String *) prev->car, separator, entry);
	}
	else {
	    cons = consNew((Object *) entry, NULL);
	    if (prev) {
		prev->cdr = (Object *) cons;
		prev = cons;
	    }
	    else {
		head = prev = cons;
	    }
	}
	if (quote) {
	    append_to_prev = !quotes_matched;
	    if (append_to_prev) {
		separator = separatorTok(placeholder);
	    }
	}
    }
    /* If the final string fails to be matched, just append it anyway. */
    if (append_to_prev) {
	rejoinStrings((String *) prev->car, separator, entry);
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
    return NULL;
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

boolean 
checkString(String *str, void *chunk)
{
    boolean found;
    if (found = checkChunk(str->value, chunk)) {
	fprintf(stderr, "...within value of \"%s\"", str->value);
    }
    if (checkChunk(str, chunk)) {
 	fprintf(stderr, "...within \"%s\"", str->value);
	found = TRUE;
    }
    return found;
}
