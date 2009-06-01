/**
 * @file   options.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
 * \endcode
 * @brief  
 * Functions for managing command line options using hashes
 */

#include <stdio.h>
#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"
#include "skit_param.h"

static Hash *core_options = NULL;
static Cons *print_options = NULL;

// This provides the list of core options for the skit command-line
// interface.  It also shows what an option key list should look like.
// The * in  the strings represents the shortest allowable abbreviation
// of the option. 
static Cons *
coreOptions()
{
    char *str = newstr(
	"(('add*deps' 'a')"
	"('c*onnect')"
	"('d*iff')"
	"('db*type')"
	"('e*xtract')"
	"('ga*ther')"
	"('ge*nerate' 'n')"
	"('g*rep')"
	"('l*ist')"
	"('m*erge')"
	"('p*rint')"
	"('printf*ull' 'f*ull' 'pf*ull')"
	"('printx*xml' 'x*ml' 'px*ml')"
	"('s*catter')"
	"('t*emplate')"
	"('u*sage')"
	"('ve*rsion')"
	"('v*grep'))");
    TokenStr token_str = {str, '\0', NULL};
    Cons *options = (Cons *) objectRead(&token_str);
    char *str2 = objectSexp((Object *) options);
    skfree(str2);
    skfree(str);
    return options;
}

Hash *
coreOptionHash()
{
    if (!core_options) {
	core_options = hashFromOptions(coreOptions());
    }
    return core_options;
}

Cons *
printOptionList()
{
    if (!print_options) {
	print_options = optionlistNew();
	optionlistAdd(print_options, stringNew("x*ml"), 
		      stringNew("type"), (Object *) stringNew("flag"));
	optionlistAdd(print_options, stringNew("f*ull"), 
		      stringNew("type"), (Object *) stringNew("flag"));
	optionlistAdd(print_options, stringNew("sources"), 
		      stringNew("value"), (Object *) int4New(1));
	optionlistAdd(print_options, stringNew("sources"), 
		      stringNew("type"), (Object *) stringNew("integer"));
    }
    return print_options;
}

void
freeOptions()
{
    objectFree((Object *) core_options, TRUE);
    core_options = NULL;
    objectFree((Object *) print_options, TRUE);
    print_options = NULL;
}

// Return a list of option names consisting of each legitimate
// abbreviation, and starting with the full name.
Cons *
optionKeyList(String *instr) 
{
    Cons *cons = NULL;
    char *str;
    char *str_base;
    char *str_end;
    char nextch;
    String *name;
    char *strtok_tmp;
    char *str_for_tok;  // Take a copy of the source string for safety
    assert(instr->type == OBJ_STRING,
	   "optionKeyList: Expected string");

    str_for_tok = newstr(instr->value);
    str_base = newstr(strtok_r(str_for_tok ,"*", &strtok_tmp));
    if (str = strtok_r(NULL, "", &strtok_tmp)) {
	str_end = newstr(str);
    }
    else {
	str_end = newstr("");
    }
    str = str_end;

    name = stringNewByRef(str_base);
    (void) consPush(&cons, (Object *) name);

    while ((nextch = *str++) != '\0') {
	str_base = newstr("%s%c", str_base, nextch);
	name = stringNewByRef(str_base);
	(void) consPush(&cons, (Object *) name);
    }

    skfree(str_end);
    skfree(str_for_tok);
    return cons;
}

// What we want for an options object is a hash, keyed by each possible
// option string and abbreviation, of primary option names.
Hash *
hashFromOptions(Cons *options)
{
    Hash *hash = hashNew(TRUE);
    Cons *aliases;
    String *option_name;
    String *this_option;
    Cons *option_keys;

    assert(options->type == OBJ_CONS,
	   "hashFromOptions: Expected options to be Cons");
    while (options) {
	aliases = (Cons *) consPop(&options);
	assert(aliases->type == OBJ_CONS,
	       "hashFromOptions: Expected aliases to be Cons");

	option_name = NULL;
	while (aliases) {
	    this_option = (String *) consPop(&aliases);
	    assert(this_option->type == OBJ_STRING,
		   "hashFromOptions: Expected this_option to be String");

	    option_keys = optionKeyList(this_option);
	    objectFree((Object *) this_option, TRUE);

	    if (!option_name) {
		option_name = (String *) option_keys->car;  
		assert(option_name->type == OBJ_STRING,
		       "hashFromOptions: Expected option_name to be String");
		// Note that this is a reference, so we must make a copy
		// before doing anything with it.
	    }
	    while (option_keys) {
		assert(option_keys->type == OBJ_CONS,
		       "hashFromOptions: Expected option_keys to be Cons");
		this_option = (String *) consPop(&option_keys);
		assert(this_option->type == OBJ_STRING,
		       "hashFromOptions: Expected this_option to be String(2)");
		
		// TODO: HANDLE RESULT FROM hashAdd
		hashAdd(hash, (Object *) this_option, 
			(Object *) stringDup(option_name));
	    }
	}
    }
    return hash;
}




