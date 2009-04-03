/**
 * @file   pgsql.c
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
#include <libpq-fe.h>
#include "skit_lib.h"
#include "exceptions.h"
#include "sql.h"

static String replace1 = {OBJ_STRING, "\\2"};

#define MATCH "[ \t=]*['\"]([^'\"]*)" 
#define HOST_MATCH ".*(hostaddr" MATCH ").*"
#define PORT_MATCH ".*(port" MATCH ").*"
#define DBNAME_MATCH ".*(dbname" MATCH ").*"
#define USER_MATCH ".*(username" MATCH ").*"
#define PASSWD_MATCH ".*(password" MATCH ").*"

static void
pgsqlConnectionCheck(PGconn *conn)
{
	if (PQstatus(conn) != CONNECTION_OK) { 
		RAISE(SQL_ERROR, 
			  newstr("libpq error: PQstatus = %d", PQstatus(conn)));
	}
}

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
	setScopeForSymbol(sym);
	symbolSet(name, (Object *) value);
	if (make_global && sym->scope) {
		/* We have to also set the default value for this scoped
		 * variable. */
		value = (String *) objectCopy((Object *) value);
		symbolSetRoot(name, (Object *) value);
	}
}

// TODO: Refactor this.  The objectCopy of connect below is a hack and
// should not be required.  The whole thing feels horribly fragile.
// Note that ../test/testdata.c is similar.
static Connection *
pgsqlConnect(Object *sqlfuncs)
{
	boolean is_new = FALSE;
	boolean make_global = FALSE;
	Connection *connection = (Connection *) symbolGetValue("dbconnection");
    String *connect = (String *) symbolGetValueWithStatus("connect", &is_new);
	String *host = get_param(connect, "host", HOST_MATCH, &is_new);
    String *port = get_param(connect, "port", PORT_MATCH, &is_new);
    String *dbname = get_param(connect, "dbname", DBNAME_MATCH, &is_new);
    String *user = get_param(connect, "username", USER_MATCH, &is_new);
    String *pass = get_param(connect, "password", PASSWD_MATCH, &is_new);
	Symbol *sym;

	if (is_new) {
		/* If any string above has been retrieved from params rather
		 * than the symbol table, then we have new database connection
		 * information for this action, and so a new connection should
		 * be made.  If there is no existing connection, we will define all
		 * variables globally, otherwise we will define them only within
		 * the scope of the current action. */
		make_global = (connection == NULL);
		record_param((String *) objectCopy((Object *) connect), 
					 "connect", make_global);
		record_param(host, "host", make_global);
		record_param(port, "port", make_global);
		record_param(dbname, "dbname", make_global);
		record_param(user, "username", make_global);
		record_param(pass, "password", make_global);
	}

	if (!connection) {
		connection = (Connection *) skalloc(sizeof(Connection));
		connection->type = OBJ_CONNECTION;
		connection->sqlfuncs = sqlfuncs;
		connection->dbtype = stringNew("postgres");
		connection->conn = NULL;
		//fprintf(stderr, "Connecting to database type \"postgres\""
		//				 " with \"%s\"\n", connect->value);
		BEGIN {
			connection->conn = (void *) PQconnectdb(connect->value);
			pgsqlConnectionCheck((PGconn *) connection->conn);
		}
		EXCEPTION(ex) {
			objectFree((Object *) connection, TRUE);
		}
		WHEN(SQL_ERROR) {
			RAISE(SQL_ERROR,
				  newstr("Cannot connect to database type \"postgres\""
						 " with \"%s\"\n  %s", connect->value, ex->text));
		}
		END;
		sym = symbolNew("dbconnection");
		sym->svalue = (Object *) connection;
	}
	return connection;
}

static void
pgsqlCleanup(Connection *connection)
{
	if (connection->conn) {
		PQfinish(connection->conn);
	}
	connection->conn = NULL;
	objectFree((Object *) connection->dbtype, TRUE);
	skfree(connection);
}

static PGconn *
pgConn(Connection *connection)
{
	if (!connection) {
		return NULL;
	}
	if (connection->type == OBJ_OBJ_REFERENCE) {
		return pgConn((Connection *) ((ObjReference *) connection)->obj);
	}
	return (PGconn *) connection->conn;
}

static void
pgResultCheck(PGresult *result)
{
	ExecStatusType status = PQresultStatus(result);
	if ((status != PGRES_COMMAND_OK) &&	(status != PGRES_TUPLES_OK)) {
		RAISE(SQL_ERROR,  
			  newstr("Postgres error: %s\n%s", PQresStatus(status),
				  PQresultErrorMessage(result)));
	}
}

static int counter = 0;

static Cursor *
pgsqlExecQry(Connection *connection, 
			 String *qry,
			 Object *params)
{
	PGconn *conn = pgConn(connection);
	char *querystr = qry->value;
	PGresult *result;
	Cursor *curs;

	if (result = PQexec(conn, querystr)) {
		pgResultCheck(result);
		curs = (Cursor *) skalloc(sizeof(Cursor));
		curs->type = OBJ_CURSOR;
		curs->cursor = (void *) result;
		curs->rows = PQntuples(result);
		curs->cols = PQnfields(result);
		curs->rownum = 0;
		curs->fields = NULL;
		curs->tuple.type = OBJ_TUPLE;
		curs->tuple.cursor = curs;
		curs->connection = connection;
		curs->querystr = stringNew(qry->value);
		return curs;
	}
	RAISE(SQL_ERROR, 
		  newstr("Fatal postgres error: %s", PQresultErrorMessage(NULL)));
}

static Object *
pgsqlFieldByIdx(Tuple *tuple, int col)
{
	Cursor *cursor = tuple->cursor;
	int row;
	int  is_binary;
	char *result;

	if (!cursor->rownum) {
		RAISE(SQL_ERROR, newstr("No tuple selected"));
	}

	if (col >= cursor->cols) {
		return NULL;
		RAISE(SQL_ERROR, newstr("Invalid index for tuple %d", col));
	}

	row = cursor->rownum - 1;
	if (PQgetisnull(cursor->cursor, row, col)) {
		return NULL;
	}

	result = PQgetvalue(cursor->cursor, row, col);

	if (is_binary = PQfformat(cursor->cursor, col)) {
		return (Object *) stringNew("SOME BINARY VALUE");
	}
	else {
		return (Object *) stringNew(result);
	}
}

static void
initTupleFields(Tuple *tuple)
{
	Cursor *cursor = tuple->cursor;
	Hash *fields = hashNew(TRUE);
	char *name;
	String *key;
	int col;
	Int4 *colidx;

	for (col = 0; col < cursor->cols; col++) {
		name = PQfname(cursor->cursor, col);
		key = stringNew(name);
		colidx = int4New(col);
		hashAdd(fields, (Object *) key, (Object *) colidx);
	}
	cursor->fields = fields;
}

static Object *
pgsqlFieldByName(Tuple *tuple, String *name)
{
	Cursor *cursor = tuple->cursor;
	Int4 *col;

	if (!cursor->fields) {
		initTupleFields(tuple);
	}

	if (col = (Int4 *) hashGet(cursor->fields, (Object *) name)) {
		return (Object *) pgsqlFieldByIdx(tuple, col->value);
	}
	
	return NULL;
}

static char *
cursorFields(Cursor *cursor)
{
	int col;
	char *name;
	char *result = newstr("");
	char *tmp;
	for (col = 0; col < cursor->cols; col++) {
		name = PQfname(cursor->cursor, col);
		tmp = result;
		result = newstr("%s '%s'", tmp, name);
		skfree(tmp);
	}
	tmp = result;
	result = newstr("%s]", tmp);
	result[0] = '[';
	skfree(tmp);
	return result;
}

static char *
cursorRow(Cursor *cursor, int row)
{
	int col;
	char *value;
	char *result = newstr("");;
	char *tmp;

	for (col = 0; col < cursor->cols; col++) {
		tmp = result;
		if (PQgetisnull(cursor->cursor, row, col)) {
			result = newstr("%s nil", tmp);
		}
		else {
			value = PQgetvalue(cursor->cursor, row, col);
			result = newstr("%s '%s'", tmp, value);
		}

		skfree(tmp);
	}
	tmp = result;
	result = newstr("%s]", tmp);
	result[0] = '[';
	skfree(tmp);
	return result;
}

static char *
cursorAllRows(Cursor *cursor)
{
	int row;
	char *tmp;
	char *rowstr;
	char *result = NULL;
	for (row = 0; row < cursor->rows; row++) {
		rowstr = cursorRow(cursor, row);
		if (tmp = result) {
			result = newstr("%s %s", tmp, rowstr);
			skfree(rowstr);
			skfree(tmp);
		}
		else {
			result = rowstr;
		}
	}
	tmp = result;
	result = newstr("[%s]", tmp);
	skfree(tmp);
	return result;
}

static char *
cursorContents(Cursor *cursor)
{
	char *tmp;
	char *tmp2;
	char *tmp3;
	char *result = newstr("%s\n(%s %s)", 
						  tmp = objectSexp((Object *) cursor->querystr),
						  tmp2 = cursorFields(cursor),
						  tmp3 = cursorAllRows(cursor));
	skfree(tmp);
	skfree(tmp2);
	skfree(tmp3);
	return result;
}

static char *
pgCursorStr(Cursor *cursor)
{
	char *tmp;
	char *result;
	result =  newstr("<#%s# %s>", objTypeName((Object *) cursor), 
					 tmp = cursorContents(cursor));
	skfree(tmp);
	return result;
}

static char *
pgTupleStr(Tuple *tuple)
{
	Cursor *cursor;
	char *name;
	String *value;
	char *result = newstr("");
	char *tmp;
	int col;
	
    tuple = (Tuple *) dereference((Object *) tuple);
	cursor = tuple->cursor;
	for (col = 0; col < cursor->cols; col++) {
		name = PQfname(cursor->cursor, col);
		value = (String *) pgsqlFieldByIdx(tuple, col);
		tmp = result;
		if (value) {
			result = newstr("%s ('%s' . '%s')", tmp, name, value->value);
		}
		else {
			result = newstr("%s ('%s')", tmp, name);
		}
		skfree(tmp);
		objectFree((Object *) value, TRUE);
	}
	
	tmp = result;
	result = newstr("<#%s# (%s)>", objTypeName((Object *) tuple), tmp);
	skfree(tmp);
	return result;
}

static void
pgsqlFreeCursor(Cursor *cursor)
{
	if (cursor->cursor) {
		PQclear(cursor->cursor);
		cursor->cursor = NULL;
	}
	objectFree((Object *) cursor->fields, TRUE);
	objectFree((Object *) cursor->querystr, TRUE);
    skfree((void *) cursor);
}

static Tuple *
pgsqlNextRow(Cursor *cursor)
{
	if (cursor->rownum < cursor->rows) {
		cursor->rownum++;
		return &(cursor->tuple);
	}
	return NULL;
}

void
registerPGSQL()
{
	static SqlFuncs funcs = {
		OBJ_MISC,
		&pgsqlConnect,
		&pgsqlExecQry,
		&pgsqlNextRow,
		&pgsqlFieldByIdx,
		&pgsqlFieldByName,
		&pgTupleStr,
		&pgCursorStr,
		&pgsqlFreeCursor,
		&pgsqlCleanup
	};

	ObjReference *obj = objRefNew((Object *) &funcs);
	Hash *dbhash = (Hash *) symbolGet("dbhandlers")->svalue;
	String *handlername = stringNew("postgres");
	hashAdd(dbhash, (Object *) handlername, (Object *) obj);
}
