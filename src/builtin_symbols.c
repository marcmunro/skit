/**
 * @file   builtin_symbols.c
 * \code
 *     Copyright (c) 2014 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Defines built-in symbols.  These are symbols for built-in functions, 
 * data structures, etc.
 * Such functions take a single Object as a parameter and return a
 * single object.
 */

#include <stdio.h>
#include <string.h>
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


static Object *
nextParam(char * fn_name, Cons **p_list, ObjType type,
          boolean required, boolean evaluate, int param_no)
{
    Object *elem;
    ObjType objtype;

    if (!*p_list) {
        RAISE(LIST_ERROR,
              newstr("%s: missing arg no %d", fn_name, param_no));
    }
    if ((*p_list)->type != OBJ_CONS) {
        RAISE(LIST_ERROR,
              newstr("%s: invalid args.  Expecting a list, got %s",
                     fn_name, objTypeName((Object *) *p_list)));
    }

    if (elem = (*p_list)->car) {
        if (evaluate) {
            elem = objectEval(elem);
        }
        else {
            elem = objectCopy(elem);
        }

	if (type != OBJ_UNDEFINED) {
	    if (elem) {
		objtype = dereference(elem)->type;

		if (objtype != type) {
		    char *sexp = objectSexp(elem);
		    char *msg = newstr("%s: invalid arg (no %d).  "
				       "Expecting %s got %s: %s",
				       fn_name, param_no, typeName(type),
				       typeName(objtype), sexp);
		    skfree(sexp);
		    objectFree(elem, TRUE);
		    RAISE(LIST_ERROR, msg);
		}
	    }
	    else {
		if (required) {
		    RAISE(LIST_ERROR,
			  newstr("%s: invalid arg (no %d).  "
				 "Expecting %s got null",
				 fn_name, param_no, typeName(type)));
		}
	    }
	}
    }
    else {
        if (required) {
            RAISE(LIST_ERROR,
                  newstr("%s: missing required paramer %d", fn_name, param_no));
        }
    }

    *p_list = (Cons *) (*p_list)->cdr;
    return elem;
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
raiseIfMoreArgs(char *fn_name, Object *obj)
{
    if (obj) {
	raiseMsg("%s: too many args at %s",  fn_name, obj);
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


// THIS HAS BEEN DEEPRECATED AS A POTENTIAL SOURCE OF MANY ERRORS
// INSTEAD WE WILL TAKE A MORE FUNCTIONAL APPROACH TO THINGS
// TODO: Ensure that var definitions cannot overwrite the current
// values of an in-scope variable.
/* (setq symbol value) */
/* This operation is fraught with peril, as overwriting the contents of 
 * a symbol cause its current contents to be freed.  This can be a
 * problem as the contents may be the original source for object
 * references used elsewhere.
 */
static Object *
fnSetq(Object *obj)
{
    Symbol *sym;
    Cons *cons = (Cons *) obj;
    Cons *next;
    Object *obj_to_eval;
    Object *new;

    raiseIfNotList("setq", obj);
    sym = (Symbol *) cons->car;
    raiseIfNotSymbol("setq", sym);
    next = (Cons *) cons->cdr;
    obj_to_eval = next? next->car: NULL;
    new = objectEval(obj_to_eval);

    symSet(sym, new);
    return (Object *) objRefNew(new);
}

// (join LIST SEPARATOR)
static Object *
fnJoin(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Cons *list;
    String *separator = NULL;
    String *result = NULL;
    String *item;

    raiseIfNotList("join", obj);
    evalCar(cons);
    list = (Cons *) dereference(cons->car);
    if (!list) {
	return (Object *) stringNew("");
    }
    raiseIfNotList("join", (Object *) list);
    cons = (Cons *) cons->cdr;
    if (cons) {
	evalCar(cons);
	separator = (String *) dereference(cons->car);
	raiseIfNotString("join", separator);
    }
    
    while (list) {
	item = (String *) dereference(list->car);
	raiseIfNotString("join", item);
	if (result) {
	    if (separator) {
		appendStr(result, separator);
	    }
	    appendStr(result, item);
	}
	else {
	    result = stringNew(item->value);
	}
	list = (Cons *) dereference(list->cdr);
    }
    if (!result) {
	result = stringNew("");
    }
    return (Object *) result;
}

// (split SRC DELIMITERS &OPTIONAL MATCH_QUOTES)
// Split a string into a list of string tokens using any character from
// delimiters.
static Object *
fnSplit(Object *obj)
{
    Cons *cons = (Cons *) obj;
    String *source;
    String *split;
    boolean match_quotes = FALSE;
    raiseIfNotList("split", obj);

    evalCar(cons);
    source = (String *) dereference(cons->car);
    if (!source) {
	return NULL;
    }
    raiseIfNotString("split", source);

    cons = (Cons *) cons->cdr;
    raiseIfNotList("split", (Object *) cons);

    evalCar(cons);
    split = (String *) dereference(cons->car);
    raiseIfNotString("split", split);

    /* Third parameter is optional */
    if (cons->cdr) {
	cons = (Cons *) cons->cdr;	
	raiseIfNotList("split", (Object *) cons);
	match_quotes = (cons->car != NULL);
    }
    
    raiseIfMoreArgs("split", cons->cdr);

    return (Object *) stringSplit(source, split, match_quotes);
}

// Creates copy of source leaving source intact
static Object *
fnInt4Promote(Object *obj)
{
    String *volatile str;
    Cons *cons = (Cons *) obj;

    raiseIfNotList("try-to-int", obj);
    raiseIfMoreArgs("try-to-int", cons->cdr);
    evalCar(cons);
    str = (String *) dereference(cons->car);
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
    split_list = stringSplit(str, dotstr, FALSE);
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
	evalCar(cons);
	str2 = (String *) cons->car;
	cons->car = NULL;  /* Prevent str2 from being automatically freed */
    }
    
    return (Object *) sqlDBQuote(str1, str2);
}

static Object *
fnCurTimestamp(Object *obj)
{
    UNUSED(obj);

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

static Object *
fnList(Object *obj)
{
    Cons *head;
    raiseIfNotList("list", obj);
    head = (Cons *) obj;
    while (head) {
	evalCar(head);
	head = (Cons *) head->cdr;
    }
    return objectCopy(obj);
}

/* (debug label expr)
 */
static Object *
fnDebug(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *volatile result = NULL;
    char *volatile sexp = NULL;
    String *volatile label = NULL;

    BEGIN {
	label = (String *) nextParam("debug", &cons, OBJ_STRING, TRUE, TRUE, 1);
	result = nextParam("debug", &cons, OBJ_UNDEFINED, TRUE, TRUE, 2);
	sexp = objectSexp(result);
	fprintf(stderr, "DEBUG %s: %s\n", label->value, sexp);
    }
    EXCEPTION(ex);
    FINALLY {
	if (sexp) {
	    skfree(sexp);
	}
	objectFree((Object *) label, TRUE);
	objectFree((Object *) result, TRUE);
    }
    END;
    return NULL;
}

static Object *
fnSelect(Object *obj)
{
    Object *volatile old;
    Cons *cons = (Cons *) obj;
    Object *container;
    Object *actual;
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
	    objectFree(old, TRUE);
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
    Object *arg1 = nextParam("string=", &cons, OBJ_STRING, FALSE, TRUE, 1);
    Object *arg2 = nextParam("string=", &cons, OBJ_STRING, FALSE, TRUE, 2);
    Object *result = NULL;

    if (arg1 && arg2 && 
	streq(((String *) dereference(arg1))->value, 
	      ((String *) dereference(arg2))->value)) {
	result = (Object *) symbolGet("t");
    }

    objectFree((Object *) arg1, TRUE);
    objectFree((Object *) arg2, TRUE);
    return result;
}

static Object *
fnStringMatch(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *result = NULL;
    Regexp* arg1 = (Regexp *) nextParam("string-match", &cons, OBJ_REGEXP, 
					TRUE, TRUE, 1);
    String* arg2 = (String *) nextParam("string-match", &cons, OBJ_STRING, 
					FALSE, TRUE, 2);

    if (arg2 && regexpMatch((Regexp *) dereference((Object *) arg1), 
			    (String *) dereference((Object *) arg2))) {
	result = (Object *) symbolGet("t");
    }

    objectFree((Object *) arg1, TRUE);
    objectFree((Object *) arg2, TRUE);
    return result;
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
    Cons *prev = NULL;
    Object *result = NULL;
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
    if (prev) {
	result = prev->car;
	prev->car = NULL;
    }
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

static Object *
fnConcat(Object *obj)
{
    String *volatile result = stringNew("");
    Cons *cons = (Cons *) obj;
    Object *item;
    String *itemstr;

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
    Object *volatile car = NULL;
    Object *volatile cdr = NULL;
    Cons *cons = (Cons *) obj;
    Cons *result = NULL;
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
fnCar(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *list = NULL;
    Object *item = NULL;
    raiseIfNotList("car", obj);

    evalCar(cons);
    list = dereference(cons->car);
    item = ((Cons *) list)->car;

    return (Object *) objRefNew(item);
}

static Object *
fnCdr(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *list = NULL;
    Object *item = NULL;
    raiseIfNotList("cdr", obj);

    evalCar(cons);
    list = dereference(cons->car);
    //dbgSexp(list);
    item = ((Cons *) list)->cdr;
    //dbgSexp(item);

    return (Object *) objRefNew(item);
}

static Object *
fnPlus(Object *obj)
{
    Int4 *volatile result = int4New(0);
    Cons *cons = (Cons *) obj;
    Object *item;

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

/* This operates as a unary minus or as a the subtraction of all other
 * items from the first item of the list  
 * ie: (- 2) -> -2 (- 2 1) -> 1
 */
static Object *
fnMinus(Object *obj)
{
    Int4 *volatile result = int4New(0);
    Cons *cons = (Cons *) obj;
    Object *item;
    boolean first = TRUE;

    BEGIN {
	raiseIfNotList("-", obj);

	while (cons) {
	    evalCar(cons);
	    item = dereference(cons->car);
	    if (item->type != OBJ_INT4) {
		RAISE(LIST_ERROR, 
		      newstr("\"-\": can only subtract integers"));
	    }
	    if (first && cons->cdr) {
		result->value = ((Int4 *) item)->value;
	    }
	    else {
		result->value -= ((Int4 *) item)->value;
	    }
	    cons = (Cons *) cons->cdr;
	    first = FALSE;
	}
    }
    EXCEPTION(ex) {
	objectFree((Object *) result, TRUE);
    }
    END;
    return (Object *) result;
}

/* Convert a string into a regexp */
static Object *
fnRegexp(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *result = NULL;
    Object *obj1 = nextParam("re", &cons, OBJ_STRING, TRUE, TRUE, 1);
    String *str = (String *) dereference(obj1);
    result = (Object *) regexpNew(str->value);
    objectFree(obj1, TRUE);

    return result;
}

/* Return length of a list as an integer. */
static Object *
fnLength(Object *obj)
{
    Cons *cons = (Cons *) obj;
    Object *obj1;
    Int4 *result = NULL;

    if (cons->car) {
	if (obj1 = objectEval(cons->car)) {
	    result = int4New(0);
	    raiseIfNotList("length", (Object *) obj1);
	    cons = (Cons *) dereference(obj1);
	    while (cons) {
		result->value++;
		cons = (Cons *) cons->cdr;
	    }
	    objectFree(obj1, TRUE);
	}
    }

    return (Object *) result;
}

static Object *
fnHashAdd(Object *obj)
{
    Object *volatile key = NULL;
    Object *volatile value = NULL;
    Cons *cons = (Cons *) obj;
    Hash *hash;
    Object *ref = NULL;
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

static Object *
fnUsername(Object *obj)
{
    UNUSED(obj);
    return (Object *) username();
}

static void
defineVar(char *name, Object *obj)
{
    Symbol *sym = symbolNew(name);
    symSet(sym, obj);
}

static void 
initBaseSymbols()
{
    static Symbol symbol_t = {OBJ_SYMBOL, "t", NULL, (Object *) &symbol_t};
    Hash *dbhash = hashNew(TRUE);
    String *xml_version = stringNew(SKIT_XML_VERSION);

    (void) symbolCopy(&symbol_t);

    /* The XX comments below identify functions that have been
     * converted to use the nextParam function, which is aimed at
     * simplifying the handling of babylisp expressions.
     * TODO: Refactor more of these functions. */

    symbolCreate("setq", &fnSetq, NULL);
    symbolCreate("join", &fnJoin, NULL);
    symbolCreate("split", &fnSplit, NULL);
    symbolCreate("try-to-int", &fnInt4Promote, NULL);
    symbolCreate("map", &fnMap, NULL);
    symbolCreate("version", &fnVersion, NULL);
    symbolCreate("current-timestamp",  &fnCurTimestamp, NULL);  
    symbolCreate("params", NULL, NULL);
    symbolCreate("debug", &fnDebug, NULL);                      /*XX*/
    symbolCreate("tuple", NULL, NULL);
    symbolCreate("tuplestack", NULL, NULL);
    symbolCreate("quote", &fnQuote, NULL);
    symbolCreate("list", &fnList, NULL);
    symbolCreate("dbquote", &fnDBQuote, NULL);
    symbolCreate("string=", &fnStringEq, NULL);                 /*XX*/
    symbolCreate("string-match", &fnStringMatch, NULL);         /*XX*/
    symbolCreate("select", &fnSelect, NULL);
    symbolCreate("replace", &fnReplace, NULL);
    symbolCreate("not", &fnNot, NULL);
    symbolCreate("and", &fnAnd, NULL);
    symbolCreate("or", &fnOr, NULL);
    symbolCreate("concat", &fnConcat, NULL);
    symbolCreate("cons", &fnCons, NULL);
    symbolCreate("car", &fnCar, NULL);
    symbolCreate("cdr", &fnCdr, NULL);
    symbolCreate("+", &fnPlus, NULL);
    symbolCreate("-", &fnMinus, NULL);
    symbolCreate("re", &fnRegexp, NULL);                        /*XX*/
    symbolCreate("length", &fnLength, NULL);
    symbolCreate("hashadd", &fnHashAdd, NULL);
    symbolCreate("username", &fnUsername, NULL);
    symbolCreate("dbhandlers", NULL, (Object *) dbhash);
    symbolCreate("skit_xml_version", NULL, (Object *) xml_version);

    defineVar("dbtype", (Object *) stringNew("postgres"));
    defineVar("dbver", NULL);
    defineVar("templates-dir",  (Object *) stringNew("templates"));

    defineVar("xnode_seq", (Object *) int4New(1));

    //evalStr("(setq dbtype 'postgres')");
    //evalStr("(setq dbver nil)");
    //evalStr("(setq templates-dir 'templates')");

    registerPGSQL();	// TODO: Move this call to somewhere more appropriate
}


void
initBuiltInSymbols(void) 
{
    static boolean done = FALSE;
    if (!done) {
	initBaseSymbols();
    }
}
  
