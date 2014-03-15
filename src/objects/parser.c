/**
 * @file   parser.c
 * \code
 *     Copyright (c) 2009, 2010, 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for parsing and tokenising string expressions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "../skit_lib.h"

static char *
skipWhiteSpace(char *str)
{
    while (isspace(*str)) {
	str++;
    }
    return str;
}

static boolean
isSpecialToken(char ch)
{
    char str[2] = {ch, '\0'};
    char *x = strpbrk(str, "()[]<>\"'/");
    return (x != NULL);
}

static boolean
isQuote(char ch)
{
    char str[2] = {ch, '\0'};
    char *x = strpbrk(str, "\"'/");
    return (x != NULL);
}

static boolean
isStrToken(char ch)
{
    char str[2] = {ch, '\0'};
    char *x = strpbrk(str, "\\\"'/");
    return (x != NULL);
}

static char *
skipToSeparator(char *str)
{
    while ((*str != '\0') && !isSpecialToken(*str) && !isspace(*str)) {
	str++;
    }
    return str;
}

static char *
skipToStrToken(char *str)
{
    while ((*str != '\0') && !isStrToken(*str)) {
	str++;
    }
    return str;
}

static char *
skipPastString(char *str)
{
    char quote = *str++;
    boolean done = FALSE;
    boolean handle_escapes = ((quote == '"') || (quote == '/'));
    while (!done) {
	str = skipToStrToken(str);
	if ((*str == '\\') && handle_escapes) {
	    str += 2;	// Step over the escaped special character
	}
	else if (*str == '\0') {
	    return str;
	}
	else {
	    done = (*str == quote);
	    str++;
	}
    }
    return str;
}

static char *
skipPastToken(char *str)
{
    char *start = str;
    str = skipToSeparator(str);
    if (str == start) {
	// We are looking at a single-character token or the end of the
	// string.
	if (*str != '\0') {
	    // If this token is for the start of a quoted string, 
	    // handle it as such
	    if (isQuote(*str)) {
		str = skipPastString(str);
	    }
	    else {
		// Skip past the special token
		str++;
	    }
	}
    }
    return str;
}

/* Extract the next token from instr, allowing re-entrancy and returning
   a reference to a modified version of the source string.  This is
   ugly but, I hope, fast. */
char *
sexpTok(TokenStr *str)
{
    char *result;
    if (str->pos) {
	*(str->pos) = str->saved;
    }
    else {
	str->pos = str->instr;
    }
    result = skipWhiteSpace(str->pos);

    str->pos = skipPastToken(result);
    str->saved = *(str->pos);
    *(str->pos) = '\0';
    return result;
}

TokenType
tokenType(char *tok)
{
    char start = *tok;
    if (isSpecialToken(start)) {
	int len;
	TokenType possible = TOKEN_QUOTE_STR;
	switch (start) {
	case '(': return TOKEN_OPEN_PAREN;
	case ')': return TOKEN_CLOSE_PAREN;
	case '[': return TOKEN_OPEN_BRACKET;
	case ']': return TOKEN_CLOSE_BRACKET;
	case '<': return TOKEN_OPEN_ANGLE;
	case '>': return TOKEN_CLOSE_ANGLE;
	case '/':
	    possible = TOKEN_REGEXP;
	case '"': 
	    if (possible == TOKEN_QUOTE_STR) {
		possible = TOKEN_DBLQUOTE_STR;  /* Note no break here */
	    }
	case '\'': 
	    len = strlen(tok);
	    if ((len > 1) && (tok[len-1] == start)) {
		return possible;
	    }
	    return TOKEN_BROKEN_STR;
	}
    }
    else {
	// Must be a dot, a symbol or a number.
	if (isdigit(start)) {
	    tok++;
	    while (*tok != '\0') {
		if (!isdigit(*tok++)) {
		    return TOKEN_SYMBOL;
		}
	    }
	    return TOKEN_NUMBER;
	}
	if ((start == '.') && (*(tok+1) == '\0')) {
	    return TOKEN_DOT;
	}
    }
    return TOKEN_SYMBOL;
}

char *
tokenTypeName(TokenType tt)
{
    switch (tt) {
    case TOKEN_OPEN_PAREN:    return "TOKEN_OPEN_PAREN";
    case TOKEN_CLOSE_PAREN:   return "TOKEN_CLOSE_PAREN";
    case TOKEN_OPEN_BRACKET:  return "TOKEN_OPEN_BRACKET";
    case TOKEN_CLOSE_BRACKET: return "TOKEN_CLOSE_BRACKET";
    case TOKEN_SYMBOL:	      return "TOKEN_SYMBOL";
    case TOKEN_NUMBER:	      return "TOKEN_NUMBER";
    case TOKEN_DOT:	      return "TOKEN_DOT";
    case TOKEN_DBLQUOTE_STR:  return "TOKEN_DBLQUOTE_STR";
    case TOKEN_QUOTE_STR:     return "TOKEN_QUOTE_STR";
    case TOKEN_REGEXP:        return "TOKEN_REGEXP";
    case TOKEN_BROKEN_STR:    return "TOKEN_BROKEN_STR";
    case TOKEN_OPEN_ANGLE:    return "TOKEN_OPEN_ANGLE";
    case TOKEN_CLOSE_ANGLE:   return "TOKEN_CLOSE_ANGLE";
    }
    return "UNKONWN_TOKEN";
}

char *
strEval(char *instr)
{
    int len = strlen(instr);
    char *out = skalloc(len - 1);
    strncpy(out, instr+1, len - 2);
    out[len - 2] = '\0';
    return out;
}
