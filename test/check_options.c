/**
 * @file   check_options.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Unit tests for command line option handling.
 */


#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "../src/skit_lib.h"
#include "suites.h"

START_TEST(option_keys)
{
    // Test that a string providing an option name is correctly 
    // converted into a list of keys for that option.
    String *option_name;
    Cons *option_list;
    char *str;

    option_name = (String *) objectFromStr("'opt*ion'");
    option_list = optionKeyList(option_name);
    str = objectSexp((Object *) option_list);

    fail_unless(streq(str, "('option' 'optio' 'opti' 'opt')"),
		"Incorrect value for option_list: %s", str);
    skfree(str);
    objectFree((Object *) option_name, TRUE);
    objectFree((Object *) option_list, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(option_keys2)
{
    // Test that a string providing an option name is correctly 
    // converted into a list of keys for that option.
    String *option_name = (String *) objectFromStr("'option'");
    Cons *option_list = optionKeyList(option_name);
    char *str = objectSexp((Object *) option_list);

    fail_unless(streq(str, "('option')"),
		"Incorrect value for option_list(2): %s", str);
    skfree(str);
    objectFree((Object *) option_name, TRUE);
    objectFree((Object *) option_list, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(option_keys3)
{
    // Test that a string providing an option name is correctly 
    // converted into a list of keys for that option.
    String *option_name = (String *) objectFromStr("'opt*io*n'");
    Cons *option_list = optionKeyList(option_name);
    char *str = objectSexp((Object *) option_list);

    fail_unless(streq(str, "('optio*n' 'optio*' 'optio' 'opti' 'opt')"),
		"Incorrect value for option_list: %s", str);
    skfree(str);
    objectFree((Object *) option_name, TRUE);
    objectFree((Object *) option_list, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(create_options)
{
    // Simply test that creation and destruction appears to go according
    // to plan.
    Hash *core_options = coreOptionHash();
    freeOptions();
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(get_option1)
{
    // Simply test that creation and destruction appears to go according
    // to plan.
    Hash *option_hash = coreOptionHash();
    String *param = stringNew("con");
    String *option = (String *) hashGet(option_hash, (Object *) param);
    fail_if(option == NULL, "Option not found");
    fail_unless(option->type == OBJ_STRING, "Incorrect type for option");
    fail_unless(streq(option->value, "connect"), 
		"Incorrect option value: \"%s\"", option->value);
    objectFree((Object *) param, TRUE);
    freeOptions();
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(get_option2)
{
    // Simply test that creation and destruction appears to go according
    // to plan.
    Hash *option_hash = coreOptionHash();
    String *param = stringNew("fu");
    String *option = (String *) hashGet(option_hash, (Object *) param);
    fail_if(option == NULL, "Option not found");
    fail_unless(option->type == OBJ_STRING, "Incorrect type for option(2)");
    fail_unless(streq(option->value, "printfull"), 
		"Incorrect option value(2): \"%s\"", option->value);
    objectFree((Object *) param, TRUE);
    freeOptions();
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(get_option3)
{
    // Simply test that creation and destruction appears to go according
    // to plan.
    Hash *option_hash = coreOptionHash();
    String *param = stringNew("n");
    String *option = (String *) hashGet(option_hash, (Object *) param);
    fail_if(option == NULL, "Option not found");
    fail_unless(option->type == OBJ_STRING, "Incorrect type for option(3)");
    fail_unless(streq(option->value, "generate"), 
		"Incorrect option value(3): \"%s\"", option->value);
    objectFree((Object *) param, TRUE);
    freeOptions();
    FREEMEMWITHCHECK;
}
END_TEST

START_TEST(get_option4)
{
    // Simply test that creation and destruction appears to go according
    // to plan.
    Hash *option_hash = coreOptionHash();
    String *param = stringNew("diff");
    String *option = (String *) hashGet(option_hash, (Object *) param);
    fail_if(option == NULL, "Option not found");
    fail_unless(option->type == OBJ_STRING, "Incorrect type for option(4)");
    fail_unless(streq(option->value, "diff"), 
		"Incorrect option value(4): \"%s\"", option->value);
    objectFree((Object *) param, TRUE);
    freeOptions();
    FREEMEMWITHCHECK;
}
END_TEST


Suite *
options_suite(void)
{
    Suite *s = suite_create("Options");

    /* Core test case */
    TCase *tc_core = tcase_create("Options");
    ADD_TEST(tc_core, option_keys);
    ADD_TEST(tc_core, option_keys2);
    ADD_TEST(tc_core, option_keys3);
    ADD_TEST(tc_core, create_options);
    ADD_TEST(tc_core, get_option1);
    ADD_TEST(tc_core, get_option2);
    ADD_TEST(tc_core, get_option3);
    ADD_TEST(tc_core, get_option4);
    suite_add_tcase(s, tc_core);

    return s;
}


