/**
 * @file   check_filepath.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Unit tests for filepath handling
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "../src/skit_lib.h"
#include "suites.h"


static Object *
evalStr(char *str)
{
    char *tmp = newstr(str);
    Object *obj = objectFromStr(tmp);
    skfree(tmp);
    return obj;
}

static char *
find_in_8_2(char *name)
{
    Vector *roots = (Vector *) evalStr("['test']");
    String *template = (String *) evalStr("'templates'");
    String *dbtype = (String *) evalStr("'postgres'");
    Object *dbver = evalStr("(8 2)");
    String *sname = stringNew(name);
    char *result;

    result = pathToFile(roots, template, dbtype, dbver, sname);
    objectFree((Object *) sname, TRUE);
    objectFree((Object *) roots, TRUE);
    objectFree((Object *) dbtype, TRUE);
    objectFree((Object *) dbver, TRUE);
    objectFree((Object *) template, TRUE);
    return result;
}


START_TEST(find_x)
{
    char *tmp;
    char *result;
    initBuiltInSymbols();

    result = find_in_8_2("root");
    fail_unless(streq("test/templates/root", result), 
		tmp = newstr("Filepath for root is %s\n", result));
    skfree(tmp);
    skfree(result);

    result = find_in_8_2("db");
    fail_unless(streq("test/templates/postgres/db", result), 
		tmp = newstr("Filepath for db is %s\n", result));
    skfree(tmp);
    skfree(result);

    result = find_in_8_2("ver");
    fail_unless(streq("test/templates/postgres/8.2/ver", result), 
		tmp = newstr("Filepath for ver is %s\n", result));
    skfree(tmp);
    skfree(result);

    result = find_in_8_2("xyzzy");
    fail_unless(result == NULL, "Filepath found for xyzzy\n");

    FREEMEMWITHCHECK;
}
END_TEST


Suite *
filepaths_suite(void)
{
    Suite *s = suite_create("Filepaths");

    /* Core test case */
    TCase *tc_core = tcase_create("Filepaths");
    ADD_TEST(tc_core, find_x);
    suite_add_tcase(s, tc_core);

    return s;
}

