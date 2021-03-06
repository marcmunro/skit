
/**
 * @file   sql.h
 * \code
 *     Copyright (c) 2011 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
 * \endcode
 * @brief  
 * Define the skitlib API.
 */


typedef Connection *(ConnectFn)(Object *);
typedef Cursor *(QueryFn)(Connection *, String *, Object *);
typedef Tuple *(TupleFn)(Cursor *);
typedef Object *(FieldByIdxFn)(Tuple *, int);
typedef Object *(FieldByNameFn)(Tuple *, String *);
typedef char *(TupleStrFn)(Tuple *);
typedef char *(CursorStrFn)(Cursor *);
typedef void (CursorIndexFn)(Cursor *, String *);
typedef Tuple *(CursorGetFn)(Cursor *, Object *);
typedef String *(DBQuoteFn)(String *, String *);
typedef void (CloseCursorFn)(Cursor *);
typedef void (CloseConnectionFn)(Connection *);


typedef struct SqlFuncs {
    ObjType        type;
    ConnectFn     *connect;
    QueryFn       *query;
    TupleFn       *nextrow;
    FieldByIdxFn  *fieldbyidx;
    FieldByNameFn *fieldbyname;
    TupleStrFn    *tuplestr;
    CursorStrFn   *cursorstr;
    CursorIndexFn *cursorindex;
    CursorGetFn   *cursorget;
    DBQuoteFn     *dbquote;
    CloseCursorFn *closecursor;
    CloseConnectionFn *cleanup;
} SqlFuncs;
    
