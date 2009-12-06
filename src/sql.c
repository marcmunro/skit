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

boolean
checkDbtypeIsRegistered(String *dbtype)
{
    Hash *dbhash = (Hash *) symbolGet("dbhandlers")->svalue;
    SqlFuncs *functions;

    functions = (SqlFuncs *) dereference(
	hashGet(dbhash, (Object *) dbtype));
    return functions != NULL;
}

static SqlFuncs *
getFunctions()
{
   static Hash *dbhash = NULL;
    String *dbtype;
    SqlFuncs *functions;

    if (!dbhash) {
	dbhash = (Hash *) symbolGet("dbhandlers")->svalue;
    }

    dbtype = (String *) symbolGetValue("dbtype");
    functions = (SqlFuncs *) dereference(hashGet(dbhash, (Object *) dbtype));

    if (!functions) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("No handler for database type %s", dbtype->value));
    }
    return functions;
}

/* Called to establish a connection, or get the current connection. */
Connection *
sqlConnect()
{
    SqlFuncs *functions;

    if (!cur_connection) {
	functions = getFunctions();

	if (!functions->connect) {
	    RAISE(NOT_IMPLEMENTED_ERROR,
		  newstr("Db connection function is not registered"));
	}
	cur_connection = functions->connect((Object *) functions);
    }
    if (cur_connection) {
	return cur_connection;
    }
			 
    RAISE(PARAMETER_ERROR, newstr("Unhandled database type"));
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

Tuple *
cursorGet(Cursor *cursor, Object *key)
{
    Connection *connection;
    SqlFuncs *functions;
    connection = cursor->connection;
    functions = (SqlFuncs *) connection->sqlfuncs;
    if (!functions->cursorget) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db cursorget function is not registered"));
    }
    return functions->cursorget(cursor, key);

}

void *
cursorIndex(Cursor *cursor, String *fieldname)
{
    Connection *connection;
    SqlFuncs *functions;
    connection = cursor->connection;
    functions = (SqlFuncs *) connection->sqlfuncs;
    if (!functions->cursorindex) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db cursorindex function is not registered"));
    }
    functions->cursorindex(cursor, fieldname);
}

static Object *
nextParam(Object **params)
{
    Object *result = NULL;
    Object *placeholder = NULL;
    return NULL;
    if (params && *params) {
	dbgSexp(*params);
	if (isCollection(*params)) {
	    fprintf(stderr, "YES\n");
	    result =  objNext(*params, &placeholder);
	}
	else {
	    fprintf(stderr, "NO\n");
	    result = *params;
	    *params = NULL;
	}
    }
    return result;
}

char *
applyOneParam(char *qrystr, char *pattern, Object *param)
{
    String *source = NULL;
    String *result = NULL;
    String *replacement = NULL;
    Regexp *match = NULL;
    char *raw_result;
    if (param->type == OBJ_STRING) {
	replacement = (String *) objectCopy(param);
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("applyOneParam cannot yet deal with non-strings"));
    }
    BEGIN {
	source = stringNewByRef(qrystr);
	match = regexpNew(pattern);
	result = regexpReplace(source, match, replacement);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) source, FALSE);
	objectFree((Object *) match, TRUE);
	objectFree((Object *) replacement, TRUE);
    }
    END;
    raw_result = result->value;
    objectFree((Object *) result, FALSE);  /* Free the string object but
					    * not its contents */
    return raw_result;
}

char *applyNthParam(char *qrystr, int n, Object *param)
{
    static char *param_str = NULL;
    if (!param_str) {
	param_str = malloc(10);
    }
    if (param->type != OBJ_STRING) {
	dbgSexp(param);
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("applyParams cannot deal with non-string objects (%d)",
		  param->type));
    }
    sprintf(param_str, ":%d", n);
    return applyOneParam(qrystr, param_str, param);
}

char *
applyParams(char *qrystr, Object *params)
{
    char *result;
    char *prev;
    Cons *list;
    int i;
    char *param_str = NULL;
    if (params->type == OBJ_CONS) {
	list = (Cons *) params;
	i = 1;
	prev = NULL;
	result = qrystr;
	while (list) {
	    result = applyNthParam(result, i, list->car);
	    if (prev) {
		skfree(prev);
	    }
	    prev = result;
	    i++;
	    list = (Cons *) list->cdr;
	}
	return result;
    }
    return applyNthParam(qrystr, 1, params);
}

String *
sqlDBQuote(String *first, String *second)
{
    SqlFuncs *functions = getFunctions();

    if (!functions->cursorstr) {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Db cursorstr function is not registered"));
    }
    return functions->dbquote(first, second);
}
