/**
 * @file   check_xmlfile.c
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
 * \endcode
 * @brief  
 * Unit tests for xmlfile handling
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"

START_TEST(optiontest)
{
    String *filename;
    String *path;
    String *key;
    String *field;
    Cons *options;
    Object *obj;
    char *tmp;
    Document *doc;

    initTemplatePath("test/");
    filename = stringNew(tmp = newstr("add_deps.xml"));
    path = findFile(filename);
    objectFree((Object *) filename, TRUE);
    skfree(tmp);

    doc = docFromFile(path);
    options = doc->options;
    objectFree((Object *) path, TRUE);

    obj = optionlistGetOptionValue(options, 
				   key = stringNew("sources"),
				   field = stringNew("value"));

    objectFree((Object *) key, TRUE);
    objectFree((Object *) field, TRUE);

    fail_unless((obj->type == OBJ_INT4) && (((Int4 *) obj)->value == 1), 
	"Incorrect sources value: %s\n", tmp = objectSexp(obj));
    skfree(tmp);
		
    obj = optionlistGetOptionValue(options, 
				   key = stringNew("g"),
				   field = stringNew("type"));
    objectFree((Object *) key, TRUE);
    objectFree((Object *) field, TRUE);
    fail_unless((obj->type == OBJ_STRING) && 
		streq(((String *) obj)->value, "flag"),
		"Incorrect grants type: %s\n", tmp = objectSexp(obj));
    skfree(tmp);
		
    obj = optionlistGetOptionValue(options, 
				   key = stringNew("gr"),
				   field = stringNew("default"));
    objectFree((Object *) key, TRUE);
    objectFree((Object *) field, TRUE);
    fail_unless(obj == NULL,
		"Incorrect grants default: %s\n", tmp = objectSexp(obj));
    skfree(tmp);
		
    obj = optionlistGetOptionValue(options, 
				   key = stringNew("full"),
				   field = stringNew("type"));
    objectFree((Object *) key, TRUE);
    objectFree((Object *) field, TRUE);

    fail_unless((obj->type == OBJ_STRING) && 
		streq(((String *) obj)->value, "flag"),
		"Incorrect details type: %s\n", tmp = objectSexp(obj));
    skfree(tmp);

    obj = optionlistGetOptionValue(options, 
				   key = stringNew("det"),
				   field = stringNew("default"));
    objectFree((Object *) key, TRUE);
    objectFree((Object *) field, TRUE);
    fail_unless(obj == NULL,
		"Incorrect details default: %s\n", tmp = objectSexp(obj));
    skfree(tmp);
		
    //fprintf(stderr, "Options: %s\n", tmp = objectSexp((Object *) options));
    //skfree(tmp);
    //printSexp(stderr, "OPTIONLIST: ", (Object *) options);
    objectFree((Object *) doc, TRUE);

    FREEMEMWITHCHECK;
}
END_TEST


/* Test the docFromFile function.
 *
 */
START_TEST(docfromfile)
{
    String *filename;
    String *path;
    char *tmp;
    Document *doc;

    initTemplatePath("test/");

    filename = stringNew(tmp = newstr("include.xml"));
    path = findFile(filename);
    objectFree((Object *) filename, TRUE);
    skfree(tmp);

    doc = docFromFile(path); 
    //printSexp(stderr, "OPTIONS: ", (Object *) doc->options);
    finishDocument(doc);

    //printSexp(stderr, "DOC: ", (Object *) doc);
    objectFree((Object *) doc, TRUE);

    objectFree((Object *) path, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST

/* Test the processInFile function.
 *
 */
START_TEST(adddeps)
{
    String *filename = stringNew("test/testfiles/x.xml");
    Document *doc;
    initTemplatePath("test/");

    loadInFile(filename);
    doc = docStackPop();
    //dbgSexp(doc);
    
    objectFree((Object *) doc, TRUE);
    objectFree((Object *) filename, TRUE);
    FREEMEMWITHCHECK;
}
END_TEST


Suite *
xmlfile_suite(void)
{
    Suite *s = suite_create("xmlfile");

    /* Core test case */
    TCase *tc_core = tcase_create("xmlfile");
    ADD_TEST(tc_core, optiontest);
    ADD_TEST(tc_core, docfromfile);  // NEXT THING TO DO!
    ADD_TEST(tc_core, adddeps);
    suite_add_tcase(s, tc_core);

    return s;
}

