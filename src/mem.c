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
#include "skit_lib.h"

// TODO: Make the debug stuff below conditionally compiled.

static int chunks_in_use = 0;
static int chunk_number = 0;
static int free_number = 0;
static int show_free_number = 0;
static int show_malloc_number = 0;

static GHashTable *hash_chunks = NULL;
static GHashTable *hash_frees = NULL;


static void
debug(char *label)
{
    fprintf(stderr, "DEBGUG: %s\n", label);
}

static gboolean 
equalKey(gconstpointer obj1, gconstpointer obj2)
{
    // All keys are strings.
    if (obj1 == obj2) {
	return TRUE;
    }
    return streq((char *) obj1, (char *) obj2);
}

static void
freeHashKey(gpointer key)
{
    free(key);
}

static GHashTable *
newHashTable()
{
    GHashTable *tbl;
    tbl = g_hash_table_new_full(g_str_hash, equalKey, freeHashKey, NULL);
    return tbl;
}

static void
freeHashTable(GHashTable *hash)
{
    g_hash_table_destroy(hash);
}


static GHashTable *
chunkTable()
{
    if (!hash_chunks) {
	hash_chunks = newHashTable();
    }

    return hash_chunks;
}

static GHashTable *
freeTable()
{
    if (!hash_frees) {
	char *x;
	hash_frees = newHashTable();
	(void) chunkTable();  
	x = newstr("x");      // Alloc and free a string to initialise
	skfree(x);            // usage of the the glib hash.  This
			      // should make supression of valgrind
			      // errors easier.
    }

    return hash_frees;
}

static char *
hash_name(GHashTable *hash)
{
    if (hash == freeTable()) {
	return "freeTable";
    }
    if (hash == chunkTable()) {
	return "chunkTable";
    }
    return "UNKNOWN HASH TABLE - BADBADBADBADBADBADBADBADBADBAD";
}

// This is nasty and probably not portable.  It's debug code though so I
// don't much care.
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

static void
printObj(Object *obj)
{
    if (isObject(obj)) {
	switch (obj->type) {
	case OBJ_INT4: 
	    fprintf(stderr, "int4: %d\n", ((Int4 *) obj)->value); 
	    break;
	case OBJ_STRING: 
	    fprintf(stderr, "string: %s\n", ((String *) obj)->value); 
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

// Move the record of a chunk of memory from one hash to another.  The
// result contains the previous contents, if present.  This does the
// heavy lifting for addChunk and delChunk, and can be also used to
// free hash table entries.  TODO: Make output interface better, we are
// overloading the result in nasty ways.
static int
moveChunk(GHashTable *from_hash, GHashTable *to_hash, void *chunk)
{
    char *keystr = malloc(20);
    gpointer previous_contents = NULL;
    gpointer previous_contents2;
    gpointer previous_key;

    sprintf(keystr, "%p", chunk);
    // Get the existing value, if any
    if (from_hash) {
	if (g_hash_table_lookup_extended(from_hash, (gpointer) keystr,
					 &previous_key, &previous_contents)) 
	{
	    // There is an existing value, so remove it.
	    g_hash_table_remove(from_hash, previous_key);
	}
    }
    if (to_hash) {
	if (g_hash_table_lookup_extended(to_hash, (gpointer) keystr,
					 &previous_key, &previous_contents2)) 
	{
	    // This is bad.  The target hash already contains this
	    // entry.
	    return -1;
	}
	// Add the entry to the target hash.
	g_hash_table_insert(to_hash, (gpointer) keystr, 
			    (gpointer) chunk_number);
    }
    else {
	free(keystr);
    }
    return (int) previous_contents;
}

// Record the allocation of a chunk of free memory.  The chunk_number is
// stored in the hash, indexed by a string representing the address.
// If any chunks remain unfreed, their details can be printed using
// showChunks().
//
static void
addChunk(void *chunk)
{
    int previous = moveChunk(freeTable(), chunkTable(), chunk);
//    fprintf(stderr, "ALLOCED: %p\n", chunk);
    if (previous == -1) {
	fprintf(stderr, "ARGGGGGGHHHHH(add)!\n");
	abort();
    }
}

// Record the de-allocation of a chunk of free memory.  The existing
// entry in the chunks hash is removed and a new entry is added to the
// frees hash, indexed by the address (as a string) and containing the
// current value of free_number.  If an attempt is made to free the same
// memory again, we can tell when the original free was done.  This
// should aid debugging.
//
static void
delChunk(void *chunk)
{
    int previous = moveChunk(chunkTable(), freeTable(), chunk);
    //if (previous == 2396) {
    //	debug("FREEING 2396");
    //	printObj((Object *) chunk);
    //}
//    fprintf(stderr, "FREED: %p\n", chunk);
    if (previous == -1) {
	debug("XX");
	fprintf(stderr, "DelChunk %p already freed!\n", chunk);
	abort();
    }
    if (!previous) {
	debug("YY");
	fprintf(stderr, "DelChunk %p not allocated!\n", chunk);
	//abort();
    }
}

// Show the currently allocated objects in as much detail as possible.
static void
showChunk(
    gpointer key,
    gpointer contents,
    gpointer param)
{
    void *ptr;
    Object *obj;
    // The keys passed to this function are real C-strings.  Before
    // placing them in the array, we should convert them to String
    // objects.  Note that we will be just copying the pointers and
    // not doing any real copies.  Remember this when it becomes time to
    // free the strings!

    fprintf(stderr, "Chunk %d not freed: %s.", (int) contents, (char *) key);
    ptr = toPtr((char *) key);

    printObj((Object *) ptr);
}

// Print the contents of all non-free'd chunks
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
    MEMPRINTF("+");
    chunk_number++;
    /* Uncomment the following with a specific chunk_number if you want
     * use gdb to see where a chunk is being created (use set break
     * debug in gdb) */
    //if (chunk_number == 2364) debug("2364");
    //if (chunk_number == 1889) debug("1889");
    //if (chunk_number == 1890) debug("1890");
    
    if (chunk_number == show_malloc_number) {
	fprintf(stderr, "Allocating chunk no %d\n", chunk_number);
    }
    chunks_in_use++;
    addChunk(chunk);
    return chunk;
}


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
//    fprintf(stderr, "FREEING %p\n", ptr);
    MEMPRINTF("-");
    free_number++;
    if (free_number == show_free_number) {
	fprintf(stderr, "Freeing number %d\n", free_number);
	printObj((Object *) ptr);
    }
    chunks_in_use--;
    delChunk(ptr);
    free(ptr);
}

void
memTrace(char *str)
{
    fprintf(stderr, "MEM: %s (%d)\n", str, chunks_in_use);
}

void
curFree()
{
    fprintf(stderr, "curFree: %d\n", free_number);
}

void
showFree(int number_to_show)
{
    show_free_number = number_to_show;
}

void
showMalloc(int number_to_show)
{
    show_malloc_number = number_to_show;
}

void
skitFreeMem()
{
    String *arg;

    while (arg = read_arg()) {
	objectFree((Object *) arg, TRUE);
    }

    freeSkitProcessors();
    xsltCleanupGlobals();
    xmlCleanupParser();
    freeStdTemplates();
    freeSymbolTable();
    freeCoreOptions();
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

int
curchunk()
{
    return chunk_number;
}
