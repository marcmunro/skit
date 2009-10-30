/**
 * @file   builtin_symbols.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Defines built-in symbols.  These are symbols for built-in functions, 
 * data structures, etc.
 * Such functions take a single Object as a parameter and return a
 * single object.
 */

#include <stdio.h>
#include <time.h>
#include "skit_lib.h"
#include "skit_param.h"
#include "exceptions.h"

// TODO: Check the argument lists to the functions defined in here, and
// raise errors if they are invalid.

static void
raiseMsg(char *template, char *fn_name, Object *obj)
{
    char *tmp1 = objectSexp(obj);
    char *errmsg = newstr(template, fn_name, tmp1);
    skfree(tmp1);
    RAISE(LIST_ERROR, errmsg);
}

static void
raiseIfNotList(char *fn_name, Object *obj)
{
    obj = dereference(obj);
    if (!(obj && (obj->type == OBJ_CONS))) {
	raiseMsg("%s: invalid arg (expecting list): %s", fn_name, obj);
    }
}

static void
raiseIfNoArg(char *fn_name, Object *obj, int argno)
{
    if (!obj) {
	RAISE(LIST_ERROR, newstr("%s: missing arg %d ", fn_name, argno));
    }
}

static void
raiseIfNotSymbol(char *fn_name, Symbol *sym)
{
    if (!(sym && (sym->type == OBJ_SYMBOL))) {
	raiseMsg("%s: invalid arg (expecting symbol): %s", 
		 fn_name, (Object *) sym);
    }
}

static void
raiseIfNotString(char *fn_name, String *str)
{
    if (!(str && (str->type == OBJ_STRING))) {
	raiseMsg("%s: invalid arg (expecting string): %s", 
		 fn_name, (Object *) str);
    }
}

static void
raiseIfNotHash(char *fn_name, Hash *hash)
{
    if (!(hash && (hash->type == OBJ_HASH))) {
	raiseMsg("%s: invalid arg (expecting hash): %s", 
		 fn_name, (Object *) hash);
    }
}

static void
raiseIfNotRegexp(char *fn_name, Regexp *regex)
{
    if (!(regex && (regex->type == OBJ_REGEXP))) {
	raiseMsg("%s: invalid arg (expecting regexp): %s", 
		 fn_name, (Object *) regex);
    }
}

static void
raiseIfNotInt(char *fn_name, Int4 *arg)
{
    if (!(arg&& (arg->type == OBJ_INT4))) {
	raiseMsg("%s: invalid arg (expecting int4): %s", 
		 fn_name, (Object *) arg);
    }
}

static void
raiseIfMoreArgs(char *fn_name, Object *obj)
{
    if (obj) {
	raiseMsg("%s: too many args: %s",  fn_name, obj);
    }
}

/* Evaluate the car of the provided list, freeing the original contents
 * of car and replacing them with the result.
 */
static void
evalCar(Cons *cons)
{
    Object *obj = cons->car;
    cons->car = objectEval(obj);
    objectFree(obj, TRUE);
}


/* (setq symbol value) */
static Object *
fnSetq(Object *obj)
{
    Symbol *sym;
    Cons *cons = (Cons *) obj;
    Cons *next;
    Object *obj_to_eval;
    Object *oldvalue;
    Object *new;

    raiseIfNotList("setq", obj);
    sym = (Symbol *) cons->car;
    raiseIfNotSymbol("setq", sym);
    next = (Cons *) cons->cdr;
    obj_to_eval = next? next->car: NULL;
    symSet(sym, new = objectEval(obj_to_eval));
    return (Object *) objRefNew(new);
}

// (split SRC DELIMITERS)
// Split a string into a list of string tokens using any character from
// delimiters.
static Object *
fnSplit(Object *obj)
{
    Cons *cons = (Cons *) obj;
    String *source;
    String *split;
    raiseIfNotList("split", obj);

    evalCar(cons);
    source = (String *) dereference(cons->car);
    raiseIfNotString("split", source);

    cons = (Cons *) cons->cdr;
    raiseIfNotList("split", (Object *) cons);

    evalCar(cons);
    split = (String *) dereference(cons->car);
    raiseIfNotString("split", split);
    raiseIfMoreArgs("split", cons->cdr);

    return (Object *) stringSplit(source, split);
}

static Object *
fnSplit2(Object *obj)
{
    Cons *cons = (Cons *) obj;
    String *source;
    String *split;
    raiseIfNotList("split", obj);

    evalCar(cons);
    source = (String *) dereference(cons->car);
    raiseIfNotString("split", source);

    cons = (Cons *) cons->cdr;
    raiseIfNotList("split", (Object *) cons);

    evalCar(cons);
    split = (String *) dereference(cons->car);
    raiseIfNotString("split", split);
    raiseIfMoreArgs("split", cons->cdr);

    return (Object *) stringSplit(source, split);
}

// Creates copy of source leaving source intact
static Object *
fnInt4Promote(Object *obj)
{
    Cons *cons = (Cons *) obj;
    String *str;
    
    raiseIfNotList("try-to-int", obj);
    raiseIfMoreArgs("try-to-int", cons->cdr);
    str = (String *) cons->car;

    if (str && (str->type == OBJ_STRING)) {
	BEGIN {
	    RETURN((Object *) stringToInt4(str));
	}
	EXCEPTION(ex);
	WHEN(TYPE_MISMATCH) {
	    // Do nothing;
	}
	END;
    }
    return objectCopy((Object *) str);
}

static Object *
fnMap(Object *obj)
{
    Cons *src;
    Cons *targ;
    Cons *out;
    Object *item;
    Object *next;
    Symbol *fn;

    raiseIfNotList("map", obj);
    src = (Cons *) obj;

    fn = (Symbol *) src->car;    // arg1
    raiseIfNotSymbol("map", fn);

    raiseIfNotList("map", src->cdr);
    src = (Cons *) src->cdr;
    raiseIfMoreArgs("map", src->cdr);
    raiseIfNotList("map", src->car);
    src = (Cons *) src->car;

    targ = out = consNew(NULL, NULL);
    while (src) {
	raiseIfNotList("map", (Object *) src);
	next = src->cdr;
	src->cdr = NULL;  // Pass a list with just a single arg to fn
	targ->car = symbolExec(fn, (Object *) src);
	src->cdr = next;
	if (src = (Cons *) src->cdr) {
	    targ->cdr = (Object *) consNew(NULL, NULL);
	    targ = (Cons *) targ->cdr;
	}
    }
    return (Object *) out;
}

// (version STRING)
// returns list of version parts from string.  Each part will be an
// integer if possible, otherwise a string.  eg "7.44.3a" would yield:
// (7 44 "3a").  These lists are directly amenable to object comparison
// in order to perform version comparison.
static Object *
fnVersion(Object *obj)
{
    Cons *cons;
    String *str;
    String *dotstr;
    Object *version;
    Cons *split_list;
    Cons *split_list_list;
    Cons *map_list;
    Object *promoted_list;
    
    raiseIfNotList("version", obj);
    cons = (Cons *) obj;

    evalCar(cons);
    str = (String *) cons->car;
    raiseIfNotString("version", str);
    dotstr = stringNew(".");
    split_list = stringSplit(str, dotstr);
    objectFree((Object *) dotstr, TRUE);
    split_list_list = consNew((Object *) split_list, NULL);
    map_list = consNew((Object *) symbolGet("try-to-int"), 
		       (Object *) split_list_list);
    promoted_list = fnMap((Object *) map_list);
    objectFree((Object *) map_list, TRUE);

    return promoted_list;
}

static Object *
fnDBQuote(Object *obj)
{
    Cons *cons;
    String *str1;
    String *str2 = NULL;
    raiseIfNotList("dbquote", obj);
    cons = (Cons *) obj;
    evalCar(cons);
    str1 = (String *) cons->car;
    raiseIfNotString("dbquote", str1);
    cons->car = NULL;  /* Prevent str1 from being automatically freed */
    if (cons = (Cons *) cons->cdr) {
	str2 = (String *) cons->car;
	cons->car = NULL;  /* Prevent str2 from being automatically freed */
    }
    
    return (Object *) sqlDBQuote(str1, str2);
}

static Object *
fnCurTimestamp(Object *obj)
{
    time_t ts = time(NULL);
    struct tm *local = localtime(&ts);
    char tstr[40];
    (void) strftime(tstr, 40, "%Y%m%d%H%M%S", local);

    return (Object *) stringNew(tstr);
}

static Object *
fnQuote(Object *obj)
{
    raiseIfNotList("quote", obj);
    return objectCopy(((Cons *) obj)->car);
}

/* (debug label expr)
 */
static Object *
fnDebug(Object *obj)
{
    Cons *cons = (Cons *) obj;
    String *label;
    char *sexp;
    Object *result;
    raiseIfNotList("debug", obj);
    label = (String *) cons->car;
    raiseIfNotString("debug", label);
    raiseIfNotList("debug", cons->cdr);
    cons = (Cons *) cons->cdr;
    result = objectEval(cons->car);
    BEGIN {
	sexp = objectSexp(result);
    }
    EXCEPTION(ex) {
	objectFree(result, TRUE);
    }
    END;
    fprintf(stderr, "DEBUG %s: %s\n", label->value, sexp);
    skfree(sexp);
    objectFree((Object *) result, TRUE);
    return NULL;
}

static Object *
fnSelect(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *container;
    Object *actual;
    Object *old;
    Object *key;
    evalCar(cons);
    container = (Object *) objRefNew(cons->car);
    while (cons = (Cons *) cons->cdr) {
	actual = dereference(container);
	if (!actual) {
	    objectFree(container, TRUE);
	    return NULL;
	}
	evalCar(cons);
	key = cons->car;
	if (!key) {
	    objectFree(container, TRUE);
	    return NULL;
	}
	old = container;
	BEGIN {
	    container = objSelect(actual, dereference(key));
	}
	EXCEPTION(ex);
	FINALLY {
	    if (old) {
		objectFree(old, TRUE);
	    }
	}
	END;
    }
    actual = dereference(container);
    if (!actual) {
	objectFree(container, TRUE);
	return NULL;
    }
    return container;
}

static Object *
fnStringEq(Object *obj)
{
    Cons *cons = (Cons *) obj;
    String* arg1;
    String* arg2;

    raiseIfNotList("string=", obj);
    evalCar(cons);
    if (arg1 = (String *) dereference(cons->car)) {
	raiseIfNotString("string= (first arg)", arg1);
	cons = (Cons *) cons->cdr;
	raiseIfNotList("string=", (Object *) cons);
	evalCar(cons);
    }
    else {
	return NULL;
    }
    if (arg2 = (String *) dereference(cons->car)) {
	raiseIfNotString("string= (first arg)", arg2);
	raiseIfMoreArgs("string=", cons->cdr);

	if (streq(arg1->value, arg2->value)) {
	    return (Object *) symbolGet("t");
	}
    }

    return NULL;
}

static Object *
fnReplace(Object *obj)
{
    Cons *cons = (Cons *) obj;
    String* arg1;
    Object* arg2;
    String* arg3;
    String *result;

    raiseIfNotList("replace ", obj);
    evalCar(cons);
    arg1 = (String *) dereference(cons->car);
    raiseIfNotString("replace (1st arg)", arg1);
    cons = (Cons *) cons->cdr;
    raiseIfNotList("replace (missing 2nd arg)", (Object *) cons);
    evalCar(cons);
    arg2 = dereference(cons->car);
    if (arg2->type == OBJ_STRING) {
	/* Convert string to regexp */
	arg2 = (Object *) regexpNew(((String *) arg2)->value);
	objectFree(cons->car, TRUE);
	cons->car = arg2;
    }

    raiseIfNotRegexp("replace (2nd arg)", (Regexp *) arg2);
    cons = (Cons *) cons->cdr;
    raiseIfNotList("replace (missing 3rd arg)", (Object *) cons);
    evalCar(cons);
    arg3 = (String *) dereference(cons->car);
    raiseIfNotString("replace (3rd arg)", arg3);

    result = regexpReplace(arg1, (Regexp *) arg2, arg3);
    return (Object *) result;
}

static Object *
fnNot(Object *obj)
{
    static Symbol *t = NULL;

    Cons *cons = (Cons *) obj;

    raiseIfNotList("not", obj);
    evalCar(cons);
    if (cons->car) {
	return NULL;
    }
    else {
	if (!t) {
	    t = symbolGet("t"); 
	}
	return (Object *) t;
    }
}

static Object *
fnAnd(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Cons *prev;
    Object *result;
    raiseIfNotList("and", obj);

    while (cons) {
	evalCar(cons);
	if (cons->car) {
	    prev = cons;
	    cons = (Cons *) cons->cdr;
	}
	else {
	    return NULL;
	}
    }
    result = prev->car;
    prev->car = NULL;
    return result;
}

static Object *
fnOr(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *result;
    raiseIfNotList("or", obj);

    while (cons) {
	evalCar(cons);
	if (result = cons->car) {
	    cons->car = NULL;
	    return result;
	}
	cons = (Cons *) cons->cdr;
    }
    return NULL;
}

static void
appendStr(String *str1, String *str2)
{
    char *str1val = str1->value;
    str1->value = newstr("%s%s", str1->value, str2->value);
    skfree(str1val);
}

static Object *
fnConcat(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *item;
    String *itemstr;
    String *result = stringNew("");

    BEGIN {
	raiseIfNotList("concat", obj);

	while (cons) {
	    evalCar(cons);
	    item = dereference(cons->car);
	    if (item) {
		if (item->type == OBJ_STRING) {
		    appendStr(result, (String *) item);
		}
		else {
		    /* Convert item to string */
		    itemstr = stringNewByRef(objectSexp(item));
		    appendStr(result, itemstr);
		    objectFree((Object *) itemstr, TRUE);
		}
	    }
	    cons = (Cons *) cons->cdr;
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) result, TRUE);
    }
    END;
    return (Object *) result;
}

static Object *
fnCons(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *car = NULL;
    Object *cdr = NULL;
    Cons *result;
    BEGIN {
	raiseIfNotList("cons", obj);
	car = objectEval(cons->car);
	cons = (Cons *) cons->cdr;
	cdr = cons ? objectEval(cons->car): NULL;
	result = consNew(car, cdr);
    }
    EXCEPTION(ex) {
	objectFree(car, TRUE);
	objectFree(cdr, TRUE);
    }
    END;
    return (Object *) result;
}

static Object *
fnPlus(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *item;
    Int4 *result = int4New(0);

    BEGIN {
	raiseIfNotList("+", obj);

	while (cons) {
	    evalCar(cons);
	    item = dereference(cons->car);
	    if (item->type != OBJ_INT4) {
		RAISE(LIST_ERROR, 
		      newstr("\"+\": can only add integers"));
	    }
	    result->value += ((Int4 *) item)->value;
	    cons = (Cons *) cons->cdr;
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) result, TRUE);
    }
    END;
    return (Object *) result;
}

static Object *
fnHashAdd(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Hash *hash;
    Object *key = NULL;
    Object *value = NULL;
    Object *ref;
    Object *old;

    BEGIN {
	raiseIfNotList("hashadd", obj);
	ref = objectEval(cons->car);
	hash = (Hash *) dereference(ref);
	objectFree(ref, TRUE);
	raiseIfNotHash("hashadd", hash);
	cons = (Cons *) cons->cdr;
	ref = cons? objectEval(cons->car): NULL;
	if (!ref) {
	    RAISE(LIST_ERROR, newstr("hashAdd: no key found"));
	}
	key = objectCopy(dereference(ref));
	objectFree(ref, TRUE);
	cons = (Cons *) cons->cdr;
	ref = cons? objectEval(cons->car): NULL;
	if (!ref) {
	    RAISE(LIST_ERROR, newstr("hashAdd: no value found"));
	}
	value = objectCopy(dereference(ref));
	old = hashAdd(hash, key, value);
	objectFree(old, TRUE);
    }
    EXCEPTION(ex) {
	objectFree(key, TRUE);
	objectFree(value, TRUE);
    }
    END;
    return ref;
}

static void
evalStr(char *str)
{
    char *tmp = newstr(str);
    Object *obj = evalSexp(tmp);
    objectFree(obj, TRUE);
    skfree(tmp);
}



static void 
initBaseSymbols()
{
    Symbol symbol_t = {OBJ_SYMBOL, "t", NULL, (Object *) &symbol_t};
    Hash *dbhash = hashNew(TRUE);

    (void) symbolCopy(&symbol_t);
    symbolCreate("setq", &fnSetq, NULL);
    symbolCreate("split", &fnSplit, NULL);
    symbolCreate("split2", &fnSplit2, NULL);
    symbolCreate("try-to-int", &fnInt4Promote, NULL);
    symbolCreate("map", &fnMap, NULL);
    symbolCreate("version", &fnVersion, NULL);
    symbolCreate("current-timestamp",  &fnCurTimestamp, NULL);
    symbolCreate("params", NULL, NULL);
    symbolCreate("debug", &fnDebug, NULL);
    symbolCreate("tuple", NULL, NULL);
    symbolCreate("tuplestack", NULL, NULL);
    symbolCreate("quote", &fnQuote, NULL);
    symbolCreate("dbquote", &fnDBQuote, NULL);
    symbolCreate("string=", &fnStringEq, NULL);
    symbolCreate("select", &fnSelect, NULL);
    symbolCreate("replace", &fnReplace, NULL);
    symbolCreate("not", &fnNot, NULL);
    symbolCreate("and", &fnAnd, NULL);
    symbolCreate("or", &fnOr, NULL);
    symbolCreate("concat", &fnConcat, NULL);
    symbolCreate("cons", &fnCons, NULL);
    symbolCreate("+", &fnPlus, NULL);
    symbolCreate("hashadd", &fnHashAdd, NULL);
    symbolCreate("dbhandlers", NULL, (Object *) dbhash);

    evalStr("(setq dbtype 'postgres')");
    evalStr("(setq dbver nil)");
    evalStr("(setq templates-dir 'templates')");

    registerPGSQL();	// TODO: Move this call to somewhere more sensible
}

void
initBuiltInSymbols() 
{
    static boolean done = FALSE;
    if (!done) {
	initBaseSymbols();
    }

    done = TRUE;
}
  
