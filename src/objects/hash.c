/**
 * @file   hash.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for manipulating hash cells.  The hash keys are always
 * standard C strings created from the key object.  The hash contents
 * are stored internally as a cons of (key_object . contents_object).
 */

#include <stdio.h>
#include <stdlib.h>
#include "../skit_lib.h"
#include "../exceptions.h"

#define DEBUG_HASH 0

/* TODO: ADD TESTS FOR hash->hash existence throughout, and raise an
 *  appropriate exception if it is missing.  The text should say
 *  something along the lines of "this hash has been rendered unusable
 *  by hashCopy".
 */

/* Key comparison predicate for skit hashes.  This is called indirectly
 * from the glib hash functions. */
static gboolean 
equalKey(gconstpointer obj1, gconstpointer obj2)
{
    // All keys are strings.
    if (obj1 == obj2) {
	return TRUE;
    }
    return streq((char *) obj1, (char *) obj2);
}

/* Memory freeing function for freeing the contents of hash
 * entries. This is called indirectly from the glib hash functions. */
static void
freeHashContents(gpointer contents)
{
    consFree((Cons *) contents, TRUE);
}

/* Memory freeing function for freeing the keys of hash entries.  This
 * is called indirectly from the glib hash functions. */
static void
freeHashKey(gpointer key)
{
    skfree((void *) key);
}

static boolean
isHash(Hash *hash)
{
    return hash->type == OBJ_HASH;
}

/* Create a skit hash object.  The boolean use_skalloc parameter allows
 * hashes to be created by the memory checking subsystem (in mem.c)
 * without it tripping over itself.  All other calls to this function
 * should provide TRUE for this parameter. */
Hash *
hashNew(boolean use_skalloc)
{
    Hash *hash;
    hash = (Hash *) (use_skalloc? skalloc(sizeof(Hash)): malloc(sizeof(Hash)));
    hash->type = OBJ_HASH;
    hash->hash = g_hash_table_new_full(g_str_hash, equalKey, 
				       freeHashKey, freeHashContents);
    return hash;
}

/* Get the contents that match key from the hash.  This returns the
 * stored contents which is an alist entry where the car is the key and
 * the cdr, the real contents.  */
static Object *
hashLookup(Hash *hash, char *key)
{
    return (Object *) g_hash_table_lookup((GHashTable *) hash->hash,
					  (gconstpointer) key);
}

/* key and contents are consumed by this call and will be freed
 * when the hash is destroyed.  If a hash is overwritten the old key is
 * freed and the old contents returned.  This allows us to easily
 * identify hash collisions and also to easily build lists where the
 * old contents are appended to the new.
 */
Object *
hashAdd(Hash *hash_in, Object *key, Object *contents)
{
    /* Make a string representation of the key object, and use that for
     * the key.  The contents will be a Cons-cell containing the real
     * key object and the real contents. */
    Hash *hash;
    char *keystr;
    Cons *cons;
    Object *previous_contents = NULL;
    char *previous_key;
    boolean already_exists;
    
    if (hash_in->type == OBJ_OBJ_REFERENCE) {
	hash = (Hash *) ((ObjReference *) hash_in)->obj;
    }
    else {
	hash = hash_in;
    }

    assert(isHash(hash), "hashAdd: hash is not a Hash");
    assert(isObject(key), "hashAdd: key is not an object");
    assert(isObject(contents), "hashAdd: contents is not an object");
    keystr = objectSexp(key);

    already_exists = g_hash_table_lookup_extended(
	(GHashTable *) hash->hash, (gpointer) keystr,
	(gpointer *) &previous_key, (gpointer *) &previous_contents);
	
    if (already_exists) {
	assert(isCons((Cons *) previous_contents), 
		      "hashAdd: Previous contents is not cons");
	// We use hash_table_steal rather than hash_table_remove in
	// order to retain control over memory management.
	if (!g_hash_table_steal((GHashTable *) hash->hash, 
				(gconstpointer) previous_key)) {
	    RAISE(GENERAL_ERROR, 
		  newstr("hashAdd: failed to remove existing hash entry"));
	}
	//dbgSexp(previous_contents);
	cons = (Cons *) previous_contents;
	//fprintf(stderr, "prev: %x\n", cons);
	//fprintf(stderr, "prev->car: %x\n", cons->car);
	//fprintf(stderr, "prev->cdr: %x\n", cons->cdr);
	//fprintf(stderr, "prev->cdr->car: %x\n", ((Cons *)cons->cdr)->car);
	//dbgSexp(((Cons *)cons->cdr)->car);
	//fprintf(stderr, "prev->cdr->car->car: %x\n", 
	//	((Cons *)((Cons *)cons->cdr)->car)->car);
	//fprintf(stderr, "prev->cdr->car->car->value: %x\n", 
	//	((String *)((Cons *)((Cons *)cons->cdr)->car)->car)->value);
	//fprintf(stderr, "prev->cdr->car->car->value: %s\n", 
	//	((String *)((Cons *)((Cons *)cons->cdr)->car)->car)->value);
	//fprintf(stderr, "prev->cdr->car->cdr: %x\n", 
	//	((Cons *)((Cons *)cons->cdr)->car)->cdr);
	previous_contents = cons->cdr;
	objectFree((Object *) cons->car, TRUE);
	objectFree((Object *) cons, FALSE);
	skfree((void *) previous_key);
    }
    cons = consNew(key, contents);

    g_hash_table_insert((GHashTable *) hash->hash,
                        (gpointer) keystr, (gpointer) cons);
    return previous_contents;
}

/* Create a hash from a Cons-cell alist, moving the objects from the
 * alist into the vector, and destroying the list. */
Hash *
toHash(Cons *cons)
{
    Hash *hash;
    Cons *entry;

    if (!consIsAlist(cons)) {
	RAISE(TYPE_MISMATCH, 
	      newstr("toHash: Invalid source for hash (expecting an alist)"));
    }
    hash = hashNew(TRUE);
    
    // Now, get each alist entry from the list and add it to the hash, 
    // destroying the list as we go.
    while (cons) {
	assert((cons->type == OBJ_CONS), "toHash: Not a cons cell");
    	entry = (Cons *) consPop(&cons);
	assert((entry->type == OBJ_CONS), "toHash: Invalid alist entry");
	// TODO: Maybe check result of addHash below
	(void) hashAdd(hash, entry->car, entry->cdr);
	objectFree((Object *) entry, FALSE);
    }
    return hash;
}

/* Get the object from the hash that matches the key.  */
Object *
hashGet(Hash *hashref, Object *key)
{
    char *strkey;
    Cons *contents;
    Hash *hash;
    if (hashref->type == OBJ_OBJ_REFERENCE) {
	hash = (Hash *) ((ObjReference *) hashref)->obj;
    }
    else {
	hash = hashref;
    }

    strkey = objectSexp(key);
    contents = (Cons *) hashLookup(hash, strkey);
    skfree(strkey);
    if (contents) {
	assert(contents->type == OBJ_CONS,
	       "hashGet: Expected contents to be Cons");
	return contents->cdr;
    }
    return NULL;
}


/* Return the object from hash that matches key, and delete the key.
 * This frees the original key. */
Object *
hashDel(Hash *hash, Object *key)
{
    char *strkey;
    Cons *contents;
    Object *result = NULL;
    gpointer orig_key;
    gpointer value;
    boolean found;

    strkey = objectSexp(key);
    found = g_hash_table_lookup_extended((GHashTable *) hash->hash,
					 (gconstpointer) strkey,
					 &orig_key, &value);
    if (found) {
	contents = (Cons *) value;
	assert(contents->type == OBJ_CONS,
	       "hashDel: Expected contents to be Cons");
	result = contents->cdr;
	contents->cdr = NULL;
	objectFree((Object *) contents, TRUE);
	if (!g_hash_table_steal((GHashTable *) hash->hash, 
				 (gconstpointer) strkey)) {
	    RAISE(GENERAL_ERROR, 
		  newstr("hashDel: failed to remove hash item \"%s\"\n",
			 strkey));
	}
	skfree(orig_key);
    }
    skfree(strkey);
    return result;
}


/* This is used by the g_hash_table_foreach call in the following
 * function.  It populates a skit vector (passed in param) with the
 * key of each hash entry.  */
static void
recordKeyFromHash(
    gpointer key,
    gpointer contents,
    gpointer param)
{
    // The keys passed to this function are real C-strings.  Before
    // placing them in the array, we should convert them to String
    // objects.  Note that we will be just copying the pointers and
    // not doing any real copies.  Remember this when it becomes time to
    // free the strings!

    String *str = stringNewByRef((char *) key);
    Vector *vector = (Vector *) param;

    (void) vectorPush(vector, (Object *) str);
}

/* Return a string representation of the hash.  This must be in a 
 * predictable order, so the keys are extracted and sorted before
 * extracting the contents for each key. */
char *
hashStr(Hash *hash)
{
    int elems;
    Vector *vector;
    char *objstr;
    char *discard;
    char *workstr = NULL;
    int i;
    String *key;
    Object *contents;

    assert((hash->type == OBJ_HASH), "hashStr: Not a hash");
    elems = g_hash_table_size(hash->hash);
    vector = vectorNew(elems);
    g_hash_table_foreach(hash->hash, recordKeyFromHash, (gpointer) vector);
    vectorStringSort(vector);

    for (i = 0; i  < vector->elems; i++) {
	key = (String *) vector->contents->vector[i];
	// contents will be a cons cell containing the key and contents
	// objects.
	contents = hashLookup(hash, key->value);
	stringFree(key, FALSE);  // Free the String object but not the 
	                         // string within it which is still in
	                         // use by the hash.

	objstr = objectSexp(contents);
	if (workstr) {
	    discard = workstr;
	    workstr = newstr("%s %s", workstr, objstr);
	    skfree(discard);
	    skfree(objstr);
	}
	else {
	    workstr = objstr;
	}
    }
    if (workstr) {
	discard = workstr;
	workstr = newstr("<%s>", workstr);
	skfree(discard);
    }
    else {
	workstr = newstr("<>");
    }
    vectorFree(vector, FALSE);
    return workstr;
}

/* This is called from g_hash_table_foreach to return an alist entry
 * matching the hash contents (including the key). */
static void
alistFromHash(
    gpointer key,
    gpointer contents,
    gpointer param)
{
    Cons **p_alist = (Cons **) param;
    Cons *cur_alist = *p_alist;
    Cons *contents_cons = (Cons *) contents;
    Object *real_key = contents_cons->car;
    Object *real_contents = contents_cons->cdr;
    Cons *new_entry = consNew(real_key, real_contents);
    Cons *new_head = consNew((Object *) new_entry, (Object *) cur_alist);
    *p_alist = new_head;
}


/* Generate an alist from the hash, containing references to keys and
 * contents.  The list will be dynamically allocated and all of its cons
 * cells must be freed, but the keys and contents must not. */
Cons *
hashToAlist(Hash *hash)
{
    Cons *alist = NULL;
    Cons **p_alist = &alist;

    g_hash_table_foreach(hash->hash, alistFromHash, (gpointer) p_alist);
    return alist;
}

static void
eachHashEntry(
    gpointer key,
    gpointer contents,
    gpointer params)
{
    HashEachFn *fn = (HashEachFn *) ((Cons *) params)->car;
    Object *param = ((Cons *) params)->cdr;
    Cons *hash_entry = (Cons *) contents;
    Object *actual_contents = ((Cons *) hash_entry)->cdr;
    Object *result = fn(hash_entry, param);

    /* If the result of fn is different from the parameter passed to it, 
     * we set the hash to the new value. */
    if (result != actual_contents) {
	((Cons *) hash_entry)->cdr = result;
    }
}


void
hashEach(Hash *hash, HashEachFn *fn, Object *arg)
{
    Cons params = {OBJ_CONS, (Object *) fn, arg};
    g_hash_table_foreach(hash->hash, eachHashEntry, (gpointer) &params);
}


static Object *
hashDropEntry(Cons *node_entry, Object *ignore)
{
    return NULL;
}


/* Free a skit hash.  */
void 
hashFree(Hash *hash, boolean free_contents)
{
    if (!free_contents) {
	hashEach(hash, &hashDropEntry, NULL);
    }
    if (hash->hash) {
	g_hash_table_destroy(hash->hash);
    }
    skfree((void *) hash);
}

/* This function is a bit of a lie.  Since the only time we should need
 * to copy a hash is when copying from a dynamically created expression,
 * what we do is create a new Hash record that contains the same
 * underlying hash, and then we remove that hash from the source */
Hash *
hashCopy(Hash *hash)
{
    Hash *result;
    result = (Hash *) skalloc(sizeof(Hash));
    result->type = OBJ_HASH;
    result->hash = hash->hash;
    hash->hash = NULL;
    return result;
}

int 
hashElems(Hash *hash)
{
    return (int) g_hash_table_size(hash->hash);
}


static Object *
addElemToVector(Cons *entry, Object *vector)
{
    Object *elem = entry->cdr;
    vectorPush((Vector *) vector, elem);
    return elem;
}


Vector *
vectorFromHash(Hash *hash)
{
    int elems = hashElems(hash);
    Vector *vector = vectorNew(elems);
    hashEach(hash, &addElemToVector, (Object *) vector);
    return vector;
}

static Object *
checkHashEntry(Cons *entry, Object *params)
{
    if (checkObj((Object *) entry, ((Cons *)params)->car)) {
        /* Just set to a non-null value */
	((Cons *) params)->cdr = (Object *) entry;  
	printSexp(stderr, "...within hash_entry: ", (Object *) entry);
    }
    return entry->cdr;
}

boolean
checkHash(Hash *hash, void *chunk)
{
    Cons *params = consNew(chunk, NULL);
    boolean found;
    hashEach(hash, checkHashEntry, (Object *) params);
    found = params->cdr != NULL;
    objectFree((Object *) params, FALSE);
    if (found) {
	fprintf(stderr, "...within hash <NOT SHOWN>");
    }
    if (checkChunk(hash, chunk)) {
	fprintf(stderr, "...in hash <NOT SHOWN>");
	return TRUE;
    }
    return found;
}
