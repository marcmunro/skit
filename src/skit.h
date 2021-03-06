/**
 * @file   skit_lib.h
 * \code
 *     Copyright (c) 2009 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Define the skitlib API.
 */

#include <inttypes.h>
#include <setjmp.h>
#include <glib.h>
#include <signal.h>
#include <regex.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxslt/xsltInternals.h>
#include <libxml/xpath.h>


#define SKIT_VERSION "0.1.1 alpha"

/* The xml versions are bumped when changes are made to xml file formats
 * other than the introuduction of new elements and attributes.  This
 * will allow incompatible file formats to be identified so that they
 * may be transformed.
 *
 * XML versions history:
 * pg   0.1    Initial development version
 */
#define SKIT_XML_VERSIONS "<('pg' . '0.1')>"

#define DIR_SEPARATOR '/'

typedef unsigned char boolean;

typedef enum {IS_NEW, IS_GONE, IS_DIFF, IS_SAME, 
	      IS_UNKNOWN, IS_REBUILD, HAS_DIFFKIDS} DiffType;

#define DIFFNEW        	   "new"
#define DIFFGONE       	   "gone"
#define DIFFDIFF       	   "diff"
#define DIFFSAME       	   "none"
#define DIFFUNKNOWN    	   "UNKNOWN"
#define DIFFKIDS       	   "diffkids"
#define ACTIONBUILD    	   "build"
#define ACTIONDROP     	   "drop"
#define ACTIONREBUILD  	   "rebuild"
#define ACTIONNONE     	   "none"
#define ACTIONFALLBACK 	   "fallback"
#define ACTIONENDFALLBACK  "endfallback"


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
    OBJ_DEPENDENCYSET,
    OBJ_CONTEXT,
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
    ObjType             type;
    xmlDocPtr           doc;
    xmlTextReaderPtr    reader;
    xsltStylesheetPtr   stylesheet;
    xmlXPathContextPtr  xpath_context;
    Hash               *inclusions;
    Cons               *options;
} Document;

typedef struct Node {
    ObjType          type;
    xmlNode         *node;
    struct Node     *parent;
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


/* Note that the order of these is significant for redirectActionType in
 * src/deps.c */
typedef enum {
    BUILD_NODE = 0,
    DROP_NODE,
    REBUILD_NODE,
    DIFF_NODE,
    FALLBACK_NODE,
    DSFALLBACK_NODE,
    ENDFALLBACK_NODE,
    DSENDFALLBACK_NODE,
    EXISTS_NODE,
    BUILD_AND_DROP_NODE,
    DIFFPREP_NODE,
    OPTIONAL_NODE,
    ARRIVE_NODE,
    DEPART_NODE,
    DEACTIVATED_NODE,
    UNSPECIFIED_NODE
} DagNodeBuildType;

#define NODEBIT(n) (1 << n)

#define BUILDABLE_BUILD_TYPES \
    (NODEBIT(BUILD_NODE) + \
     NODEBIT(DROP_NODE) + \
     NODEBIT(REBUILD_NODE) + \
     NODEBIT(DIFF_NODE) + \
     NODEBIT(DIFFPREP_NODE))

#define FORWARD_BUILDABLE_BUILD_TYPES \
    (NODEBIT(BUILD_NODE) + \
     NODEBIT(REBUILD_NODE) + \
     NODEBIT(DIFF_NODE))

#define IS_BUILDABLE(node) \
    (NODEBIT(node->build_type) & BUILDABLE_BUILD_TYPES)

#define IS_FORWARD_BUILDABLE(node) \
    (NODEBIT(node->build_type) & FORWARD_BUILDABLE_BUILD_TYPES)

typedef enum {
    UNVISITED = 27,
    VISITING,
    RESOLVED,
    VISITED
} DagNodeStatus;

/* Used to identify to which side of the DAG, a given dependency
 * applies. */
typedef enum {
    UNKNOWN_DIRECTION = 0,
    BACKWARDS,
    FORWARDS,
    BOTH_DIRECTIONS,
} DependencyApplication;


typedef struct DagNode {
    ObjType             type;
    String             *fqn;
    xmlNode            *dbobject;  // Reference only - not to be freed from here
    DagNodeStatus       status;
    DagNodeBuildType    build_type;
    Vector             *deps;         // use objectFree(obj, FALSE);
    Vector             *tmp_deps;
    int                 cur_dep;
    int                 resolver_depth;
    boolean             is_fallback;
    struct DagNode     *parent;       // Reference only
    struct DagNode     *mirror_node;  // Reference only
} DagNode;


struct Dependency;

typedef struct DependencySet {
    ObjType                type;
    int                    id;
    int                    priority;
    int                    chosen_dep;
    int                    cycles;
    boolean                direction_is_forwards;
    boolean                has_fallback;
    DagNode               *definition_node;
    String                *fallback_expr;
    String                *fallback_parent;
    String                *condition;
    boolean                deactivated;
    Vector                *deps;
} DependencySet;


typedef struct Dependency {
    ObjType                type;
    int                    id;
    String                *qn;
    boolean                qn_is_full;
    boolean                is_forwards;
    boolean                propagate_mirror;
    DagNode               *dep;
    DependencySet         *depset;
    DagNode               *from; 
    String                *condition;
    struct Dependency     *endfallback;
} Dependency;


typedef struct Context {
    ObjType                type;
    String                *context_type;
    String                *value;
    String                *dflt;
} Context;


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


#ifdef WITH_CASSERT
#define assert(cond,...)						\
    do { if (!(cond)) { RAISE(ASSERTION_FAILURE, newstr(__VA_ARGS__));} \
    } while (FALSE)
#else
#define assert(cond,...) 
#endif

/* Assert that an object variable is of the correct type.
 */
#define assertType(x,t)							\
    assert(x && (x->type == t),						\
	   x? "Variable " #x " is not of type " #t			\
	   ".  Instead is %d at " __FILE__ ":%d"			\
	   : "Variable " #x " is NULL (%d) at " __FILE__ ":%d",		\
	   x? x->type: 0, __LINE__)

#define assertDoc(x) assertType(x, OBJ_DOCUMENT)
#define assertVector(x) assertType(x, OBJ_VECTOR)
#define assertHash(x) assertType(x, OBJ_HASH)
#define assertString(x) assertType(x, OBJ_STRING)
#define assertNode(x) assertType(x, OBJ_XMLNODE)
#define assertDagNode(x) assertType(x, OBJ_DAGNODE)
#define assertDependency(x) assertType(x, OBJ_DEPENDENCY)
#define assertDependencySet(x) assertType(x, OBJ_DEPENDENCYSET)
#define assertCons(x) assertType(x, OBJ_CONS)
#define assertInt4(x) assertType(x, OBJ_INT4)

/* To suppress warnings about unused parameters */
#define UNUSED(x) (void)(x)

/* Useful debugging macros. */
#define dbgSexp(x) printSexp(stderr, #x ": ", (Object *) x)
#define dbgNode(x) printNode(stderr, #x ": ", x)


#define streq(a,b) (strcmp(a,b)==0)
#define stringeq(a,b) (strcmp((a)->value,(b)->value)==0)

typedef Object *(TraverserFn)(Object *, Object *);
typedef Object *(HashEachFn)(Cons *, Object *);
typedef int (ComparatorFn)(Object **, Object **);
typedef boolean (MatchFn)(Object *, Object *);


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
extern boolean consIsAlist(Cons *cons);
extern int consCmp(Cons *cons1, Cons *cons2);
extern boolean isCons(Cons *obj);
extern Object *alistGet(Cons *alist, Object *key);
extern Object *listExtract(Cons **p_list, Object *key, MatchFn *match);
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
extern DependencySet *dependencySetNew(DagNode *definition_node);
extern Dependency *dependencyNew(
    String *qn, boolean qn_is_full, boolean is_forwards);
extern Context *contextNew(String *context_type, String *value, String *dflt);
extern boolean depIsActive(Dependency *dep);

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
extern void vectorInsert(Vector *vec, Object *obj, int idx);
extern Object *vectorRemove(Vector *vec, int index);
extern Object *vectorDel(Vector *vec, Object *obj);
extern void vectorSort(Vector *vec, ComparatorFn *fn);
extern boolean vectorSearch(Vector *vec, Object *obj, int *p_index);
extern Object *setPush(Vector *vector, Object *obj);
extern boolean checkVector(Vector *vec, void *chunk);
extern void vectorClose(Vector *vec);
extern Object *vectorFind(Vector *vec, Object *obj);
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
extern Vector *hashVectorAppend(Hash *hash, Object *key, Object *contents);
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
extern void appendStr(String *str1, String *str2);
extern String *stringNew(const char *value);
extern String *stringNewByRef(char *value);
extern String *stringDup(String *src);
extern void stringFree(String *obj, boolean free_contents);
extern int stringCmp4Hash(const void *obj1, const void *obj2);
extern int stringCmp(String *str1, String *str2);
extern boolean stringMatch(String *str, char *expr);
extern Int4 *stringToInt4(String *str);
extern Cons *stringSplit(String *instr, String *split, boolean match_quotes);
extern String *stringLower(String *str);
extern void stringLowerInPlace(String *str);
extern String *stringNext(String *str, Object **placeholder);
extern boolean checkString(String *str, void *chunk);

// symbol.c
extern Hash *symbolTable(void);
extern void freeSymbolTable(void);
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
extern void newSymbolScope(void);
extern void dropSymbolScope(void);
extern void setScopeForSymbol(Symbol *sym);
extern Object *symGet(Symbol *sym);
extern Object *symbolGetValue(char *name);
extern Object *symbolGetValueWithStatus(char *name, boolean *in_local_scope);
extern void symbolSetRoot(char *name, Object *value);
extern void symbolCreate(char *name, ObjectFn *fn, Object *value);
extern boolean checkSymbol(Symbol *sym, void *chunk);

// parse.c
extern char *sexpTok(TokenStr *instr);
extern TokenType tokenType(char *tok);
extern char *tokenTypeName(TokenType tt);
extern char *strEval(char *instr);

//mem.c
#ifdef MEM_DEBUG

#define MEMPRINTF printf
extern void *memchunks_incr(void *chunk);
extern int memchunks_in_use(void);
extern void showChunks(void);
extern boolean checkChunk(void *chunk, void *check_for);
extern void memShutdown(void);
extern void curFree(void);
extern void showFree(int number_to_show);
extern void showMalloc(int number_to_show);
extern void trackMalloc(int number_to_show);
extern void chunkInfo(void *chunk);
extern void *getChunk(intptr_t chunk_id);

#define newstr(...)  memchunks_incr(g_strdup_printf(__VA_ARGS__))

#else 

#define MEMPRINTF 
#define memchunks_incr(x) x
#define memchunks_in_use() 0
#define showChunks()
#define checkChunk(x,y) TRUE
#define memShutdown()
#define showFree(x)
#define showMalloc(x)
#define trackMalloc(x)
#define chunkInfo(x)

#define newstr(...)  g_strdup_printf(__VA_ARGS__)

#endif

extern void *skalloc(size_t size);
extern void skfree(void *ptr);
extern void *skrealloc(void *p, size_t size);
extern void skitFreeMem(void);

// optionlist.c
extern Cons *optionlistNew(void);
extern void optionlistAdd(Cons *list, String *option_name, 
			  String *field, Object *value);
extern void optionlistAddAlias(Cons *list, String *alias, String *key);
extern Object *optionlistGetOption(Cons *list, String *key);
extern Object *optionlistGetOptionValue(Cons *list, String *key, String *field);
extern String *optionlistGetOptionName(Cons *list, String *key);

// options.c
extern Hash *coreOptionHash(void);
extern Cons *printOptionList(void);
extern void freeOptions(void);
extern Hash *hashFromOptions(Cons *options);
extern Cons *optionKeyList(String *name);

// params.c
extern void record_args(int argc, char *argv[]);
extern String *usage_msg(String *usage_for);
extern void show_usage(FILE *dest, String *usage_for);
extern String *read_arg(void);
extern void unread_arg(String *arg, boolean is_option);
extern String *nextArg(String **p_arg, boolean *p_option);
extern String *nextAction(void);
extern Object *validateParamValue(String *type, String *value);
extern void initTemplatePath(char *arg);


// action.c
extern void freeStdTemplates(void);
extern void loadInFile(String *filename);
extern void docStackPush(Document *doc);
extern Document *docStackPop(void);
extern Hash *parseAction(String *action);
extern void executeAction(String *action, Hash *params);
extern void finalAction(void);
extern void addDeps(void);
extern void applyXSL(Document *xslsheet);
extern Document *getFallbackProcessor(void);
extern Document *getDDLProcessor(void);

// builtin_symbols.c
extern void initBuiltInSymbols(void);

// filepath.c
extern void searchdir(Hash *hash, char *path, char *pattern);
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
extern void documentFreeMem(void);
extern void delFile(char *filename);

// xmlfile.c
extern void parseXSLStylesheet(Document *doc);
extern Document *applyXSLStylesheet(Document *src, Document *stylesheet);
extern Document *processTemplate(Document *template);
extern void addParamsNode(Document *doc, Object *params);
extern void rmParamsNode(Document *doc);
extern void docGatherContents(Document *doc, String *filename);
extern xmlNode *firstElement(xmlNode *start);
extern xmlNode *copyObjectNode(xmlNode *source);
extern void freeSkitProcessors(void);
extern Document *docFromVector(xmlNode *parent_node, Vector *sorted_nodes);
extern boolean isPrintable(xmlNode *node);

// document.c
extern char *nodestr(xmlNode *node);
extern Node *nodeNew(xmlNode *node);
extern xmlXPathObject *xpathEval(Document *doc, xmlNode *node, char *expr);
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
extern String *nodeAttribute(xmlNodePtr node, char *name);
extern boolean nodeHasAttribute(xmlNodePtr node, char *name);
extern void readDocDbver(Document *doc);
extern xmlNode *getText(xmlNode *node);
extern void printNode(FILE *output, char *label, xmlNode *node);
extern void pNode(xmlNode *node);
extern void dumpNode(FILE *output, xmlNode *node);
extern void dNode(xmlNode *node);
extern xmlNode *getNextNode(xmlNode *node);
extern xmlNode *nextDependency(xmlNode *start, xmlNode *prev);
extern xmlNode *nextDepFromTree(xmlNode *start, xmlNode *prev);


// exceptions.c functions are defined in exceptions.h

// regexp.c
extern String *regexpReplace(String *src, Regexp *regexp, String *replacement);
extern String *regexpReplaceOnly(String *src, Regexp *regexp, 
				 String *replacement);
extern boolean regexpMatch(Regexp *regexp, String *str);

// sql.c
extern String *trimSqlText(String *text);
extern void finishWithConnection(void);
extern Connection *sqlConnect(void);
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
extern void registerPGSQL(void);
extern void pgsqlFreeMem(void);

// deps.c
extern boolean isDependencySet(xmlNode *node);
extern boolean isDependency(xmlNode *node);
extern boolean isDependencies(xmlNode *node);
extern boolean isDepNode(xmlNode *node);
extern String *conditionForDep(xmlNode *node);
extern String *attributeForDep(xmlNode *node, char *attribute);

extern void showDeps(DagNode *node);
extern void showVectorDeps(Vector *nodes);

extern Vector *nodesFromDoc(Document *doc);
extern void prepareDagForBuild(Vector **p_nodes);
extern Vector *resolving_tsort(Vector *nodelist);

extern Vector *dagFromDoc(Document *doc);
extern DependencyApplication dependencyApplicationForString(String *direction);
extern Vector *dagNodesFromDoc(xmlNode *root);



// tsort.c
extern Vector *simple_tsort(Vector *nodes);
extern Vector *tsort(Document *doc);

// navigation.c
extern void addNavigationToDoc(xmlNode *parent);

// libxslt.c
extern void registerXSLTFunctions(xsltTransformContextPtr ctxt);
extern void xsltEvalFunction(xmlXPathParserContextPtr ctxt, int nargs);

//diff.c
extern xmlNode *doDiff(String *diffrules, boolean swap);

// system.c
extern String *username(void);
extern String *homedir(void);
