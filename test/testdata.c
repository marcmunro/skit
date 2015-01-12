/**
 * @file   testdata.c
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
 * \endcode
 * @brief  
 * Provides test data and a database access layer to retrieve it
 *
 */


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "../src/skit.h"
#include "../src/exceptions.h"
#include "../src/sql.h"
#include "version.sql"
#include "database.sql"
#include "roles.sql"
#include "users.sql"
#include "tablespace.sql"
#include "domain.sql"
#include "constraint.sql"
#include "schema.sql"
#include "language.sql"
#include <check.h>
#include "suites.h"

static String *last_key = NULL;

static String *
despacedString(String *str)
{
    char *src = str->value;
    char *result = skalloc(strlen(src) + 1);
    char *targ = result;
    char c;

    while (c = *src++) {
	if (!isspace(c)) {
	    *targ++ = c;
	}
    }
    *targ = '\0';
    return stringNewByRef(result);
}

static void
addQuery(Hash *hash, Cons *cons)
{
    String *orig = (String *) cons->car;
    String *key = despacedString(orig);
    Cons *contents = (Cons *) cons->cdr;
    objectFree((Object *) cons, FALSE);
    hashAdd(hash, (Object *) key, (Object *) contents);
    last_key = key;
    //printSexp(stderr, "LAST: ", last_key);
    objectFree((Object *) orig, TRUE);
}

static Hash *
initQueries()
{
    Hash *hash = hashNew(TRUE);
    Object *obj = objectFromStr(VERSION_QRY);
    addQuery(hash, (Cons *) obj);
    addQuery(hash, (Cons *) objectFromStr(DATABASE_QRY));
    addQuery(hash, (Cons *) objectFromStr(ROLES_QRY));
    addQuery(hash, (Cons *) objectFromStr(USERS_QRY));
    addQuery(hash, (Cons *) objectFromStr(TABLESPACE_QRY));
    addQuery(hash, (Cons *) objectFromStr(DOMAIN_QRY));
    addQuery(hash, (Cons *) objectFromStr(CONSTRAINT_QRY));
    addQuery(hash, (Cons *) objectFromStr(SCHEMA_QRY));
    addQuery(hash, (Cons *) objectFromStr(LANGUAGE_QRY));

    return hash;
}

static String replace1 = {OBJ_STRING, "\\2"};

#define MATCH "[ \t=]*['\"]([^'\"]*)" 
#define HOST_MATCH ".*(hostaddr" MATCH ").*"
#define PORT_MATCH ".*(port" MATCH ").*"
#define DBNAME_MATCH ".*(dbname" MATCH ").*"
#define USER_MATCH ".*(username" MATCH ").*"
#define PASSWD_MATCH ".*(password" MATCH ").*"

static String *
get_param(String *connect,
	  char *symbol_name, 
	  char *regexp_str,
	  boolean *from_params)
{
    boolean is_new = FALSE;
    String *result= (String *) symbolGetValueWithStatus(symbol_name, &is_new);
    Regexp *match;
    
    if (connect && !result) {
	/* If there is a connect string but no result information,
	 * attempt to retrieve the result information from the connect
	 * string */
	match = regexpNew(regexp_str);		 
	result = regexpReplaceOnly(connect, match, &replace1);
	if (result->value[0] == '\0') {
	    /* String is empty */
	    objectFree((Object *) result, TRUE);
	    result = NULL;
	}
	objectFree((Object *) match, TRUE);
    }
    else {
	/* Every result from this function is it's own value not
	 * referenced elsewhere, and should be freed appropriately */
	result = (String *) objectCopy((Object *) result);
    }
    if (is_new) {
	*from_params = TRUE;
    }
    return result;
}

static void
record_param(String *value, char *name, boolean make_global)
{
	Symbol *sym = symbolGet(name);

	if (!sym) {
		sym = symbolNew(name);
	}
	if (!make_global) {
		setScopeForSymbol(sym);
	}
	symSet(sym, (Object *) value);
}

static Connection *
testConnect(Object *sqlfuncs)
{
    /* If there is no existing connection then we must create a new
     * connection.
     * If new connection information has been provided, then we must
     * create a new connection.
     * The first connection we create will be made global.
     */

    Connection *connection = (Connection *) symbolGetValue("dbconnection");
    boolean make_global;
    boolean new_connection;
    String *connect;
    String *host;
    String *port;
    String *dbname;
    String *user;
    String *pass;
    Symbol *sym;

    new_connection = make_global = (connection == NULL);

    connect = (String *) symbolGetValueWithStatus("connect", &new_connection);
    host = get_param(connect, "host", HOST_MATCH, &new_connection);
    port = get_param(connect, "port", PORT_MATCH, &new_connection);
    dbname = get_param(connect, "dbname", DBNAME_MATCH, &new_connection);
    user = get_param(connect, "username", USER_MATCH, &new_connection);
    pass = get_param(connect, "password", PASSWD_MATCH, &new_connection);

    if (new_connection) {
	record_param(host, "host", make_global);
	record_param(port, "port", make_global);
	record_param(dbname, "dbname", make_global);
	record_param(user, "username", make_global);
	record_param(pass, "password", make_global);
	
	record_param(connect, "connect", make_global);

	connection = (Connection *) skalloc(sizeof(Connection));
	connection->type = OBJ_CONNECTION;
	connection->sqlfuncs = sqlfuncs;
	connection->dbtype = stringNew("pgtest");
	connection->conn = (void *) connection;
	sym = symbolNew("dbconnection");
	symSet(sym,  (Object *) connection);
    }
    return connection;
}

static Hash *query_hash = NULL;

static void
compare(String *str1, String *str2)
{
    char *s1 = str1->value;
    char *s2 = str2->value;
    char c;
    while (c = *s1++) {
	if (c == *s2++) {
	    fprintf(stderr, "%c", c);
	}
	else {
	    //fprintf(stderr, "\nSTR1: %s\nSTR2: %s\n", s1, s2);
	    break;
	}
    }
    //fprintf(stderr, "\nSTR1: %s\nSTR2: %s\n", s1, s2);
}

static Cursor *
testExecQry(Connection *connection, 
	    String *qry,
	    Object *params)
{
    Cons *results;
    Cursor *curs = NULL;
    Vector *fields;
    Vector *rows;
    String *key = despacedString(qry);
    UNUSED(params);

    if (!query_hash) {
	query_hash = initQueries();
    }

    if (results = (Cons *) hashGet(query_hash, (Object *) key)) {
	curs = (Cursor *) skalloc(sizeof(Cursor));
	curs->type = OBJ_CURSOR;
	curs->cursor = (void *) results;
	fields = (Vector *) results->car;
	rows = (Vector *) ((Cons *) results->cdr)->car;
	curs->rows = rows->elems;
	curs->cols = fields->elems;
	curs->fields = NULL;
	curs->cursor = results;
	curs->connection = connection;
	curs->tuple.type = OBJ_TUPLE;
	curs->tuple.cursor = curs;
	curs->tuple.dynamic = FALSE;
	curs->tuple.rownum = 0;
	curs->querystr = stringNew(qry->value);
    }
    else {
	compare(last_key, key);
	objectFree((Object *) key, TRUE);
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Query not defined: %s", qry->value));
    }

    objectFree((Object *) key, TRUE);
    return curs;

}

static Object *
testFieldByIdx(Tuple *tuple, int col)
{
    Cursor *curs = tuple->cursor;
    Cons *results = (Cons *) curs->cursor;
    Vector *rows = (Vector *) ((Cons *) results->cdr)->car;
    Vector *result_row;
    int row = tuple->rownum - 1;
    String *result;

    result_row = (Vector *) rows->contents->vector[row];
    if (result = (String *) result_row->contents->vector[col]) {
	return (Object *) stringNew(result->value);
    }
    return NULL;
}

static Object *
testFieldByName(Tuple *tuple, String *name)
{
    Cursor *curs = tuple->cursor;
    Cons *results = (Cons *) curs->cursor;
    Vector *fields = (Vector *) results->car;
    int col;

    for (col = fields->elems - 1; col >= 0; col--) {
	if (stringCmp(name, (String *) fields->contents->vector[col]) == 0) {
	    return testFieldByIdx(tuple, col);
	}
    }
    return NULL;
}

static Tuple *
testNextRow(Cursor *cursor)
{
    if (cursor->tuple.rownum < cursor->rows) {
	cursor->tuple.rownum++;
	return &(cursor->tuple);
    }
    return NULL;
}

static char *
testCursorStr(Cursor *cursor)
{
    char *tmp = objectSexp((Object *) cursor->cursor);
    char *query = objectSexp((Object *) cursor->querystr);
    char *result = newstr("<#OBJ_CURSOR# %s\n%s>", query, tmp);
    skfree(query);
    skfree(tmp);
    return result;
}

static char *
testTupleStr(Tuple *tuple)
{
    Cursor *cursor = tuple->cursor;
    Cons *results = (Cons *) cursor->cursor;
    Vector *fields = (Vector *) results->car;
    String *name;
    String *value;
    int col;
    char *result = newstr("");
    char *tmp;
    for (col = 0; col < cursor->cols; col++) {
	name = (String *) fields->contents->vector[col];
	value = (String *) testFieldByIdx(tuple, col);
	tmp = result;
	if (value) {
	    result = newstr("%s ('%s' . '%s')", tmp, name->value, value->value);
	    objectFree((Object *) value, TRUE);
	}
	else {
	    result = newstr("%s ('%s')", tmp, name->value);
	}
	skfree(tmp);
    }
    tmp = result;
    result = newstr("<#%s# (%s)>", objTypeName((Object *) tuple), tmp);
    skfree(tmp);
    return result;
}

static void
testFreeCursor(Cursor *cursor)
{
    objectFree((Object *) cursor->querystr, TRUE);
    skfree((void *) cursor);
}

static void
testCleanup(Connection *conn)
{
    objectFree((Object *) query_hash, TRUE);
    query_hash = NULL;
    objectFree((Object *) conn->dbtype, TRUE);
    skfree(conn);
}

void
registerTestSQL()
{
    static SqlFuncs funcs = {
	OBJ_MISC,
	&testConnect,
	&testExecQry,
	&testNextRow,
	&testFieldByIdx,
	&testFieldByName,
	&testTupleStr,
	&testCursorStr,
	NULL,
	NULL,
	NULL,
	&testFreeCursor,
	&testCleanup
    };
    
    ObjReference *obj = objRefNew((Object *) &funcs);
    Hash *dbhash = (Hash *) symbolGetValue("dbhandlers");
    String *handlername = stringNew("pgtest");
    hashAdd(dbhash, (Object *) handlername, (Object *) obj);
}

