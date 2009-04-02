/**
 * @file   check_object.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Unit tests for general object handling
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"

#define SIMPLE_TEST(name, cond, str)		\
    START_TEST(name)				\
{						\
    fail_unless(cond, str);			\
    FREEMEMWITHCHECK;				\
}						\
END_TEST

static boolean
objectReadCheck(char *instr, char *chkstr)
{
    Object *obj = objectFromStr(instr);
    char *obstr = objectSexp(obj);
    boolean result = streq(obstr, chkstr);
    objectFree(obj, TRUE);
    skfree(obstr);
    return result;
}

static boolean
objectReadCheckNoFree(char *instr, char *chkstr)
{
    Object *obj = objectFromStr(instr);
    char *obstr = objectSexp(obj);
    boolean result = streq(obstr, chkstr);
    skfree(obstr);
    return result;
}

static boolean
objectTypeCheck(Object *obj, ObjType otype)
{
    return obj? obj->type == otype: FALSE;
}

static boolean
objectIntCheck(Object *obj, int value)
{
    if (obj && (obj->type == OBJ_INT4)) {
	return ((Int4 *) obj)->value == value;
    }
    return FALSE;
}

boolean
objectStrCheck(Object *obj, char *value)
{
    if (obj && (obj->type == OBJ_STRING)) {
	return streq(((String *) obj)->value, value);
    }
    return FALSE;
}

START_TEST(integer_object)
{
    Object *obj = objectFromStr("123");
    char *tmpstr;
    fail_unless(objectIntCheck(obj, 123), 
		"Incorrect integer value for Int4");
    fail_unless(streq(tmpstr = objectSexp(obj), "123"), 
		"Incorrect string representation for Int4");
    skfree(tmpstr);
    objectFree(obj, TRUE);
    
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(string_object1)
{
    Object *obj = objectFromStr("'456'");
    char *tmpstr;
    fail_unless(objectStrCheck(obj, "456"), 
		"Incorrect value for String(1): (%s)", ((String *)obj)->value);
    fail_unless(streq(tmpstr = objectSexp(obj), "'456'"), 
		"Incorrect string representation for String(1)");
    objectFree(obj, TRUE);
    skfree(tmpstr);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(nil_object)
{
    Object *obj = objectFromStr("()");
    fail_if(obj, "Object must be nil");
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(simple_list)
{
    char *tmp;
    Object *obj;
    Object *obj2;
    obj = objectFromStr("(567)");
    obj2 = getCar((Cons *) obj);
    fail_unless(objectTypeCheck(obj, OBJ_CONS), "Expected Cons cell\n");
    fail_unless(objectIntCheck(obj2, 567), 
		"Incorrect integer value for List of Int4\n");
    fail_unless(streq(tmp = objectSexp(obj), "(567)"), 
		"Incorrect string representation for List(1)\n");
    objectFree(obj, TRUE);
    skfree(tmp);
    FREEMEMWITHCHECK;
}
END_TEST

int
exec_broken_list1(void *param)
{
    Object *obj;
    obj = objectFromStr("(42 43 44 (. 567))");
}

START_TEST(broken_list1)
{
    BEGIN {
	exec_broken_list1(NULL);
    }
    EXCEPTION(e);
    WHEN_OTHERS {
	fail_unless(contains(e->text, "checkedConsRead: list beginning "
			     "with \".\" not permitted"),
		    "Failure of list beginning with '.' not detected");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

int
exec_broken_list2(void *param)
{
    Object *obj;
    obj = objectFromStr("(1 . 567 789)");
}

START_TEST(broken_list2)
{
    BEGIN {
	exec_broken_list2(NULL);
    }
    EXCEPTION(e);
    WHEN_OTHERS {
	fail_unless(contains(e->text, 
			     "consRead: Unexpected objects after cdr of alist"),
		    "Failure of alist with superfluous entries not detected");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

int
exec_broken_list3(void *param)
{
    Object *obj;
    obj = objectFromStr("( 567 789]");
}

START_TEST(broken_list3)
{
    BEGIN {
	exec_broken_list3(NULL);
    }
    EXCEPTION(e);
    WHEN_OTHERS {
	fail_unless(contains(e->text, 
			     "Invalid closing token at end of list"),
		    "alist with invalid closing token not detected");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

int
exec_broken_list4(void *param)
{
    Object *obj;
    obj = objectFromStr("(string= 'y' (get tuple 'superuser')");
}

START_TEST(broken_list4)
{
    BEGIN {
	exec_broken_list4(NULL);
    }
    EXCEPTION(e);
    WHEN_OTHERS {
	fail_unless(contains(e->text, 
			     "premature end of expression"),
		    "list with missing closing paren not detected");
    }
    END;

    FREEMEMWITHCHECK;
}
END_TEST

SIMPLE_TEST(nil_string, 
	    objectReadCheck("()", "nil"),
	    "Incorrect string representation of NULL\n")

SIMPLE_TEST(integer_list, 
	    objectReadCheck("(567 12 34 56)", "(567 12 34 56)"),
	    "Incorrect string representation of list(1)\n")

SIMPLE_TEST(list_with_list, 
	    objectReadCheck("(567 (12 34) 56)", "(567 (12 34) 56)"),
	    "Incorrect string representation of list with list(1)\n")

SIMPLE_TEST(list_with_nil, 
	    objectReadCheck("(567 nil 56)", "(567 nil 56)"),
	    "Incorrect string representation of nil within list\n")

SIMPLE_TEST(alist, 
	    objectReadCheck("((1 . 2) (2 . 3) (3 . 4))", 
			    "((1 . 2) (2 . 3) (3 . 4))"),
	    "Incorrect string representation of alist(1)\n")

SIMPLE_TEST(simple_list2, 
	    objectReadCheck("((1 . (2 3)))", "((1 2 3))"),
	    "Incorrect string representation of list(2)\n")

SIMPLE_TEST(vector1, 
	    objectReadCheck("[(1 . (2 3))]", "[(1 2 3)]"),
	    "Incorrect string representation of vector(1)\n")

SIMPLE_TEST(hash1, 
	    objectReadCheck("<(1 . 2) (2 . 3)>", "<(1 . 2) (2 . 3)>"),
	    "Incorrect string representation of hash(1)\n")

SIMPLE_TEST(nil, 
	    objectReadCheck("(nil () nil)", "(nil nil nil)"),
	    "Incorrect string representation of list of nils\n")

SIMPLE_TEST(emptyhash, 
	    objectReadCheck("<>", "<>"),
	    "Incorrect string representation of empty hash\n")

SIMPLE_TEST(emptyhash2, 
	    objectReadCheck("(<>)", "(<>)"),
	    "Incorrect string representation of list with empty hash\n")

SIMPLE_TEST(optionlist, 
	    objectReadCheck("(t <> <>)", "(t <> <>)"),
	    "Incorrect string representation of optionlist\n")

START_TEST(is_alist1)
{
    Object *obj;
    obj = objectFromStr("((1 . 2) (2 . 3) (3 . 4))");
    fail_unless(consIsAlist(obj),
	"Incorrect non-identification of alist(1)");
    objectFree(obj, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(is_alist2)
{
    Object *obj;
    obj = objectFromStr("((1))");
    fail_unless(consIsAlist(obj),
	"Incorrect non-identification of alist(2)");
    objectFree(obj, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(not_alist1)
{
    Object *obj;
    obj = objectFromStr("((1 . 2) (2 . 3) nil (4 . 5))");
    fail_if(consIsAlist(obj),
	"Incorrect identification of non-alist(1)");
    objectFree(obj, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(not_alist2)
{
    Object *obj;
    obj = objectFromStr("(1 2 3)");
    fail_if(consIsAlist(obj),
	"Incorrect identification of non-alist(2)");
    objectFree(obj, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST


SIMPLE_TEST(string_list, 
	    objectReadCheck("(('adddeps' 'a') ('connect' 'c'))",
		"(('adddeps' 'a') ('connect' 'c'))"),
	    "Incorrect string representation of stringlist(1)\n")

SIMPLE_TEST(symbol_list, 
	    objectReadCheck("mysymbol",	"mysymbol"),
	    "Incorrect string representation of symbol(1)\n")

START_TEST(objcmp1)
{
    Object *obj1;
    Object *obj2;

    obj1 = objectFromStr("(8 0 1)");
    obj2 = objectFromStr("(8 1 0)");
    fail_unless(objectCmp(obj1, obj2) < 0, 
		"8 0 1 should be less than 8 1 0");
    objectFree(obj1, TRUE);
    objectFree(obj2, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(objcmp2)
{
    Object *obj1;
    Object *obj2;

    obj1 = objectFromStr("423");
    obj2 = objectFromStr("'424'");
    fail_unless(objectCmp(obj1, obj2) < 0, 
		"423 should be less than '424'");
    objectFree(obj1, TRUE);
    objectFree(obj2, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(objcmp3)
{
    Object *obj1;
    Object *obj2;

    obj1 = objectFromStr("'423'");
    obj2 = objectFromStr("424");
    fail_unless(objectCmp(obj1, obj2) < 0, 
		"'423' should be less than 424");
    objectFree(obj1, TRUE);
    objectFree(obj2, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(objcmp4)
{
    Object *obj1;
    Object *obj2;

    obj1 = objectFromStr("423");
    obj2 = objectFromStr("'abc'");
    fail_unless(objectCmp(obj1, obj2) < 0, 
		"423 should be less than 'abc'");
    objectFree(obj1, TRUE);
    objectFree(obj2, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(objcmp5)
{
    Object *obj1;
    Object *obj2;
    int result;

    obj1 = objectFromStr("1");
    obj2 = objectFromStr("(1 2 3)");
    BEGIN {
	result = objectCmp(obj1, obj2);
	fail("An exception should have been raised");
    }
    EXCEPTION(e);
    WHEN_OTHERS {
	fail_unless(contains(e->text, 
			     "Cannot compare 1 with .1 2 3.",
			     "Comparison exception not raised"));
    }
    END;
    objectFree(obj1, TRUE);
    objectFree(obj2, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(split_str1)
{
    String *str1 = (String *) objectFromStr("'1.2.3'");
    String *str2 = (String *) objectFromStr("'.'");
    Object *obj1 = (Object *) stringSplit(str1, str2);
    Object *obj2 = objectFromStr("('1' '2' '3')");
    char *sexp = objectSexp(obj1);
    char *failstr = newstr("stringSplit failure: %s", sexp);

    fail_unless(objectCmp(obj1, obj2) == 0, failstr);
		
    skfree(failstr);
    skfree(sexp);
    objectFree((Object *) str1, TRUE);
    objectFree((Object *) str2, TRUE);
    objectFree((Object *) obj1, TRUE);
    objectFree((Object *) obj2, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST


START_TEST(split_str2)
{
    char *sexpstr;
    Object *version;
    char *str;
    char *failstr;

    initBuiltInSymbols();

    sexpstr = newstr("(split '1.2.3' '.')");
    version = evalSexp(sexpstr);
    skfree(sexpstr);

    str = objectSexp(version);
    objectFree(version, TRUE);

    failstr = newstr("split_str2: version is wrong: %s", str);

    fail_unless(streq(str, "('1' '2' '3')"), failstr);
    skfree(str);
    skfree(failstr);
    FREEMEMWITHCHECK;
}
END_TEST


START_TEST(version_chk1)
{
    char *sexpstr;
    Object *version1;
    Object *version2;

    initBuiltInSymbols();

    sexpstr = newstr("(version '8.44.17')");
    version1 = evalSexp(sexpstr);
    skfree(sexpstr);

    sexpstr = newstr("(version (quote '8.5.27'))");
    version2 = evalSexp(sexpstr);
    skfree(sexpstr);

    fail_unless(objectCmp(version1, version2) > 0,
	"version_chk1: version comparison failure");

    objectFree(version1, TRUE);
    objectFree(version2, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(quote_chk1)
{
    char *sexpstr;
    Object *obj1;
    char *str;

    initBuiltInSymbols();

    sexpstr = newstr("(quote '8.44.17')");
    obj1 = evalSexp(sexpstr);
    skfree(sexpstr);
    str = objectSexp(obj1);

    fail_unless(streq(str, "'8.44.17'"),
		"quote: failure with \"%s\"", str);

    skfree(str);
    objectFree(obj1, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST


START_TEST(promote)
{
    char *sexpstr;
    Object *obj;

    initBuiltInSymbols();

    sexpstr = newstr("(try-to-int '473')");
    obj = evalSexp(sexpstr);
    skfree(sexpstr);

    sexpstr = objectSexp(obj);
    fail_unless(streq(sexpstr, "473"), "Failed to promote integer string");
    skfree(sexpstr);
    objectFree(obj, TRUE);

    sexpstr = newstr("(try-to-int '473a')");
    obj = evalSexp(sexpstr);
    skfree(sexpstr);

    sexpstr = objectSexp(obj);
    fail_unless(streq(sexpstr, "'473a'"), "non-integer string");
    skfree(sexpstr);
    objectFree(obj, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

void
printSym(char *name)
{
    Object *obj = symbolGetValue(name);
    char *sexp = objectSexp(obj);

    fprintf(stderr, "Symbol %s = %s\n", name, sexp);
    skfree(sexp);
}

START_TEST(map)
{
    char *sexpstr;
    Object *obj;

    initBuiltInSymbols();

    sexpstr = newstr("(map try-to-int ('473' '67a' '42'))");
    obj = evalSexp(sexpstr);
    skfree(sexpstr);

    sexpstr = objectSexp(obj);
    fail_unless(streq("(473 '67a' 42)", sexpstr));
    skfree(sexpstr);
    objectFree(obj, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(setq)
{
    char *sexpstr;
    char *tmp;
    Object *obj;

    initBuiltInSymbols();
    sexpstr = newstr("dbver");
    obj = evalSexp(sexpstr);
    skfree(sexpstr);

    sexpstr = objectSexp(obj);
    fail_unless(streq("(8 1)", sexpstr), 
		tmp = newstr("Sexpstr is %s\n", sexpstr));
    skfree(tmp);
    skfree(sexpstr);

    objectFree(obj, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(regexp)
{
    String *source = stringNew("dbname = 'skit' port='5432'");
    Regexp *match = regexpNew("(port)");
    String *replacement = stringNew("\\1\\1\\1");
    String *result = regexpReplace(source, match, replacement);
    
    fail_unless(contains(result->value, "portportport='5432'"),
		"Rexgexp Failure");

    objectFree((Object *) replacement, TRUE);
    objectFree((Object *) source, TRUE);
    objectFree((Object *) match, TRUE);
    objectFree((Object *) result, TRUE);
    initBuiltInSymbols();
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(symbol_scope1)
{
    Symbol *wibble;
    Symbol *wibble2;
    String *tmp;
    initBuiltInSymbols();

    wibble = symbolNew("wibble");
    wibble2 = symbolNew("wibble2");
    symbolSet("wibble", (Object *) (tmp = stringNew("wubble")));
    symbolSet("wibble", (Object *) tmp);
    symbolSet("wibble", (Object *) (tmp = stringNew("wubble")));

    fail_unless(streq("wubble", ((String *) wibble->svalue)->value),
	"Initial symbol value for wibble is incorrect");

    fail_unless(wibble2->svalue == NULL,
	"wibble2 has unexepected value");

    newSymbolScope();
    setScopeForSymbol(wibble);
    setScopeForSymbol(wibble2);
    fail_unless(wibble->svalue == NULL,
	"wibble has unexepected value");

    symbolSet("wibble", (Object *) stringNew("wubble2"));

    fail_unless(streq("wubble2", ((String *) wibble->svalue)->value),
	"Scoped symbol value for wibble is incorrect");

    symbolSet("wibble2", (Object *) (tmp = stringNew("wibble2")));

    symbolSetRoot("wibble2", (Object *) (tmp = stringNew("wibble3")));

    fail_unless(streq("wibble2", ((String *) wibble2->svalue)->value),
	"Scoped symbol value for wibble2 is incorrect");

    dropSymbolScope();

    fail_unless(streq("wubble", ((String *) wibble->svalue)->value),
	"Final symbol value for wibble is incorrect");

    fail_unless(streq("wibble3", ((String *) wibble2->svalue)->value),
	"Final symbol value for wibble2 is incorrect");

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(symbol_scope2)
{
    Symbol *wibble;
    Symbol *wibble2;
    initBuiltInSymbols();

    wibble = symbolNew("wibble");
    wibble2 = symbolNew("wibble2");

    newSymbolScope();
    newSymbolScope();
    setScopeForSymbol(wibble);
    setScopeForSymbol(wibble2);

    symbolSet("wibble2", (Object *) stringNew("wubble2"));

    setScopeForSymbol(wibble2);

    fail_unless(streq("wubble2", ((String *) wibble2->svalue)->value),
	"Scoped symbol value for wibble2 is incorrect");

    symbolSet("wibble", (Object *) stringNew("wubble2"));

    dropSymbolScope();
    dropSymbolScope();

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(symbol_scope3)
{
    Symbol *wibble;
    Symbol *wibble2;
    initBuiltInSymbols();

    wibble2 = symbolNew("wibble2");
    setScopeForSymbol(wibble2);

    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(extract_from_list)
{
    char *sexpstr;
    Cons *list;
    Int4 *result;
    char *tmp;

    sexpstr = newstr("(1 2 3 4 5 6)");
    list = (Cons *) objectFromStr(sexpstr);

    result = (Int4 *) consNth(list, 0);
    fail_unless(result && (result->value == 1),
		tmp = newstr("extract_from_list: 1st element is incorrect (%d)",
			     result->value));
    skfree(tmp);

    result = (Int4 *) consNth(list, 1);
    fail_unless(result && (result->value == 2),
		tmp = newstr("extract_from_list: 2nd element is incorrect (%d)",
			     result->value));
    skfree(tmp);

    result = (Int4 *) consNth(list, 5);
    fail_unless(result && (result->value == 6),
		tmp = newstr("extract_from_list: 6th element is incorrect (%d)",
			     result->value));
    skfree(tmp);

    result = (Int4 *) consNth(list, 6);
    fail_if(result,
	    tmp = newstr("extract_from_list: 7th element has been returned"));
    skfree(tmp);

    objectFree((Object *) list, TRUE);
    skfree(sexpstr);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(regexp_object)
{
    Object *obj = objectFromStr("/12\\/3/");
    char *sexpstr = objectSexp(obj);
    char *tmp;
    fail_unless(streq("/12/3/", sexpstr), 
		tmp = newstr("Regexp string is invalid %s", sexpstr));

    skfree(sexpstr);
    skfree(tmp);
    objectFree(obj, TRUE);
    
    FREEMEMWITHCHECK;
}
END_TEST


Suite *
objects_suite(void)
{
    Suite *s = suite_create("Objects");

    /* Core test case */
    TCase *tc_core = tcase_create("Objects");

    ADD_TEST(tc_core, integer_object);
    ADD_TEST(tc_core, string_object1);
    ADD_TEST(tc_core, nil_object);
    ADD_TEST(tc_core, simple_list);
    ADD_TEST(tc_core, nil_string);
    ADD_TEST(tc_core, integer_list);
    ADD_TEST(tc_core, list_with_list);
    ADD_TEST(tc_core, list_with_nil);
    ADD_TEST(tc_core, alist);
    ADD_TEST(tc_core, simple_list2);
    ADD_TEST(tc_core, broken_list1);
    ADD_TEST(tc_core, broken_list2);
    ADD_TEST(tc_core, broken_list3);
    ADD_TEST(tc_core, broken_list4);
    ADD_TEST(tc_core, vector1);
    ADD_TEST(tc_core, hash1);
    ADD_TEST(tc_core, nil);
    ADD_TEST(tc_core, emptyhash);
    ADD_TEST(tc_core, is_alist1);
    ADD_TEST(tc_core, is_alist2);
    ADD_TEST(tc_core, not_alist1);
    ADD_TEST(tc_core, not_alist2);
    ADD_TEST(tc_core, string_list);
    ADD_TEST(tc_core, symbol_list);
    ADD_TEST(tc_core, objcmp1);
    ADD_TEST(tc_core, objcmp2);
    ADD_TEST(tc_core, objcmp3);
    ADD_TEST(tc_core, objcmp4);
    ADD_TEST(tc_core, objcmp5);
    ADD_TEST(tc_core, split_str1);
    ADD_TEST(tc_core, split_str2);
    ADD_TEST(tc_core, version_chk1);
    ADD_TEST(tc_core, promote);
    ADD_TEST(tc_core, map);
    ADD_TEST(tc_core, setq);
    ADD_TEST(tc_core, regexp);
    ADD_TEST(tc_core, quote_chk1);
    ADD_TEST(tc_core, symbol_scope1);
    ADD_TEST(tc_core, symbol_scope2);
    ADD_TEST(tc_core, symbol_scope3);
    ADD_TEST(tc_core, extract_from_list);
    ADD_TEST(tc_core, regexp_object);
    suite_add_tcase(s, tc_core);

    return s;
}

