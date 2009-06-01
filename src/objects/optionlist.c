/**
 * @file   optonlist.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating optionlists.  Optionlists are
 * read from xml template files to provide a clue to the templates API.
 * An optionlist is actually a list containing the following:
 * (complete aliases optioninfo)
 * where:
 * - complete is a boolean and is t or nil. It is set to nil whenever
 *   a new option or alias is added.  It is set to true after processing
 *   the aliases list to provide a complete list of all valid
 *   abbreviations.  This is done whenever a lookup is performed against
 *   a not-complete optionlist
 * - Aliases is a hash of string keys to string values  The
 *   string values are themselves the keys into optioninfo
 * - Optioninfo is a hash of option names to alists.  The alist entries
 *   contain name value pairs for parameters relevant to the option.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../skit_lib.h"
#include "../exceptions.h"

/* Create a new optionlist.  Note that it does this by generating from 
 * a string expression.  Although not the fastest way, this gives some
 * idea of the power of string expressions and reduces the code that has
 * to be written. */
Cons *
optionlistNew()
{
    char *tmp = newstr("(t <> <>)");
    TokenStr token_str = {tmp, '\0', NULL};
    Cons* optionlist = (Cons *) objectRead(&token_str);
    skfree(tmp);
    return optionlist;
}

/* Predicate to determine whether an object is a true boolean.  Valid
 * values are the symbol 't' and the value NULL.  */
static boolean
isBool(Object *obj)
{
    if (obj) {
	return (obj == (Object *) symbolGet("t"));
    }
    // Value is nil which is a good boolean value
    return TRUE;
}


/* Predicate to determine whether an object is a hash.
* QUESTIONS: should the arg type be object?  Should this be moved into
* hash.c and made extern?  */
static boolean
isHash(Hash *hash)
{
    return (hash && (hash->type == OBJ_HASH));
}

/* Predicate to determine whether an object is an optionlist.  Note
 * that an optionlist is a compound object so the test is relatively
 * complex.  The rule here is that if it looks like an optionlist, ten
 * it is.  */
static boolean
isOptionlist(Cons *optionlist)
{
    Cons *cons2;
    Cons *cons3;
    Symbol *complete;

    if (isCons(optionlist)) {
	cons2 = (Cons *) optionlist->cdr;
    }
    else {
	return FALSE;
    }

    if (isCons(cons2)) {
	cons3 = (Cons *) cons2->cdr;
    }
    else {
	return FALSE;
    }

    return (isCons(cons3) &&
	    isBool(optionlist->car) &&
	    isHash((Hash *) cons2->car) &&
	    isHash((Hash *) cons3->car));
}

/* Set the complete flag in the optionlist to false.  */
static void
setIncomplete(Cons *list)
{
    list->car = NULL;
}

/* Set the complete flag in the optionlist to true.  */
static void
setComplete(Cons *list)
{
    static Symbol* t = NULL;
    if (!t) {
	t = symbolGet("t");
    }
    list->car = (Object *) t;
}

/* Extract the aliases hash from the optionlist */
static Hash *
getAliases(Cons *list)
{
    Cons *cons2 = (Cons *) list->cdr;
    return (Hash *) cons2->car;
}

/* Extract the optioninfo hash from the optionlist */
static Hash *
getOptioninfo(Cons *list)
{
    Cons *cons2 = (Cons *) list->cdr;
    Cons *cons3 = (Cons *) cons2->cdr;
    return (Hash *) cons3->car;
}

// TODO: Consider using symbols for the keys
// Always consumes the objects associated with the final 3 parameters.
void
optionlistAdd(Cons *list, String *option_name, 
	      String *field, Object *value)
{
    char *x;
    assert(isOptionlist(list), 
	   "optionlistAdd: list is not an optionlist");

    //fprintf(stderr, "ADD: %s", option_name->value);
    //if (field) fprintf(stderr, ", field=%s", field->value);
    //if (value) {
    //	if (value->type == OBJ_STRING) 
    //	    fprintf(stderr, ", value=%s", ((String *) value)->value);
    //}
    //fprintf(stderr, "\n");

    setIncomplete(list);
    {
	Hash *hash = getOptioninfo(list);
	Cons *entry = consNew((Object *) field, (Object *) value);
	Cons *old_alist = (Cons *) hashGet(hash, (Object *) option_name);
	Cons *new_alist;
	new_alist = consNew((Object *) entry, (Object *) old_alist);
	// TODO: HANDLE RESULT FROM hashAdd and thereby eliminate the 
	// call to hashGet
	hashAdd(hash, (Object *) option_name, (Object *) new_alist);
	//fprintf(stderr, "IS NOW: %s\n", x = objectSexp((Object *) list));
	//skfree(x);
    }
}

void
optionlistAddAlias(Cons *list, String *alias, String *key)
{
    Hash *hash;
    assert(isOptionlist(list), 
	   "optionlistAddAlias: list is not an optionlist");
    hash = getAliases(list);
    setIncomplete(list);
    // TODO: HANDLE RESULT FROM hashAdd
    hashAdd(hash, (Object *) alias, (Object *) key);
}

// Return a list containing only the keys that contain *, destroying
// the input alist.  The resulting list will be dynamically allocated
// but will contain references to strings which may not be freed.
static Cons *
abbrevsFromOptionsAlist(Cons *alist)
{
    Cons *keylist = NULL;
    Cons *cur = alist;
    Cons *next;
    Cons *entry;
    String *key;
    while (cur) {
	entry = (Cons *) cur->car;
	next = (Cons *) cur->cdr;

	key = (String *) entry->car;
	if (stringMatch(key, "\\*")) {
	    // Recyle entry, adding it to the result list
	    entry->cdr = (Object *) keylist;
	    keylist = entry;
	}
	else {
	    objectFree((Object *) entry, FALSE);
	}
	objectFree((Object *) cur, FALSE);
	cur = next;
    }
    return keylist;
}

// Process one set of keys, which is a list of all the keys that match
// an original_key.  The original_key entry must be replaced with the
// first key from the keyset.  All other keys in the keyset are added as
// aliases.
// Consumes the abbrev_list while modifying optionlist
static void
addAbbrevs(Cons *optionlist, String *newkey, Cons *abbrevs)
{
    Hash *hash = getAliases(optionlist);
    Cons *list;
    Cons *next;
    String *alias;

    list = abbrevs;
    while (list) {
	// Add the next alias to the hash of aliases
	next = (Cons *) list->cdr;
	alias = (String *) list->car;
	list->car = NULL;
	if (!hashGet(hash, (Object *) alias)) {
	    // TODO: HANDLE RESULT FROM hashAdd
	    hashAdd(hash, (Object *) alias, objectCopy((Object *) newkey));
	}
	else {
	    objectFree((Object *) alias, TRUE);
	}
	skfree(list);
	list = next;
    }
    return;
}


static void
makeComplete(Cons *optionlist)
{
    Hash *aliashash= getAliases(optionlist);
    Hash *optionhash = getOptioninfo(optionlist);
    Cons *options_alist = hashToAlist(optionhash);
    Cons *aliases_alist = hashToAlist(aliashash);
    Object *original;
    String *fullkey;
    Cons *abbrevs;
    Cons *entries;
    Cons *next;
    Cons *tmp;
    String *alias;
    String *original_key;
    String *key;
    // For each key in the hash, if the key contains *, replace the key
    // in the hash with a version without the *.  Then record aliases
    // for each abbreviation of the key.

    entries = abbrevsFromOptionsAlist(options_alist);
    while (entries) {
	next = (Cons *) entries->cdr;
	original_key = (String *) entries->car;
	
	abbrevs = optionKeyList(original_key);
	// The first entry of abbrevs contains the full-length,
	// unabbreviated value.
	tmp = abbrevs;
	fullkey = (String *) abbrevs->car;
	abbrevs = (Cons *) abbrevs->cdr;
	tmp->car = NULL;
	objectFree((Object *) tmp, FALSE);
	addAbbrevs(optionlist, fullkey, abbrevs);

	// Now remove the original entry containing the '*'
	original = hashDel(optionhash, (Object *) original_key);

	// And replace with the full key
	// TODO: HANDLE RESULT FROM hashAdd
	hashAdd(optionhash, (Object *) fullkey, original);

	objectFree((Object *) entries, FALSE);
	entries = next;
    }

    entries = aliases_alist;
    while (entries) {
	next = (Cons *) entries->cdr;
	tmp = (Cons *) entries->car;
	alias = (String *) tmp->car;
	key = (String *) tmp->cdr;
	objectFree((Object *) tmp, FALSE);
	
	abbrevs = optionKeyList(alias);
	if (!(abbrevs->cdr)) {
	    // If abbrevs is only one entry long, there is nothing to
	    // do.  Just free the abbrevs list
	    objectFree((Object *) abbrevs, TRUE);
	}
	else {
	    addAbbrevs(optionlist, key, abbrevs);

	    // Now remove the original entry containing the '*'
	    original = hashDel(aliashash, (Object *) alias);
	    objectFree(original, TRUE);
	}
	objectFree((Object *) entries, FALSE);
	entries = next;
    }

    setComplete(optionlist);
}

// Returns a reference to the details for the given option.  This
// reference must not be freed by the caller.
Object *
optionlistGetOption(Cons *list, String *key)
{
    Hash *hash;
    Object *real_key;
    assert(isOptionlist(list), 
	   "optionlistGetOption: list is not an optionlist");

    if (!list->car) {
	makeComplete(list);
    }

    hash = getAliases(list);
    real_key = hashGet(hash, (Object *) key); 
    if (!real_key) {
	real_key = (Object *) key;
    }

    hash = getOptioninfo(list);
    return hashGet(hash, (Object *) real_key);
}

// Returns a reference to the fullname for a given key.  This
// reference must not be freed by the caller.
String *
optionlistGetOptionName(Cons *list, String *key)
{
    Hash *hash;
    String *real_key;
    assert(isOptionlist(list), 
	   "optionlistGetOptionName: list is not an optionlist");

    if (!list->car) {
	makeComplete(list);
    }

    hash = getAliases(list);
    real_key = (String *) hashGet(hash, (Object *) key); 
    if (!real_key) {
	real_key = key;
    }

    return real_key;
}

Object *
optionlistGetOptionValue(Cons *list, String *key, String *field)
{
    Cons *alist = (Cons *) optionlistGetOption(list, key);
    return alistGet(alist, (Object *) field);
}

