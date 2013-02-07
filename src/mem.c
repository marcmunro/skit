/**
 * @file   mem.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Dynamic memory handling helpers for skit.
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "skit_lib.h"
#include "exceptions.h"

static int chunks_in_use = 0;
static int chunk_number = 0;
static int free_number = 0;
static int show_free_number = 0;
static int show_malloc_number = 0;
static int track_malloc_number = 0;
static void *track_chunk = NULL;

static GHashTable *hash_chunks = NULL;
static GHashTable *hash_frees = NULL;


#ifdef MEM_DEBUG

/**
 * For debugging purposes.  Add a call to this from wherever you need 
 * breakpoint and then you can break on it.
 */
static void
memdebug(char *label)
{
    fprintf(stderr, "MEMDEBUG: %s\n", label);
}

/**
 * Key equality function for use in hash tables.
 */
static gboolean 
equalKey(gconstpointer obj1, gconstpointer obj2)
{
    // All keys are strings.
    if (obj1 == obj2) {
	return TRUE;
    }
    return streq((char *) obj1, (char *) obj2);
}

/** 
 * Function for freeing hash keys, required by hash table.
 */
static void
freeHashKey(gpointer key)
{
    free(key);
}

/**
 * Create a hash table for tracking malloc and free usage.
 */
static GHashTable *
newHashTable()
{
    GHashTable *tbl;
    tbl = g_hash_table_new_full(g_str_hash, equalKey, freeHashKey, NULL);
    return tbl;
}

/**
 * Free a hash table previously used for tracking malloc and free usage.
 */
static void
freeHashTable(GHashTable *hash)
{
    g_hash_table_destroy(hash);
}

/**
 * Return the hash table used to keep track of alloc'd chunks of memory.
 */
static GHashTable *
chunkTable()
{
    if (!hash_chunks) {
	hash_chunks = newHashTable();
    }

    return hash_chunks;
}

/**
 * Return the hash table used to keep track of freed chunks of memory.
 */
static GHashTable *
freeTable()
{
    if (!hash_frees) {
	char *x;
	hash_frees = newHashTable();
	(void) chunkTable();  
	x = newstr("x");      
	skfree(x);            /* Alloc and free a string to initialise 
			       * usage of the the glib hash.  This
			       * should make supression of valgrind
			       * errors easier. */
    }

    return hash_frees;
}

/* This is nasty and probably not portable.  It's debug code though so I
 * don't much care.
 */
static void *
toPtr(char *str)
{
    long ptr = 0;
    int nibble;
    char ch;
    while (ch = *str++) {
	if (ch > 'f') nibble = 0;
	else if (ch >= 'a') nibble = ch - 'a' + 10;
	else nibble = ch - '0';
	ptr = (ptr << 4) + nibble;
    }
    return (void *) ptr;
}

/**
 * Function to print objects that have not been freed at the end of a
 * run.  This is aimed at just being helpful enough for debugging
 * purposes.  It is not intended as a general purpose print function.
 */
static void
printObj(Object *obj)
{
    if (isObject(obj)) {
	switch (obj->type) {
	case OBJ_INT4: 
	    fprintf(stderr, "int4: %d\n", ((Int4 *) obj)->value); 
	    break;
	case OBJ_STRING: 
	    fprintf(stderr, "string: \"%s\"\n", ((String *) obj)->value); 
	    break;
	case OBJ_CONS: 
	    fprintf(stderr, "cons: car=%p, cdr=%p\n", 
		    ((Cons *) obj)->car, ((Cons *) obj)->cdr); 
	    if (((Cons *) obj)->car) {
		fprintf(stderr, "Contents: ");
		printObj(((Cons *) obj)->car);
	    }
	    break;
	case OBJ_EXCEPTION: 
	{
	    Exception *ex = (Exception *) obj;
	    fprintf(stderr, "exception: depth=%d, signal=%d, ", 
		    ex->depth, ex->signal);
	    if (ex->text) {
		fprintf(stderr, "text=\"%s\", ", ex->text);
	    }
	    fprintf(stderr, "caught=%d, re_raise=%d, ", 
		    ex->caught, ex->re_raise);
	    if (ex->backtrace) {
		fprintf(stderr, "backtrace=\"%s\", ", ex->backtrace);
	    }
	    if (ex->file_raised) {
		fprintf(stderr, "file_raised=\"%s\", ", ex->file_raised);
	    }
	    if (ex->file_caught) {
		fprintf(stderr, "file_caught=\"%s\", ", ex->file_caught);
	    }
	    fprintf(stderr, "\n");

	}
	default: 
	    fprintf(stderr, "type %s\n", objTypeName(obj)); 
	    break; 
	}
    }
    else {
	fprintf(stderr, "char *??: %s\n", (char *) obj); 
    }
}

/* Print to stderr whatever is known about the given memory chunk.
 */
void
chunkInfo(void *chunk)
{
    GHashTable *hash = chunkTable();
    char *keystr = malloc(20);
    gpointer key;
    gpointer contents = NULL;

    sprintf(keystr, "%p", chunk);
    if (g_hash_table_lookup_extended(hash, (gpointer) keystr,
				     &key, &contents)) {
	fprintf(stderr, "Chunk %p is malloc'd  as number %d\n", 
		chunk, (intptr_t) contents);
    }
    else {
	hash = freeTable();
	if (g_hash_table_lookup_extended(hash, (gpointer) keystr,
					 &key, &contents)) {
	    fprintf(stderr, "Chunk %p was freed as number %d\n", 
		    chunk, (intptr_t) contents);
	    memdebug("in chunkinfo");
	}
	else {
	    fprintf(stderr, "Chunk %p is not known to mem.c\n", chunk);
	    memdebug("in chunkinfo(2)");
	}
    }
    free(keystr);
}



/* Move the record of a chunk of memory from one hash to another.  The
 * result contains the previous contents, if present.  This does the
 * heavy lifting for addChunk and delChunk, and can be also used to
 * free hash table entries.
 */
static intptr_t
moveChunk(GHashTable *from_hash, GHashTable *to_hash, 
	  void *chunk, intptr_t chunk_number)
{
    char *keystr = malloc(20);
    gpointer previous_contents = NULL;
    gpointer previous_contents2;
    gpointer previous_key;

    sprintf(keystr, "%p", chunk);
    /* Get the existing value, if any. */
    if (from_hash) {
	if (g_hash_table_lookup_extended(from_hash, (gpointer) keystr,
					 &previous_key, &previous_contents)) 
	{
	    /* There is an existing value, so remove it. */
	    g_hash_table_remove(from_hash, previous_key);
	}
    }
    if (to_hash) {
	if (g_hash_table_lookup_extended(to_hash, (gpointer) keystr,
					 &previous_key, &previous_contents2)) 
	{
	    /* This is bad.  The target hash already contains this
	     * entry. */
	    return -1;
	}
	/* Add the entry to the target hash. */
	g_hash_table_insert(to_hash, (gpointer) keystr, 
			    (gpointer) chunk_number);
    }
    else {
	free(keystr);
    }
    return (intptr_t) previous_contents;
}

/* Record the allocation of a chunk of free memory.  The chunk_number is
 * stored in the hash, indexed by a string representing the address.
 * If any chunks remain unfreed, their details can be printed using
 * showChunks().
 */
static void
addChunk(void *chunk)
{
    int previous = moveChunk(freeTable(), chunkTable(), chunk, chunk_number);
    if (previous == -1) {
	RAISE(MEMORY_ERROR, 
	      newstr("addChunk: This chunk (%p) is already recorded!", chunk));
    }
}

/* Record the de-allocation of a chunk of free memory.  The existing
 * entry in the chunks hash is removed and a new entry is added to the
 * frees hash, indexed by the address (as a string) and containing the
 * chunk_number as malloc'd.  If an attempt is made to free the same
 * memory again, we can tell which chunk was.
 */
static void
delChunk(void *chunk)
{
    int previous = moveChunk(chunkTable(), freeTable(), chunk, free_number);

    if (!previous) {
	memdebug("FREEING UNKNOWN CHUNK");
	printObj((Object *) chunk);
	RAISE(MEMORY_ERROR, 
	      newstr("delChunk: Chunk %p not allocated (at free %d)", 
		     chunk, free_number));
    }
    if (previous == -1) {
	memdebug("CHUNK ALREADY FREED");
	RAISE(MEMORY_ERROR, 
	      newstr("delChunk: Chunk %p already freed", chunk));
    }
    if (free_number == show_free_number) {
	fprintf(stderr, "  Freeing chunk %p: freed as %d, malloc'd as %d\n", 
		chunk, free_number, previous);
	printObj((Object *) chunk);
	memdebug("FREEING IDENTIFIED CHUNK");
    }
}

/* Show the currently allocated objects in as much detail as possible.
 */
static void
showChunk(
    gpointer key,
    gpointer contents,
    gpointer param)
{
    void *ptr;
    Object *obj;
    /* The keys passed to this function are real C-strings.  Before
     * placing them in the array, we should convert them to String
     * objects.  Note that we will be just copying the pointers and
     * not doing any real copies.  Remember this when it becomes time to
     * free the strings! */

    fprintf(stderr, "Chunk %d not freed: %s.", 
	    (intptr_t) contents, (char *) key);
    ptr = toPtr((char *) key);

    printObj((Object *) ptr);
}

/* Print the contents of all non-free'd chunks
 */
void
showChunks()
{
    g_hash_table_foreach(chunkTable(), showChunk, NULL);
}

int
memchunks_in_use()
{
    return chunks_in_use;
}

void *
memchunks_incr(void *chunk)
{
    //MEMPRINTF("+");
    chunk_number++;
    
    if (chunk_number == show_malloc_number) {
	fprintf(stderr, "  Allocating %p, chunk no %d\n", chunk, chunk_number);
	memdebug("ALLOCATING IDENTIFIED CHUNK");
    }
    if (chunk_number == track_malloc_number) {
	track_chunk = chunk;
    }
    chunks_in_use++;
    addChunk(chunk);
    return chunk;
}

void
memShutdown()
{
    GHashTable *table;

    table = chunkTable();
    freeHashTable(table);
    hash_chunks = NULL;

    table = freeTable();
    freeHashTable(table);
    hash_frees = NULL;
}

void
curFree()
{
    fprintf(stderr, "curFree: %d\n", free_number);
}

/* A debugging tool.  If you want to figure out when the Nth memory
 * chunk is freed , call showFree(N) and memdebug() will be called at that
 * moment.  Set a breakpoint on memdebug and you can figure out what is
 * going on at the point of that memory de-allocation.  Note that this
 * is the Nth free, not the free of the Nth allocated chunk.
 */
void
showFree(int number_to_show)
{
    show_free_number = number_to_show;
}

/* A debugging tool.  If you want to figure out when memory chunk N
 * is allocated, call showMalloc(N) and memdebug() will be called at that
 * moment.  Set a breakpoint on memdebug and you can figure out what is
 * going on at the point of that memory allocation.
 */
void
showMalloc(int number_to_show)
{
    show_malloc_number = number_to_show;
}

/* A debugging tool.  
 * If you want to figure out when memory chunk N is freed (ie when the
 * Nth allocated chunk is freed), call showMalloc(N) and memdebug() will
 * be called at that moment.  Set a breakpoint on memdebug and you can
 * figure out what is going on at the point of that memory deallocation.
 */
void
trackMalloc(int number_to_show)
{
    track_malloc_number = number_to_show;
}

int
getMalloc()
{
    return chunk_number;
}

static void
skforget(void *ptr)
{
    //MEMPRINTF("-");
    if (ptr && (ptr == track_chunk)) {
	fprintf(stderr, "About to free chunk %p (malloc no %d)\n", 
		ptr, show_malloc_number);
	memdebug("FREEING TRACKED CHUNK");
	track_chunk = NULL;
    }
    free_number++;
    chunks_in_use--;
    delChunk(ptr);
}

extern boolean
checkChunk(void *chunk, void *check_for)
{
    char *keystr = malloc(20);
    GHashTable *chunk_hash = chunkTable();
    gpointer previous_contents = NULL;
    gpointer previous_key;

    sprintf(keystr, "%p", chunk);
    if (chunk_hash) {
	if (g_hash_table_lookup_extended(chunk_hash, (gpointer) keystr,
					 &previous_key, &previous_contents)) 
	{
	    /* Chunk is found */
	    free(keystr);
	    if (chunk == check_for) {
		fprintf(stderr, "Found chunk %p\n", chunk);
		return TRUE;
	    }
	    return FALSE;
	}
    }
    free(keystr);
    memdebug("checkChunk");
    RAISE(MEMORY_ERROR, newstr("checkChunk: Chunk %p not allocated", chunk));
}

#endif

void *
skalloc(size_t size)
{
    void *result;
    result = malloc(size);
    memchunks_incr(result);
    return result;
}

void 
skfree(void *ptr)
{
#ifdef MEM_DEBUG
    skforget(ptr);
#endif
    free(ptr);
}

void *
skrealloc(void *p, size_t size)
{
    void *result = realloc(p, size);
#ifdef MEM_DEBUG
    if (p != result) {
	delChunk(p);
	addChunk(result);
    }
#endif
    return result;
}

void
skitFreeMem()
{
    String *arg;
    Symbol *sym = symbolGet("current-timestamp");

    while (arg = read_arg()) {
	objectFree((Object *) arg, TRUE);
    }

    documentFreeMem();
    pgsqlFreeMem();
    freeSkitProcessors();
    freeStdTemplates();
    xsltCleanupGlobals();
    xmlCleanupParser();
    freeOptions();
    freeSymbolTable();
}


