/**
 * @file   regexp.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * 
 *
 */

#include "skit_lib.h"
#include <stdio.h>
#include <string.h>
 
#define MAX_MATCHES 10

String *
newSubstr(char *in, int len)
{
    char *str = skalloc(len + 1);
    strncpy(str, in, len);
    str[len] = '\0';
    return stringNewByRef(str);
}

static Vector *
splitReplacementStr(String *replacement)
{
    Vector *result = vectorNew(MAX_MATCHES * 4);
    char *str = replacement->value;
    int start = 0;
    int i;
    char ch;
    int len = strlen(str);
    boolean escaped = FALSE;
    char *part;
    String *newstr;
    Int4 *newint;

    for (i = 0; i < len; i++) {
	ch = str[i];
	if (escaped) {
	    if ((ch >= '0') && (ch <= '9')) {
		/* This is a replacement token.  Record the string
		 * preceding this token, then the token and then carry
		 * on. */
		part = skalloc(i - start);
		strncpy(part, str + start, (i - start) - 1);
		part[(i - start) - 1] = '\0';
		newstr = stringNewByRef(part);
		vectorPush(result, (Object *) newstr);
		newint = int4New(ch - '0');
		vectorPush(result, (Object *) newint);
		start = i + 1;
	    }
	    escaped = FALSE;
	}
	else {
	    escaped = (ch == '\\');
	}
    }
    if (start <= i) {
	newstr = stringNew(str + start);
	vectorPush(result, (Object *) newstr);
    }
    return result;
}

static int 
rreplace(char *buf, int size, regex_t *re, char *rp)
{
    char *pos;
    int sub, so, n;
    regmatch_t pmatch [10]; /* regoff_t is int so size is int */
 
    if (regexec (re, buf, 10, pmatch, 0)) return 0;

    for (pos = rp; *pos; pos++)
	if (*pos == '\\' && *(pos + 1) > '0' && *(pos + 1) <= '9') {
	    so = pmatch [*(pos + 1) - 48].rm_so;
	    n = pmatch [*(pos + 1) - 48].rm_eo - so;
	    if (so < 0 || strlen (rp) + n - 1 > size) return 1;
	    memmove (pos + n, pos + 2, strlen (pos) - 1);
	    memmove (pos, buf + so, n);
	    pos = pos + n - 2;
	}

    sub = pmatch [1].rm_so; /* no repeated replace when sub >= 0 */

    for (pos = buf; !regexec (re, pos, 1, pmatch, 0); ) {
	n = pmatch [0].rm_eo - pmatch [0].rm_so;
	pos += pmatch [0].rm_so;
	if (strlen (buf) - n + strlen (rp) + 1 > size) return 1;
	memmove (pos + strlen (rp), pos + n, strlen (pos) - n + 1);
	memmove (pos, rp, strlen (rp));
	pos += strlen (rp);
	if (sub >= 0) break;
    }
    
    return 0;
}


String *
replacementStr(char *buf, regmatch_t *match, Vector *replacements)
{
    long len = 0;
    int i;
    int matchidx;
    char *result;
    char *pos;
    Object *elem;

    /* Start by figuring out how long the replacement string is */
    for (i = 0; i < replacements->elems; i++) {
	elem = replacements->contents->vector[i];
	if (elem->type == OBJ_STRING) {
	    len += strlen(((String *) elem)->value);
	}
	else if (elem->type == OBJ_INT4) {
	    matchidx = ((Int4 *) elem)->value;
	    len += match[matchidx].rm_eo - match[matchidx].rm_so;
	}
	// TODO: ELSE RAISE AN EXCEPTION
    }
    result = skalloc(len + 1);
    pos = result;
    for (i = 0; i < replacements->elems; i++) {
	elem = replacements->contents->vector[i];
	if (elem->type == OBJ_STRING) {
	    len = strlen(((String *) elem)->value);
	    strncpy(pos, ((String *) elem)->value, len);
	}
	else if (elem->type == OBJ_INT4) {
	    matchidx = ((Int4 *) elem)->value;
	    len = match[matchidx].rm_eo - match[matchidx].rm_so;
	    strncpy(pos, buf + match[matchidx].rm_so, len);
	}
	pos += len;
    }
    *pos = '\0';
    return stringNewByRef(result);
}

String *
regexpReplace(String *src, Regexp *regexp, String *replacement)
{
    regex_t *re = &(regexp->regexp);
    Vector *rvec;
    Vector *results;
    String *part;
    String *final;
    regmatch_t match[MAX_MATCHES]; 
    char *buf = src->value;
    regoff_t start;
    regoff_t end;
    int     flags = 0;

    rvec = splitReplacementStr(replacement);
    results = vectorNew(1000);

    while (!regexec(re, buf, MAX_MATCHES, match, flags)) {
	start = match[0].rm_so;
	end = match[0].rm_eo;
	part = newSubstr(buf, start);
	vectorPush(results, (Object *) part);
	part = replacementStr(buf, match, rvec);
	vectorPush(results, (Object *) part);
	if (!end) {
	    break;
	}
	buf = buf + end;
	flags |= REG_NOTBOL;
    }
    part = stringNew(buf);
    vectorPush(results, (Object *) part);
    final = vectorConcat(results);

    objectFree((Object *) rvec, TRUE);
    objectFree((Object *) results, TRUE);
    return final;
}

/* Return a string containing only the replaced part of the regex */
String *
regexpReplaceOnly(String *src, Regexp *regexp, String *replacement)
{
    regex_t *re = &(regexp->regexp);
    Vector *rvec;
    Vector *results;
    String *part;
    String *final;
    regmatch_t match[MAX_MATCHES]; 
    char *buf = src->value;
    regoff_t end;
    int     flags = 0;

    rvec = splitReplacementStr(replacement);
    results = vectorNew(1000);

    while (!regexec(re, buf, MAX_MATCHES, match, flags)) {
	part = replacementStr(buf, match, rvec);
	vectorPush(results, (Object *) part);
	end = match[0].rm_eo;
	if (!end) {
	    break;
	}
	buf = buf + end;
	flags |= REG_NOTBOL;
    }
    final = vectorConcat(results);

    objectFree((Object *) rvec, TRUE);
    objectFree((Object *) results, TRUE);
    return final;
}

