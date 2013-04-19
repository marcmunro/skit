/**
 * @file   skit_lib.h
 * \code
 *     Copyright (c) 2009, 2010, 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Define the skitlib API.
 */

#include <setjmp.h>
#include <glib.h>
#include <signal.h>
#include <regex.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxslt/xsltInternals.h>

/* Bump this whenever we make any structural changes to the skit xml
 * namespace.
 * XML versions history:
 * 0.1    Initial development version
 */
#define SKIT_XML_VERSION "0.1"

#define DIR_SEPARATOR '/'

typedef unsigned char boolean;

typedef enum {IS_NEW, IS_GONE, IS_DIFF, IS_SAME, 
	      IS_UNKNOWN, IS_REBUILD, HAS_DIFFKIDS} DiffType;

#define DIFFNEW       "new"
#define DIFFGONE      "gone"
#define DIFFDIFF      "diff"
#define DIFFSAME      "none"
#define DIFFUNKNOWN   "UNKNOWN"
#define DIFFKIDS      "diffkids"
#define ACTIONBUILD   "build"
#define ACTIONDROP    "drop"
#define ACTIONREBUILD "rebuild"
#define ACTIONNONE    "none"


typedef enum {
    TOKEN_OPEN_PAREN = 0,
    TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSE_BRACKET,
    TOKEN_OPEN_ANGLE,
    TOKEN_CLOSE_ANGLE,
    TOKEN_SYMBOL,
    TOKEN_NUMBER,
    TOKEN_DOT,
    TOKEN_DBLQUOTE_STR,
    TOKEN_QUOTE_STR,
    TOKEN_REGEXP,
    TOKEN_BROKEN_STR
} TokenType;


typedef enum {
    OBJ_UNDEFINED = 128,  	/* Start with a non-zero to make
				 * debugging and testing for unitialised
				 * objects easier */
    OBJ_INT4,			
    OBJ_STRING,
    OBJ_CONS,
    OBJ_EXCEPTION,
    OBJ_VECTOR,
    OBJ_VARRAY,                 /* VARRAY IS THE DYNAMICALLY-SIZED
                                 * CONTENTS OF A VECTOR.  IT IS NOT
                                 * REALLY AN OBJECT IN ITS OWN RIGHT */
    OBJ_HASH,                  
    OBJ_SYMBOL,
    OBJ_OPTIONLIST,
    OBJ_DOCUMENT,
    OBJ_XMLNODE,
    OBJ_FN_REFERENCE,
    OBJ_OBJ_REFERENCE,
    OBJ_REGEXP,
    OBJ_CONNECTION,
    OBJ_CURSOR,
    OBJ_TUPLE,
    OBJ_DAGNODE,
    OBJ_DEPENDENCY,
    OBJ_TRIPLE,
    OBJ_MISC,                   /* Eg, SqlFuncs structure */
    OBJ_DOT,       		/* This is not a real-object */
    OBJ_CLOSE_PAREN,    	/* This is not a real-object */
    OBJ_CLOSE_BRACKET,  	/* This is not a real-object */
    OBJ_CLOSE_ANGLE     	/* This is not a real-object */
} ObjType;

#define OBJ_NOTOBJECT OBJ_DOT

typedef struct Object {
    ObjType type;
} Object;

typedef struct Int4 {
    ObjType type;
    int     value;
} Int4;

typedef struct String {
    ObjType type;
    char *  value;
} String;

typedef struct Regexp {
    ObjType type;
    char   *src_str;
    regex_t regexp;
} Regexp;

typedef struct Cons {
    ObjType type;
    Object *car;
    Object *cdr;
} Cons;

typedef struct Exception{
    ObjType type;
    int     depth;
    int     signal;
    char   *text;
    Object *param;
    boolean caught;
    boolean re_raise;
    jmp_buf handler;
    char   *backtrace;
    char   *file_raised;
    int     line_raised;
    char   *file_caught;
    int     line_caught;
    struct Exception *prev;
} Exception;

typedef struct Varray {
    ObjType  type;
    Object  *vector[];
} Varray;

typedef struct Vector {
    ObjType  type;
    int      elems;  // Number of elems currently in the vector
    int      size;   // Size of vector (how many elems can be stored)
    Varray  *contents;
} Vector;

typedef struct Hash {
    ObjType     type;
    GHashTable *hash;    
} Hash;

typedef Object *(ObjectFn)(Object *);

typedef struct Symbol {
    ObjType     type;
    char       *name;
    ObjectFn   *fn;
    Object     *svalue;
    Cons       *scope;
} Symbol;

typedef struct Document {
    ObjType           type;
    xmlDocPtr         doc;
    xmlTextReaderPtr  reader;
    xsltStylesheetPtr stylesheet;
    Hash             *inclusions;
    Cons             *options;
} Document;

typedef struct Node {
    ObjType          type;
    xmlNode         *node;
} Node;

typedef struct FnReference {
    ObjType  type;
    void    *fn;
} FnReference;

typedef struct ObjReference {
    ObjType  type;
    void    *obj;
} ObjReference;

typedef struct Connection {
    ObjType  type;
    String  *dbtype;
    void    *sqlfuncs;
    void    *conn;
} Connection;

typedef struct Tuple {
    ObjType  type;
    int      rownum;
    boolean  dynamic;
    void    *cursor;
} Tuple;

typedef struct Cursor {
    ObjType  type;
    int      rows;
    int      cols;
    void    *cursor;
    Hash    *index;
    Hash    *fields;
    Tuple    tuple;
    String  *querystr;
    Connection *connection;
} Cursor;

typedef enum {
    BUILD_NODE = 0,
    DROP_NODE,
    DIFF_NODE,
    EXISTS_NODE,
    REBUILD_NODE,
    ARRIVE_NODE,
    DEPART_NODE,
    DIFFPREP_NODE,
    DIFFCOMPLETE_NODE,
    BUILD_AND_DROP_NODE,
    OPTIONAL_NODE,
    FALLBACK_NODE,
    ENDFALLBACK_NODE,
    UNSPECIFIED_NODE
} DagNodeBuildType;

#define BUILD_NODE_BIT 1
#define DROP_NODE_BIT 2
#define DIFF_NODE_BIT 4
#define EXISTS_NODE_BIT 8
#define ALL_BUILDTYPE_BITS 15

typedef int BuildTypeBitSet;
#define inBuildTypeBitSet(btbs, bt)		\
    ((btbs & (1 << (int) bt)) != 0)

typedef enum {
    UNVISITED = 27,
    VISITING,
    RESOLVED_F,         /* Node has been resolved forwards */
    RESOLVED,           /* Node has been resolved by resolving_tsort */
    VISITED,            /* Node has been visited by the graph resolver */
    SORTED,             /* Node has been sorted by tsort */
    UNBUILDABLE,	// Used in original tsort.c
    BUILDABLE,		// ditto
    SELECTED_FOR_BUILD	// ditto
} DagNodeStatus;

/* Used to identify to which side of the DAG, a given dependency
 * applies. */
typedef enum {
    FORWARDS = 17,
    BACKWARDS,
    BOTH
} DependencyApplication;


typedef struct DagNode {
    ObjType          type;
    String          *fqn;
    xmlNode         *dbobject;    // Reference only - not to be freed from here
    DagNodeStatus    status;
    DagNodeBuildType build_type;
    int              dep_idx;
    Vector          *forward_deps;   	// use objectFree(obj, FALSE);
    Vector          *backward_deps;  	// use objectFree(obj, FALSE);
    struct DagNode  *mirror_node;    	// Reference only
    struct DagNode  *parent;   	     	// Reference only
    struct DagNode  *breaker;           // The breaker for this node
    struct DagNode  *breaker_for;       // The node for which this is a breaker
    struct DagNode  *supernode;
    struct DagNode  *forward_subnodes;       	// Linked list
    struct DagNode  *backward_subnodes;       	// Linked list
    struct DagNode  *fallback_node;  
} DagNode;


/* Used to conveniently pass around multiple nodes as parameters to
 * hashEach.  This type is only ever allocated statically. */
typedef struct Triple {
    ObjType          type;
    Object          *obj1;
    Object          *obj2;
    Object          *obj3;
} Triple;

/* Used by sexpTok as its parameter, to retain position information
 * while tokenising a string expression */
typedef struct TokenStr {
    char *instr;
    char saved;
    char *pos;
} TokenStr;


// TODO: Make this stuff configurable within the build
#define WITH_CASSERT
#define MEM_DEBUG
#undef MEM_DEBUG

#ifdef WITH_CASSERT
#define assert(cond,...)						\
    do { if (!(cond)) { RAISE(ASSERTION_FAILURE, newstr(__VA_ARGS__));} \
    } while (FALSE)
#else
#define assert(cond,...) 
#endif


#define dbgSexp(x) printSexp(stderr, #x ": ", (Object *) x)
#define dbgNode(x) printNode(stderr, #x ": ", x)


#define streq(a,b) (strcmp(a,b)==0)

typedef Object *(TraverserFn)(Object *, Object *);
typedef Object *(HashEachFn)(Cons *, Object *);
typedef int (ComparatorFn)(Object **, Object **);


// cons.c
extern Cons *consNew(Object *car, Object *cdr);
extern void consFree(Cons *cons, boolean free_contents);
extern Object *consPush(Cons **list, Object *obj);
extern Object *consPop(Cons **list);
extern Cons *consRead(Object *closer, TokenStr *sexp);
extern Object *checkedConsRead(Object *closer, TokenStr *sexp);
extern char *consStr(Cons *cons);
extern Object *setCar(Cons *cons, Object *obj);
extern Object *setCdr(Cons *cons, Object *obj);
extern Object *getCar(Cons *cons);
extern Object *getCdr(Cons *cons);
extern int consLen(Cons *cons);
extern boolean consIaAlist(Cons *cons);
extern int consCmp(Cons *cons1, Cons *cons2);
extern boolean isCons(Cons *obj);
extern Object *alistGet(Cons *alist, Object *key);
extern Cons *alistExtract(Cons **p_alist, Object *key);
extern Object *consNth(Cons *list, int n);
extern Object *consGet(Cons *list, Object *key);
extern Object *consNext(Cons *list, Object **p_placeholder);
extern Cons *consAppend(Cons *list, Object *item);
extern Cons *consConcat(Cons *list, Object *list2);
extern boolean checkCons(Cons *cons, void *chunk);
extern boolean consIn(Cons *cons, Object *obj);
extern Cons *consRemove(Cons *cons, Cons *remove);
extern Cons *consCopy(Cons *list);

// object.c
extern char * typeName(ObjType type);
extern Tuple *tupleNew(Cursor *cursor);
extern char *objTypeName(Object *obj);
extern Int4 *int4New(int value);
extern int int4Cmp(Int4 *p1, Int4 *p2);
extern FnReference *fnRefNew(void *ref);
extern Object *objectRead(TokenStr *sexp);
extern void objectCmpFail(Object *obj1, Object *obj2);
extern int objectCmp(Object *obj1, Object *obj2);
extern void objectFree(Object *obj, boolean free_contents);
extern char *objectSexp(Object *obj);
extern void printSexp(void *stream, char *prefix, Object *obj);
extern void pSexp(Object *obj);
extern boolean isObject(Object *obj);
extern Object *objectCopy(Object *obj);
extern Object *objectEval(Object *obj);
extern Object *trappedObjectEval(Object *obj);
extern void traceOn(boolean on);
extern Object *evalSexp(char *str);
extern ObjReference *objRefNew(Object *obj);
extern Regexp *regexpNew(char *str);
extern int objType(Object *obj);
extern Object *dereference(Object *obj);
extern Object *objSelect(Object *collection, Object *key);
extern Object *objNext(Object *collection, Object **p_placeholder);
extern boolean isCollection(Object *object);
extern Object *objectFromStr(char *instr);
extern DagNode *dagNodeNew(xmlNode *node, DagNodeBuildType build_type);
extern char *nameForBuildType(DagNodeBuildType build_type);
extern boolean checkObj(Object *obj, void *chunk);

#define isSubnode(node)       (node && node->supernode)

// vector.c
extern Vector *vectorNew(int elems);
extern Object *vectorPush(Vector *vector, Object *obj);
extern Object *vectorPop(Vector *vector);
extern Vector *toVector(Cons *cons);
extern char *vectorStr(Vector *vector);
extern void vectorFree(Vector *vector, boolean free_contents);
extern void vectorStringSort(Vector *vector);
extern String *vectorConcat(Vector *vector);
extern void vectorAppend(Vector *vector1, Vector *vector2);
extern Vector *vectorCopy(Vector *vector);
extern Object *vectorGet(Vector *vec, Object *key);
extern Object *vectorRemove(Vector *vec, int index);
extern Object *vectorDel(Vector *vec, Object *obj);
extern void vectorSort(Vector *vec, ComparatorFn *fn);
extern boolean vectorSearch(Vector *vec, Object *obj, int *p_index);
extern Object *setPush(Vector *vector, Object *obj);
extern boolean checkVector(Vector *vec, void *chunk);
#define setPop vectorPop
#define setStr vectorStr
#define setGet vectorSet
#define setRemove vectorRemove
#define setDel vectorDel
#define setSearch vectorSearch

#define EACH(vector, idx)				\
    if (vector)						\
        for ((idx) = 0; (idx) < vector->elems; (idx)++)

#define ELEM(vec, idx)				\
    vec->contents->vector[idx]

#define isVector(obj)       (obj && (obj->type == OBJ_VECTOR))

#define isLastElem(vector, idx) (idx == vector->elems)


// hash.c
extern Hash *hashNew(boolean use_skalloc);
extern Object *hashAdd(Hash *hash, Object *key, Object *contents);
extern Object *hashDel(Hash *hash, Object *key);
extern Hash *toHash(Cons *cons);
extern char *hashStr(Hash *hash);
extern void hashFree(Hash *hash, boolean free_contents);
extern Object *hashGet(Hash *hash, Object *key);
extern Cons *hashToAlist(Hash *hash);
extern void hashEach(Hash *hash, HashEachFn *fn, Object *arg);
extern Hash *hashCopy(Hash *hash);
extern int hashElems(Hash *hash);
extern Vector *vectorFromHash(Hash *hash);
extern boolean checkHash(Hash *hash, void *chunk);
extern Object *hashNext(Hash *hash, Object **p_placeholder);

// string.c
extern String *stringNew(const char *value);
extern String *stringNewByRef(char *value);
extern String *stringDup(String *src);
extern void stringFree(String *obj, boolean free_contents);
extern int stringCmp4Hash(const void *obj1, const void *obj2);
extern int stringCmp(String *str1, String *str2);
extern boolean stringMatch(String *str, char *expr);
extern Int4 *stringToInt4(String *str);
extern Cons *stringSplit(String *instr, String *split);
extern String *stringLower(String *str);
extern String *stringNext(String *str, Object **placeholder);
extern boolean checkString(String *str, void *chunk);

// symbol.c
extern Hash *symbolTable();
extern void freeSymbolTable();
extern Symbol *symbolNew(char *name);
extern Symbol *symbolGet(char *name);
extern void symSet(Symbol *sym, Object *obj);
extern void symbolSet(char *name, Object *obj);
extern void symbolFree(Symbol *sym, boolean free_contents);
extern void symbolForget(Symbol *sym);
extern char *symbolStr(Symbol *sym);
extern Symbol *symbolCopy(Symbol *old);
extern Object *symbolEval(Symbol *sym);
extern Object *symbolExec(Symbol *sym, Object *obj);
extern void newSymbolScope();
extern void dropSymbolScope();
extern void setScopeForSymbol(Symbol *sym);
extern Object *symGet(Symbol *sym);
extern Object *symbolGetValue(char *name);
extern Object *symbolGetValueWithStatus(char *name, boolean *in_local_scope);
extern void symbolSetRoot(char *name, Object *value);
extern void symbolCreate(char *name, ObjectFn *fn, Object *value);
extern void checkSymbols();
extern boolean checkSymbol(Symbol *sym, void *chunk);

// parse.c
extern char *sexpTok(TokenStr *instr);
extern TokenType tokenType(char *tok);
extern char *tokenTypeName(TokenType tt);
extern char *strEval(char *instr);

//mem.c
#define MEM_DEBUG TRUE
#ifdef MEM_DEBUG

#define MEMPRINTF printf
extern void *memchunks_incr(void *chunk);
extern int memchunks_in_use();
extern void showChunks();
extern boolean checkChunk(void *chunk, void *check_for);
extern void memShutdown();
extern void curFree();
extern void showFree(int number_to_show);
extern void showMalloc(int number_to_show);
extern void TrackMalloc(int number_to_show);
extern void chunkInfo(void *chunk);

#define newstr(...)  memchunks_incr(g_strdup_printf(__VA_ARGS__))

#else 

#define MEMPRINTF 
#define memchunks_incr(x)
#define memchunks_in_use() 0
#define showChunks()
#define checkChunk(x)
#define memShutdown()
#define showFree(x)
#define showMalloc(x)
#define TrackMalloc(x)
#define checkChunk(x)
#define chunkInfo(x)

#define newstr(...)  g_strdup_printf(__VA_ARGS__)

#endif

extern void *skalloc(size_t size);
extern void skfree(void *ptr);
extern void *skrealloc(void *p, size_t size);
extern void skitFreeMem();

// optionlist.c
extern Cons *optionlistNew();
extern void optionlistAdd(Cons *list, String *option_name, 
			  String *field, Object *value);
extern void optionlistAddAlias(Cons *list, String *alias, String *key);
extern Object *optionlistGetOption(Cons *list, String *key);
extern Object *optionlistGetOptionValue(Cons *list, String *key, String *field);
extern String *optionlistGetOptionName(Cons *list, String *key);

// options.c
extern Hash *coreOptionHash();
extern Cons *printOptionList();
extern void freeOptions();
extern Hash *hashFromOptions(Cons *options);
extern Cons *optionKeyList(String *name);

// params.c
extern void record_args(int argc, char *argv[]);
extern char *usage_msg();
extern void show_usage(FILE *dest);
extern String *read_arg();
extern void unread_arg(String *arg, boolean is_option);
extern String *nextArg(String **p_arg, boolean *p_option);
extern String *nextAction();
extern Object *validateParamValue(String *type, String *value);


// action.c
extern void freeStdTemplates();
extern void loadInFile(String *filename);
extern Document *docStackPop();
extern Hash *parseAction(String *action);
extern void executeAction(String *action, Hash *params);
extern void finalAction();
extern void addDeps();

// builtin_symbols.c
extern void initBuiltinSymbols();

// filepath.c
extern char *pathToFile(Vector *roots, String *templatedir, String *dbdir, 
			Object *version, String *filename);
extern String *findFile(String *filename);
extern FILE *openFile(String *filename);
extern String *readFile(String *filename);
extern String *nextWord(FILE *file);
extern Document *docFromFile(String *path);
extern Document *simpleDocFromFile(String *path);
extern void makePath(char *path);
extern Document *scatterTemplate(String *path);
extern void documentFreeMem();

// xmlfile.c
extern void parseXSLStylesheet(Document *doc);
extern Document *applyXSLStylesheet(Document *src, Document *stylesheet);
extern Document *processTemplate(Document *template);
extern void addParamsNode(Document *doc, Object *params);
extern void rmParamsNode(Document *doc);
extern void docFromVector(xmlNode *parent_node, Vector *sorted_nodes);
extern void docGatherContents(Document *doc, String *filename);
extern xmlNode *firstElement(xmlNode *start);
extern xmlNode *copyObjectNode(xmlNode *source);

// document.c
extern char *nodestr(xmlNode *node);
extern Node *nodeNew(xmlNode *node);
extern Document *documentNew(xmlDocPtr xmldoc, xmlTextReaderPtr reader);
extern void documentFree(Document *doc, boolean free_contents);
extern char *documentStr(Document *doc);
extern void documentPrint(FILE *fp, Document *doc);
extern void documentPrintXML(FILE *fp, Document *doc);
extern void finishDocument(Document *doc);
extern void recordCurDocumentSource(String *URI, String *path);
extern void recordCurDocumentSkippedLines(String *URI, int lines);
extern Cons *getDocumentInclusion(Document *doc, String *URI);
extern Document *findDoc(String *filename);
extern boolean docIsPrintable(Document *doc);
extern boolean docHasDeps(Document *doc);
extern Object *xmlTraverse(xmlNode *start, TraverserFn *traverser, 
			   Object *param);
extern Object *xpathEach(Document *doc, String *xpath,
			 TraverserFn *traverser, Object *param);
extern String *nodeAttribute(xmlNodePtr node, const xmlChar *name);
extern void readDocDbver(Document *doc);
extern xmlNode *getElement(xmlNode *node);
extern xmlNode *getText(xmlNode *node);
extern void printNode(FILE *output, char *label, xmlNode *node);
extern void pNode(xmlNode *node);
extern void dumpNode(FILE *output, xmlNode *node);
extern void dNode(xmlNode *node);

// exceptions.c functions are defined in exceptions.h

// regexp.c
extern String *regexpReplace(String *src, Regexp *regexp, String *replacement);
extern String *regexpReplaceOnly(String *src, Regexp *regexp, 
				 String *replacement);

// sql.c
extern String *trimSqlText(String *text);
extern void finishWithConnection();
extern Connection *sqlConnect();
extern Cursor *sqlExec(Connection *connection, 
		       String *qry, Object *params);
extern void connectionFree(Connection *connection);
extern void cursorFree(Cursor *curs);
extern Tuple *sqlNextRow(Cursor *cursor);
extern char *tupleStr(Tuple *tuple);
extern String *tupleGet(Tuple *tuple, Object *key);
extern Object *cursorNext(Cursor *cursor, Object **p_placeholder);
extern char *cursorStr(Cursor *cursor);
extern boolean checkDbtypeIsRegistered(String *dbtype);
extern Tuple *cursorGet(Cursor *cursor, Object *key);
extern void *cursorIndex(Cursor *cursor, String *fieldname);
extern String *sqlDBQuote(String *first, String *second);
extern char *applyParams(char *qrystr, Object *params);


// pgsql.c
extern void registerPGSQL();
extern void pgsqlFreeMem();

// deps.c
extern void showDeps(DagNode *node);
extern void showHashDeps(Hash *nodes);
extern void showVectorDeps(Vector *nodes);

extern Vector *nodesFromDoc(Document *doc);
extern Hash *hashByFqn(Vector *vector);
extern void prepareDagForBuild(Vector **p_nodes);
extern Vector *resolving_tsort(Vector *nodelist);

extern Vector *dagFromDoc(Document *doc);



// tsort.c
extern Vector *simple_tsort(Vector *nodes);
extern Vector *gensort(Document *doc);

// navigation.c
extern Vector *navigationToNode(DagNode *current, DagNode *target);

// libxslt.c
extern void registerXSLTFunctions(xsltTransformContextPtr ctxt);

//diff.c
extern xmlNode *doDiff(String *diffrules, boolean swap);

