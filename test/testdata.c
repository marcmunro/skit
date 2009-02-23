/**
 * @file   testdata.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides test data and a database access layer to retrieve it
 *
 */


#include <stdio.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "../src/sql.h"

#ifdef WIBBLE
CURSOR: <#OBJ_CURSOR# '
select d.datname as name,
       user as username,
       r.rolname as owner,
       pg_catalog.pg_encoding_to_char(d.encoding) as encoding,
       t.spcname as tablespace,
       d.datconnlimit as connections,
       quote_literal(obj_description(d.oid, 'pg_database')) as comment,
       
       regexp_replace(d.datacl::text, '.(.*).', E'\\1') as privs
from   pg_catalog.pg_database d
inner join pg_catalog.pg_roles r 
        on d.datdba = r.oid
  inner join pg_catalog.pg_tablespace t
          on t.oid = d.dattablespace
where d.datname = pg_catalog.current_database();

'
[ 'name' 'username' 'owner' 'encoding' 'tablespace' 'connections' 'comment' 'privs']
[[ 'skittest' 'marc' 'regress' 'UTF8' 'tbs3' '-1' nil '=Tc/regress,regress=CTc/regress']]>
CURSOR: <#OBJ_CURSOR# '
select role.rolname as priv,
       member.rolname as "to",
       grantor.rolname as "from",
       case when a.admin_option then 'yes' else 'no' end as with_admin
from   pg_catalog.pg_auth_members a,
       pg_catalog.pg_authid role,
       pg_catalog.pg_authid member,
       pg_catalog.pg_authid grantor
where  role.oid = a.roleid
and    member.oid = a.member
and    grantor.oid = a.grantor
order by 1,2;
'
[ 'priv' 'to' 'from' 'with_admin']
[[ 'keep' 'lose' 'keep' 'yes']
[ 'keep' 'wibble' 'keep' 'yes']
[ 'keep2' 'wibble' 'keep2' 'no']]>
CURSOR: <#OBJ_CURSOR# '
select a.rolname as name,
       case when a.rolsuper then 'y' else 'n' end as superuser,
       case when a.rolinherit then 'y' else 'n' end as inherit,
       case when a.rolcreaterole then 'y' else 'n' end as createrole,
       case when a.rolcreatedb then 'y' else 'n' end as createdb,
       
       case when a.rolcanlogin then 'y' else 'n' end as login,
       a.rolconnlimit as max_connections,
       a.rolpassword as password,
       a.rolvaliduntil as expires,
       a.rolconfig as config
from   pg_catalog.pg_authid a
order by a.rolname;
'
[ 'name' 'superuser' 'inherit' 'createrole' 'createdb' 'login' 'max_connections' 'password' 'expires' 'config']
[[ 'keep' 'n' 'n' 'n' 'n' 'y' '-1' 'md5a6e3dfe729e3efdf117eeb1059051f77' nil nil]
[ 'keep2' 'n' 'n' 'n' 'n' 'y' '-1' 'md5dd9b387fa54744451a97dc9674f6aba2' nil nil]
[ 'lose' 'n' 'n' 'n' 'n' 'y' '-1' 'md5c62bc3e38bac4209132682f13509ba96' nil nil]
[ 'marc' 'y' 'y' 'y' 'y' 'y' '-1' 'md5c62bc3e38bac4209132682f13509ba96' nil '{client_min_messages=error}']
[ 'marco' 'n' 'n' 'n' 'n' 'y' '-1' 'md54ea9ea89bc47825ea7b2fe7c2288b27a' nil nil]
[ 'regress' 'y' 'n' 'n' 'n' 'y' '-1' 'md5c2a101703f1e515ef9769f835d6fe78a' 'infinity' '{client_min_messages=warning}']
[ 'wibble' 'n' 'n' 'n' 'n' 'y' '-1' 'md54ea9ea89bc47825ea7b2fe7c2288b27a' '2007-03-01 00:00:00-08' nil]]>

#endif

static String *last_key;

static void
addQuery(Hash *hash, Cons *cons)
{
    String *key = (String *) cons->car;
    Cons *contents = (Cons *) cons->cdr;
    objectFree((Object *) cons, FALSE);
    hashAdd(hash, (Object *) key, (Object *) contents);
    last_key = key;
}

static Hash *
initQueries()
{
    Hash *hash = hashNew(TRUE);
    Object *obj = objectFromStr("(\"\n\n"
			   "select substring(pg_catalog.version() "
			   "from E'\\\\([0-9]+\\\\.[0-9]+\\\\"
				"(\\\\.[0-9]+\\\\)?\\\\)') \n"
			   "	as version;\n\""
			   "[ 'version'] [[ '8.3.5']])");
    addQuery(hash, (Cons *) obj);

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
    setScopeForSymbol(sym);
    symbolSet(name, (Object *) value);
    if (make_global && sym->scope) {
	/* We have to also set the default value for this scoped
	 * variable. */
	value = (String *) objectCopy((Object *) value);
	symbolSetRoot(name, (Object *) value);
    }
}

static Connection *
testConnect(Object *sqlfuncs)
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
    Symbol *sym = symbolGet("dbconnection");

    if (is_new) {
	/* If any string above has been retrieved from params rather
	 * than the symbol table, then we have new database connection
	 * information for this action, and so a new connection should
	 * be made.  If there is no existing connection, we will define all
	 * variables globally, otherwise we will define them only within
	 * the scope of the current action. */
	make_global = (connection == NULL);
	record_param(connect, "connect", make_global);
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
	connection->dbtype = stringNew("pgtest");
	connection->conn = (void *) connection;
	sym = symbolNew("dbconnection");
	sym->value = (Object *) connection;
    }
    return connection;
}

static Hash *query_hash = NULL;

static Cursor *
testExecQry(Connection *connection, 
	    String *qry,
	    Object *params)
{
    Cons *results;
    Cursor *curs;
    Vector *fields;
    Vector *rows;

    if (!query_hash) {
	query_hash = initQueries();
    }

    if (results = (Cons *) hashGet(query_hash, (Object *) qry)) {
	curs = (Cursor *) skalloc(sizeof(Cursor));
	curs->type = OBJ_CURSOR;
	curs->cursor = (void *) results;
	fields = (Vector *) results->car;
	rows = (Vector *) ((Cons *) results->cdr)->car;
	curs->rows = rows->elems;
	curs->cols = fields->elems;
	curs->rownum = 0;
	curs->fields = NULL;
	curs->tuple.type = OBJ_TUPLE;
	curs->tuple.cursor = curs;
	curs->cursor = results;
	curs->connection = connection;
	curs->querystr = stringNew(qry->value);
    }
    else {
	RAISE(NOT_IMPLEMENTED_ERROR,
	      newstr("Query not defined: %s", qry->value));
    }

    return curs;

}

static Object *
testFieldByIdx(Tuple *tuple, int col)
{
    Cursor *curs = tuple->cursor;
    Cons *results = (Cons *) curs->cursor;
    Vector *rows = (Vector *) ((Cons *) results->cdr)->car;
    Vector *result_row;
    int row = curs->rownum - 1;
    String *result;

    result_row = rows->vector[row];
    result = (String *) result_row->vector[col];
    return (Object *) stringNew(result->value);
}

static Object *
testFieldByName(Tuple *tuple, String *name)
{
    Cursor *curs = tuple->cursor;
    Cons *results = (Cons *) curs->cursor;
    Vector *fields = (Vector *) results->car;
    int col;

    for (col = fields->elems - 1; col >= 0; col--) {
	if (stringCmp(name, (String *) fields->vector[col]) == 0) {
	    return testFieldByIdx(tuple, col);
	}
    }
    return NULL;
}

static Tuple *
testNextRow(Cursor *cursor)
{
    if (cursor->rownum < cursor->rows) {
	cursor->rownum++;
	return &(cursor->tuple);
    }
    return NULL;
}

static char *
testCursorStr(Cursor *cursor)
{
    char *tmp = objectSexp((Object *) cursor->cursor);
    char *result = newstr("<#OBJ_CURSOR# %s>", tmp);
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
	NULL,
	&testCursorStr,
	&testFreeCursor,
	&testCleanup
    };
    
    ObjReference *obj = objRefNew((Object *) &funcs);
    Hash *dbhash = (Hash *) symbolGet("dbhandlers")->value;
    String *handlername = stringNew("pgtest");
    hashAdd(dbhash, (Object *) handlername, (Object *) obj);
}

