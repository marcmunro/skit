/**
 * @file   sql.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * 
 *
 */

#include <stdio.h>
#include "skit_lib.h"
#include "sql.h"
#include "exceptions.h"


static String empty_str = {OBJ_STRING, ""};
static String dbtype_str = {OBJ_STRING, "dbtype"};

/* Remove SQL comments from text */
String *
trimSqlText(String *text)
{
    Regexp *replace = regexpNew("--.*$");
    String *result;
    result =  regexpReplace(text, replace, &empty_str);
    objectFree((Object *) replace, TRUE);
    return result;
}

static Connection *cur_connection = NULL;

/* Called to establish a connection, or get the current connection. */
Connection *
sqlConnect()
{
    static Hash *dbhash = NULL;
    boolean ignore;
    String *dbtype;
    SqlFuncs *functions;

    if (!dbhash) {
	dbhash = (Hash *) symbolGet("dbhandlers")->value;
    }

    if (!cur_connection) {
	dbtype = (String *) symbolGetValue("dbtype");
	functions = (SqlFuncs *) dereference(
	    hashGet(dbhash, (Object *) dbtype));

	if (!functions->connect) {
	    RAISE(NOT_IMPLEMENTED_ERROR,
		  newstr("Db connection function is not registered"));
	}
	cur_connection = functions->connect((Object *) functions);
    }
    if (cur_connection) {
	return cur_connection;
    }
			 
    skitFail("Unhandled database type");
}

/* Called after executing an action.  This tells us that the next time
 * sqlConnect is called, it will have to figure out what connection to
 * use, and maybe establish a new one.
 */
void
finishWithConnection()
{
    cur_connection = NULL;
}

void
connectionFree(Connection *connection)
{
    SqlFuncs *functions = (SqlFuncs *) connection->sqlfuncs;
    if (!functions->cleanup) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db cleanup function is not registered"));
    }
    functions->cleanup(connection);
}

void
cursorFree(Cursor *cursor)
{
    Connection *connection = cursor->connection;
    SqlFuncs *functions = (SqlFuncs *) connection->sqlfuncs;
    if (!functions->closecursor) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db closecursor function is not registered"));
    }
    functions->closecursor(cursor);
}

Cursor *
sqlExec(Connection *connection, 
	String *qry,
	Object *params)
{
    boolean ignore;
    String *dbtype;
    SqlFuncs *functions = (SqlFuncs *) connection->sqlfuncs;

    dbtype = (String *) symbolGetValue("dbtype");
    if (!functions->query) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db query function is not registered"));
    }
    return functions->query(connection, qry, params);
}

/* Does not need to be freed */
Tuple *
sqlNextRow(Cursor *cursor)
{
    Connection *connection = cursor->connection;
    SqlFuncs *functions = (SqlFuncs *) connection->sqlfuncs;
    if (!functions->nextrow) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db nextrow function is not registered"));
    }
    return functions->nextrow(cursor);
}

static String *
tupleGetByIdx(Tuple *tuple, int idx)
{
    Cursor *cursor;
    Connection *connection;
    SqlFuncs *functions;
    tuple = (Tuple *) dereference((Object *) tuple);
    cursor = (Cursor *) tuple->cursor;
    connection = cursor->connection;
    functions = (SqlFuncs *) connection->sqlfuncs;

    if (!functions->fieldbyidx) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db fieldbyidx function is not registered"));
    }
    return (String *) functions->fieldbyidx(tuple, idx);
}

/* Needs to be freed */
static String *
tupleGetByName(Tuple *tuple, String *name)
{
    Cursor *cursor;
    Connection *connection;
    SqlFuncs *functions;
    tuple = (Tuple *) dereference((Object *) tuple);
    cursor = (Cursor *) tuple->cursor;
    connection = cursor->connection;
    functions = (SqlFuncs *) connection->sqlfuncs;

    if (!functions->fieldbyname) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db fieldbyname function is not registered"));
    }
    return (String *) functions->fieldbyname(tuple, name);
}

String *
tupleGet(Tuple *tuple, Object *key)
{
    if (key->type == OBJ_INT4) {
	return tupleGetByIdx(tuple, ((Int4 *) key)->value);
    }
    return tupleGetByName(tuple, (String *) key);
}

static void
sqlResetCursor(Cursor *cursor)
{
    cursor = (Cursor *) dereference((Object *) cursor);
    // TODO: Assert that this is a cursor

    cursor->rownum = 0;
}

Object *
cursorNext(Cursor *cursor, Object **p_placeholder)
{
    Tuple *tuple;
    if (!(*p_placeholder)) {
	sqlResetCursor(cursor);
	*p_placeholder = (Object *) objRefNew((Object *) cursor);
    }
    tuple = sqlNextRow(cursor);
    if (!tuple) {
	objectFree(*p_placeholder, TRUE);
	*p_placeholder = NULL;
    }
    return (Object *) tuple;
}

char *
tupleStr(Tuple *tuple)
{
    Cursor *cursor;
    Connection *connection;
    SqlFuncs *functions;
    tuple = (Tuple *) dereference((Object *) tuple);
    cursor = (Cursor *) tuple->cursor;
    connection = cursor->connection;
    functions = (SqlFuncs *) connection->sqlfuncs;
    if (!functions->tuplestr) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db tuplestr function is not registered"));
    }
    return functions->tuplestr(tuple);
}

char *
cursorStr(Cursor *cursor)
{
    Connection *connection;
    SqlFuncs *functions;
    connection = cursor->connection;
    functions = (SqlFuncs *) connection->sqlfuncs;
    if (!functions->cursorstr) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db cursorstr function is not registered"));
    }
    return functions->cursorstr(cursor);
}
