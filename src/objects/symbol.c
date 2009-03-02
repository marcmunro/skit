/**
 * @file   symbol.c
 * \code
 *     Copyright (c) 2008 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides functions for dealing with symbols.  There is a single
 * namespace for symbols, managed by a hash.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../skit_lib.h"
#include "../exceptions.h"

static Hash *symbols = NULL;
static Cons *symbol_scope = NULL;

static void
dropScopeForSymbol(Symbol *sym)
{
    Cons *prev = (Cons *) consPop(&(sym->scope));
    Cons *scope;
    if (!prev) {
	RAISE(LIST_ERROR, 
	      newstr("Symbol %s has no past scope", sym->name));
    }
    // TODO: Assert that prev is a cons
    if (prev->cdr != (Object *) symbol_scope) {
	RAISE(LIST_ERROR, 
	      newstr("Symbol %s is not defined in this scope", sym->name));
    }
    objectFree(sym->value, TRUE);
    sym->value = prev->car;
    objectFree((Object *) prev, FALSE);
}

void
setScopeForSymbol(Symbol *sym)
{
    Cons *prev;
    Cons *cur_scope;

    if (!symbol_scope) {
	/* There is nothing to do if we have not defined a symbol scope
	 * yet. */
	return;
    }

    if (sym->scope) {
	/* Check whether this symbol has already been defined in the
	 * current scope.  If so, there is nothing to be done. */
	cur_scope = (Cons *) sym->scope->car;
	if (cur_scope->cdr == (Object *) symbol_scope) {
	    return;
	}
    }
    prev = consNew(sym->value, (Object *) symbol_scope);
    (void) consPush(&(sym->scope), (Object *) prev);
    sym->value = NULL;

    if (symbol_scope) {
	consPush((Cons **) &(symbol_scope->car), (Object *) sym);
    }
}

void
newSymbolScope()
{
    (void) consPush(&symbol_scope, NULL);
}

void
dropSymbolScope()
{
    Cons *symbol_list;
    Symbol *sym;

    symbol_list = symbol_scope? (Cons *) symbol_scope->car: NULL;

    while (symbol_list) {
	// TODO: Assert that symbol_list is a cons
	sym = (Symbol *) consPop(&symbol_list);
	// TODO: Assert that symbol is a symbol
	dropScopeForSymbol(sym);
    }
    (void) consPop(&symbol_scope);
}


Hash *
symbolTable()
{
    if (symbols) {
	return symbols;
    }
    return symbols = hashNew(TRUE);
}


void
freeSymbolTable()
{
    Hash *hash = symbols;
    symbols = NULL;
    if (hash) {
	hashFree(hash, TRUE);
    }
}

// Create new symbol and add it to the symbol table if it does not
// already exist.
Symbol *
symbolNew(char *name)
{
    Symbol *sym;

    if (!(sym = symbolGet(name))) {
	Hash *symbols = symbolTable();
	String *hashkey = stringNew(name);
	sym = (Symbol *) skalloc(sizeof(Symbol));
	sym->type = OBJ_SYMBOL;
	sym->name = newstr(name);
	sym->fn = NULL;
	sym->value = NULL;
	sym->scope = NULL;
	hashAdd(symbols, (Object *) hashkey, (Object *) sym);
	setScopeForSymbol(sym);
    }
    return sym;
}

void
symbolSet(char *name, Object *value)
{
    Hash *symbols = symbolTable();
    String *hashkey = stringNew(name);
    Symbol *sym = (Symbol *) hashGet(symbols, (Object *) hashkey);

    if (!sym) {
	RAISE(GENERAL_ERROR,
	      newstr("Error in symbolSet - no such symbol: %s", name));
    }
    if (sym->value && (sym->value != value)) {
	objectFree(sym->value, TRUE);
    }
    
    stringFree(hashkey, TRUE);
    sym->value = value;
}

void
symbolSetRoot(char *name, Object *value)
{
    Hash *symbols = symbolTable();
    String *hashkey = stringNew(name);
    Symbol *sym = (Symbol *) hashGet(symbols, (Object *) hashkey);
    Object **valptr;
    if (sym->scope) {
	Cons *scope_node = sym->scope;
	Cons *value_node;
	while (scope_node->cdr) {
	    scope_node = (Cons *) scope_node->cdr;
	}
	value_node = (Cons *) scope_node->car;
	valptr = &(value_node->car);
    }
    else {
	valptr = &(sym->value);
    }

    if (*valptr && (*valptr != value)) {
	objectFree(*valptr, TRUE);
    }
    
    stringFree(hashkey, TRUE);
    *valptr = value;
}

Symbol *
symbolGet(char *name)
{
    Hash *symbols = symbolTable();
    String *hashkey = stringNew(name);
    Symbol *sym = (Symbol *) hashGet(symbols, (Object *) hashkey);
    
    stringFree(hashkey, TRUE);
    return sym;
}


Object *
symbolGetValue(char *name)
{
    Hash *symbols = symbolTable();
    String *hashkey = stringNew(name);
    Symbol *sym = (Symbol *) hashGet(symbols, (Object *) hashkey);
    
    stringFree(hashkey, TRUE);
    if (sym) {
	return sym->value;
    }
    return NULL;
}

Object *
symbolGetValueWithStatus(char *name, boolean *in_local_scope)
{
    Hash *symbols = symbolTable();
    String *hashkey = stringNew(name);
    Symbol *sym = (Symbol *) hashGet(symbols, (Object *) hashkey);
    Cons *cur_scope;

    stringFree(hashkey, TRUE);
    if (sym) {
	if (sym->scope) {
	    /* Check whether this symbol has already been defined in the
	     * current scope.  If so, there is nothing to be done. */
	    cur_scope = (Cons *) sym->scope->car;
	    if (cur_scope->cdr == (Object *) symbol_scope) {
		*in_local_scope = TRUE;
	    }
	}
	return sym->value;
    }
    return NULL;
}

// Only free a symbol if the symbol table is not in use (where a copy
// will be kept)
void 
symbolFree(Symbol *sym, boolean free_contents)
{
    assert((sym->type == OBJ_SYMBOL), "symbolFree: Not a symbol");
    if (!symbols) {
	if (free_contents) {
	    if (sym->value) {  // Do not free values that are their symbol
		if (sym->value != (Object *) sym) {
		    objectFree(sym->value, free_contents);
		}
	    }
	    skfree(sym->name);
	}
	skfree((void *) sym);
    }
}

void 
symbolForget(Symbol *sym)
{
    assert((sym->type == OBJ_SYMBOL), "symbolForget: Not a symbol");
    skitFail("symbolForget not implemented");
    // Need to remove the symbol from the symbol table and then free it.
}

char *
symbolStr(Symbol *sym)
{
    assert((sym->type == OBJ_SYMBOL), "symbolStr: Not a symbol");
    return newstr(sym->name);
}

// This is used to make a dynamically allocated copy of a symbol 
// that would have been allocated statically.  There is no harm in
// copying a symbol that has already been dynamically allocated but
// neither is there any benefit.
Symbol *
symbolCopy(Symbol *old)
{
    Symbol *new;

    assert((old->type == OBJ_SYMBOL), "symbolCopy: Not a symbol");
    new = symbolNew(old->name);
    new->fn = old->fn;
    if (old->value == (Object *) old) {
	// Symbol is self-referencing, eg 't'
	new->value = (Object *) new;
    }
    else {
	if (old->value) {
	    new->value = objectCopy(new->value);
	}
    }
    return new;
}

void
symbolCreate(char *name, ObjectFn *fn, Object *value)
{
   Symbol *new = symbolNew(name);
   new->fn = fn;
   new->value = value;
}


Object *
symbolEval(Symbol *sym)
{
    if (sym) {
	assert(sym->type == OBJ_SYMBOL, "symbolEval arg is not a symbol");
	if (sym->value) {
	    return (Object *) objRefNew(sym->value);
	}
    }
    return NULL;
}

Object *
symbolExec(Symbol *sym, Object *obj)
{
    Object *result;
    //printSexp(stderr, "SymbolExec: ", sym); 
    //printSexp(stderr, "  -- ", obj); 
    assert(sym && sym->type == OBJ_SYMBOL, "symbolExec arg is not a symbol");
    if (sym->fn) {
	result = (*sym->fn)(obj);
	//printSexp(stderr, "SymbolExec: result", result); 
	return result;
    }
    RAISE(LIST_ERROR, 
	  newstr("symbolExec: symbol %s has no function", sym->name));
}

