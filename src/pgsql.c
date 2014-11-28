/**
 * @file   pgsql.c
 * \code
 *     Copyright (c) 2009 - 2014 Marc Munro
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
#include <ctype.h>
#include <libpq-fe.h>
#include "skit_lib.h"
#include "exceptions.h"
#include "sql.h"

static String replace3 = {OBJ_STRING, "\\3"};

#define MATCH "[ \t=]*(['\"]?)([^'\"]*)\\2"  
#define HOST_MATCH ".*(host" MATCH ").*"
#define PORT_MATCH ".*(port" MATCH ").*"
#define DBNAME_MATCH ".*(dbname" MATCH ").*"
#define USER_MATCH ".*(username" MATCH ").*"
#define PASSWD_MATCH ".*(password" MATCH ").*"

static void
pgsqlConnectionCheck(PGconn *conn)
{
	char *supplemental;
	if (PQstatus(conn) != CONNECTION_OK) { 
		if (conn) {
			supplemental = PQerrorMessage(conn);
		}
		else {
			supplemental = "DOH!";
		}
		RAISE(SQL_ERROR, 
			  newstr("libpq error: PQstatus = %d, msg = \"%s\"", 
					 PQstatus(conn), supplemental));
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
		result = regexpReplaceOnly(connect, match, &replace3);
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

static char *
append(char *source, char *token, String *str)
{
	char *result = newstr("%s%s='%s' ", source, token, str->value);
	skfree(source);
	return result;
}

static Connection *
pgsqlConnect(Object *sqlfuncs)
{
	/* If there is no existing connection then we must create a new
	 * connection.
	 * If new connection information has been provided, then we must
	 * create a new connection.
     * The first connection we create will be made global.
     */

	Connection *volatile connection = 
		(Connection *) symbolGetValue("dbconnection");
	String *volatile connect;
	boolean make_global;
	boolean new_connection;
	String *host;
    String *port;
    String *dbname;
    String *user;
    String *pass;
	Symbol *sym;
	char *tmp;

	connect = (String *) symbolGetValueWithStatus("connect", &new_connection);

	if (!connect) {
		RAISE(SQL_ERROR,
			  newstr("No database connection defined"));
	}

	new_connection = make_global = (connection == NULL);

	host = get_param(connect, "host", HOST_MATCH, &new_connection);
    port = get_param(connect, "port", PORT_MATCH, &new_connection);
    dbname = get_param(connect, "dbname", DBNAME_MATCH, &new_connection);
	if (!dbname) {
		/* In a connect string the dbname param is called dbname, but
		   the parameter, as defined in connect.xml, we have called
		   database. */
		dbname = get_param(connect, "database", 
						   DBNAME_MATCH, &new_connection);
	}
    user = get_param(connect, "username", USER_MATCH, &new_connection);
    pass = get_param(connect, "password", PASSWD_MATCH, &new_connection);

	if (new_connection) {
		record_param(host, "host", make_global);
		record_param(port, "port", make_global);
		record_param(dbname, "dbname", make_global);
		record_param(user, "username", make_global);
		record_param(pass, "password", make_global);

		tmp = newstr("");
		if (host) tmp = append(tmp, "host", host);
		if (port) tmp = append(tmp, "port", port);
		if (dbname) tmp = append(tmp, "dbname", dbname);
		skfree(connect->value);
		connect->value = tmp;

		record_param(connect, "connect", make_global);

		connection = (Connection *) skalloc(sizeof(Connection));
		connection->type = OBJ_CONNECTION;
		connection->sqlfuncs = sqlfuncs;
		connection->dbtype = stringNew("postgres");
		connection->conn = NULL;

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
						 " with \"%s\"\n  %s", connect->
						 value, ex->text));
		}
		END;
		sym = symbolNew("dbconnection");
		sym->svalue = (Object *) connection;
	}
	else {
		objectFree((Object *) host, TRUE);
		objectFree((Object *) port, TRUE);
		objectFree((Object *) dbname, TRUE);
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

static Cursor *
pgsqlExecQry(Connection *connection, 
			 String *qry,
			 Object *params)
{
	PGconn *conn = pgConn(connection);
	char *querystr = qry->value;
	PGresult *result;
	Cursor *curs;
	
	if (params) {
		querystr = applyParams(querystr, params);
	}
	if (result = PQexec(conn, querystr)) {
		pgResultCheck(result);
		curs = (Cursor *) skalloc(sizeof(Cursor));
		curs->type = OBJ_CURSOR;
		curs->cursor = (void *) result;
		curs->rows = PQntuples(result);
		curs->cols = PQnfields(result);
		curs->fields = NULL;
		curs->tuple.type = OBJ_TUPLE;
		curs->tuple.cursor = curs;
		curs->tuple.dynamic = FALSE;
		curs->tuple.rownum = 0;
		curs->connection = connection;
		curs->querystr = stringNew(qry->value);
		curs->index = NULL;
		if (params) {
			skfree(querystr);
		}
		return curs;
	}
	if (params) {
		skfree(querystr);
	}
	RAISE(SQL_ERROR, 
		  newstr("Fatal postgres error: %s", PQresultErrorMessage(NULL)));
	return NULL;
}

static Object *
pgsqlFieldByIdx(Tuple *tuple, int col)
{
	Cursor *cursor = tuple->cursor;
	int row;
	int  is_binary;
	char *result;

	if (!tuple->rownum) {
		RAISE(SQL_ERROR, newstr("No tuple selected"));
	}

	if (col >= cursor->cols) {
		return NULL;
		RAISE(SQL_ERROR, newstr("Invalid index for tuple %d", col));
	}

	row = tuple->rownum - 1;

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
		(void) hashAdd(fields, (Object *) key, (Object *) colidx);
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
	if (tmp = result) {
		result = newstr("[%s]", tmp);
		skfree(tmp);
	}
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
	if (tmp3) {
		skfree(tmp3);
	}
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
	objectFree((Object *) cursor->index, TRUE);
	objectFree((Object *) cursor->fields, TRUE);
	objectFree((Object *) cursor->querystr, TRUE);
    skfree((void *) cursor);
}

static Tuple *
pgsqlNextRow(Cursor *cursor)
{
	if (cursor->tuple.rownum < cursor->rows) {
		cursor->tuple.rownum++;
		return &(cursor->tuple);
	}
	return NULL;
}

static Tuple *
pgsqlCursorGet(Cursor *cursor, Object *key)
{
	int index;
	Int4 *rownum;
	if (key->type == OBJ_INT4) {
		index = ((Int4 *) key)->value;
		if ((index < 1) || (index > cursor->rows)) {
			return NULL;
		}

		cursor->tuple.rownum = index;
		return &(cursor->tuple);
	}
	else {
		Tuple *tuple;
		if (!cursor->index) {
			RAISE(GENERAL_ERROR,
				  newstr("Cannot select by string from this cursor - "
						 "it has not been indexed"));
		}
		if (rownum = (Int4 *) hashGet(cursor->index, key)) {
			tuple = tupleNew(cursor);
			tuple->rownum = rownum->value;
			return tuple;
		}
	}
	return NULL;
}

static void
pgsqlIndexCursor(Cursor *cursor, String *fieldname)
{
	Int4 *col;
	Int4 *rowobj;
	Object *field;
	Object *prev;

	if (cursor->index) {
		objectFree((Object *) cursor->index, TRUE);
		cursor->index = NULL;
	}

	/* Ensure columns are indexed by name */
	if (!cursor->fields) {
		initTupleFields(&(cursor->tuple));
	}

	/* Figure out which column we are interested in */
	col = (Int4 *) hashGet(cursor->fields, (Object *) fieldname);
	if (!col) {
		return;
	}
	cursor->index = hashNew(TRUE);
	for (cursor->tuple.rownum = 1; 
		 cursor->tuple.rownum <= cursor->rows; 
		 cursor->tuple.rownum++) {
		if (field = (Object *) pgsqlFieldByIdx(&(cursor->tuple), col->value)) {
			rowobj = int4New(cursor->tuple.rownum);
			prev = hashAdd(cursor->index, field, (Object *) rowobj);
			if (prev) {
				objectFree(prev, TRUE);
				RAISE(INDEX_ERROR,
					  newstr("Multiple values for hash key in cursor: %s",
							 cursor->querystr->value));
			}
		}
	}
}

static void
loadWordList(String *filename, Hash *hash)
{
	FILE *file;
	String *word;
	Symbol *true = symbolGet("t");
	if (file = openFile(filename)) {
		while (word = nextWord(file)) {
			hashAdd(hash, (Object *) word, (Object *) true);
		}
		fclose(file);
	}
}


static Regexp *quotexpr = NULL;
static String replacement = {OBJ_STRING, "\\\""};
static Hash *reserved_words = NULL;

static Hash *
reservedWords()
{
	String *filename;
	if (!reserved_words) {
		filename = stringNewByRef(newstr("reserved_words.txt"));
		reserved_words = hashNew(TRUE);
		loadWordList(filename, reserved_words);
		objectFree((Object *) filename, TRUE);
	}
	return reserved_words;
}

static boolean
isReservedWord(String *word)
{
	Hash *reserved = reservedWords();
	Object *found = hashGet(reserved, (Object *) word);
	return found != NULL;
}

#if 0
static boolean
isOperatorChar(char c)
{
	return (c == '+') || (c == '-') || (c == '*') || (c == '/') ||
		(c == '<') || (c == '>') ||	(c == '=') || (c == '~') ||
		(c == '!') || (c == '@') ||	(c == '#') || (c == '%') ||
		(c == '^') || (c == '&') ||	(c == '|') || (c == '`');
}
#endif

static boolean
nameNeedsQuote(String *name)
{
	char *str = name->value;
	char c;
	if (isdigit(str[0])) {
		return TRUE;
	}

	while (c = *str++) {
		if (islower(c) || isdigit(c) || c == '_') {
			continue;
		}
		return TRUE;
	}

	return isReservedWord(name);
}

static Regexp *
quoteExpr()
{
	if (!quotexpr) {
		quotexpr = regexpNew("\"");
	}
	return quotexpr;
}

static String *
enquote(String *src)
{
	String *escaped = regexpReplace(src, quoteExpr(), &replacement);
	String *result = stringNewByRef(newstr("\"%s\"", escaped->value));
	objectFree((Object *) escaped, TRUE);
	return result;
}

static String *
pgsqlQuoteName(String *name)
{
	String *result = name;
	if (nameNeedsQuote(name)) {
		result = enquote(name);
		objectFree((Object *) name, TRUE);
	}
	return result;
}

/* Consumes first and second, returning a new quoted string */
static String *
pgsqlDBQuote(String *first, String *second)
{
	String *tmp1;
	String *tmp2;
	String *result;

	if (second) {
		if (first->value[0] == '\0') {
			objectFree((Object *) first, TRUE);
			return pgsqlQuoteName(second);
		}
		tmp1 = pgsqlQuoteName(first);
		tmp2 = pgsqlQuoteName(second);
		result = stringNewByRef(newstr("%s.%s", tmp1->value, tmp2->value));
		objectFree((Object *) tmp1, TRUE);
		objectFree((Object *) tmp2, TRUE);
		return result;
	}
	return pgsqlQuoteName(first);
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
		&pgsqlIndexCursor,
		&pgsqlCursorGet,
		&pgsqlDBQuote,
		&pgsqlFreeCursor,
		&pgsqlCleanup
	};

	ObjReference *obj = objRefNew((Object *) &funcs);
	Hash *dbhash = (Hash *) symbolGet("dbhandlers")->svalue;
	String *handlername = stringNew("postgres");
	(void) hashAdd(dbhash, (Object *) handlername, (Object *) obj);
}

void pgsqlFreeMem()
{
	objectFree((Object *) quotexpr, TRUE);
	quotexpr = NULL;
	objectFree((Object *) reserved_words, TRUE);
	reserved_words = NULL;
}
