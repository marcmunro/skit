/**
 * @file   tsort.c
 * \code
 *     Copyright (c) 2010, 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for performing topological sorts.
 */

#include <string.h>
#include "skit_lib.h"
#include "exceptions.h"


/* Add a DagNode to a hash of DagNodes, keyed by the node's fqn attribute.
 */
static void
addNodeToHash(Hash *hash, Node *node, DagNodeBuildType build_type)
{
    DagNode *dagnode = dagnodeNew(node, build_type);
    String *key = stringDup(dagnode->fqn);
    Object *old;

    dagnode->build_type = build_type;

    if (old = hashAdd(hash, (Object *) key, (Object *) dagnode)) {
	objectFree(old, TRUE);
	RAISE(GENERAL_ERROR, 
	      newstr("doAddNode: duplicate node \"%s\"", key->value));
    }
}

static DagNodeBuildType
buildTypeFor(Node *node)
{
    Object *do_drop;
    String *diff;
    String *fqn;
    char *errmsg;
    DagNodeBuildType build_type;

    if (!streq(node->node->name, "dbobject")) {
	RAISE(TSORT_ERROR, newstr("buildTypeFor: node is not a dbobject"));
    }
	      
    diff = nodeAttribute(node->node, "diff");

    if (diff) {
	if (streq(diff->value, DIFFSAME)) {
	    build_type = EXISTS_NODE;
	}
	else if (streq(diff->value, DIFFKIDS)) {
	    build_type = EXISTS_NODE;
	}
	else if (streq(diff->value, DIFFNEW)) {
	    build_type = BUILD_NODE;
	}
	else if (streq(diff->value, DIFFGONE)) {
	    build_type = DROP_NODE;
	}
	else if (streq(diff->value, DIFFDIFF)) {
	    build_type = DIFF_NODE;
	}
	else {
	    fqn = nodeAttribute(((Node *) node)->node, "fqn");
	    errmsg = newstr(
		"buildTypeFor: unexpected diff type \"%s\" in %s", 
		diff->value, fqn->value);
	    objectFree((Object *) diff, TRUE);
	    objectFree((Object *) fqn, TRUE);
	    RAISE(TSORT_ERROR, errmsg);
	}
    }
    else {
	do_drop = dereference(symbolGetValue("drop"));
	if (dereference(symbolGetValue("build"))) {
	    if (do_drop) {
		build_type = BUILD_AND_DROP_NODE;
	    }
	    build_type = BUILD_NODE;
	}
	else if (do_drop) {
	    build_type = DROP_NODE;
	}
	else {
	    fqn = nodeAttribute(((Node *) node)->node, "fqn");
	    errmsg = newstr(
		"buildTypeFor: cannot identify build type for node %s", 
		diff->value, fqn->value);
	    objectFree((Object *) fqn, TRUE);
	    objectFree((Object *) diff, TRUE);
	    RAISE(TSORT_ERROR, errmsg);
	}
    }
    objectFree((Object *) diff, TRUE);
    return build_type;
}

/* A TraverserFn to identify dbobject nodes, adding them as Dagnodes to
 * our hash.
 */
static Object *
dagnodesToHash(Object *this, Object *hash)
{
    Node *node = (Node *) this;
    DagNodeBuildType build_type;
    if (streq(node->node->name, "dbobject")) {
	if ((build_type = buildTypeFor(node)) == BUILD_AND_DROP_NODE) {
	    addNodeToHash((Hash *) hash, node, BUILD_NODE);
	    addNodeToHash((Hash *) hash, node, DROP_NODE);
	}
	else {
	    addNodeToHash((Hash *) hash, node, build_type);
	}
    }
    return NULL;
}

/* Build a hash of dagnodes from the provided document.  The hash
 * may contain one build node and one drop node per database object,
 * depending on which build and drop options have been selected.
 */
static Hash *
dagnodesFromDoc(Document *doc)
{
    Hash *daghash = hashNew(TRUE);


    BEGIN {
	(void) xmlTraverse(doc->doc->children, &dagnodesToHash, 
			   (Object *) daghash);
    }
    EXCEPTION(ex) {
	objectFree((Object *) daghash, TRUE);
    }
    END;
    return daghash;
}

/* A HashEachFn that, if our DagNode contains pqns, adds it to our pqn hash.
 */
static Object *
addPqnEntry(Cons *node_entry, Object *param)
{
    DagNode *node = (DagNode *) node_entry->cdr;
    Hash *pqnhash = (Hash *) param;
    xmlChar *base_pqn  = xmlGetProp(node->dbobject, "pqn");
    String *pqn;
    Cons *entry;

    if (base_pqn) {
	switch (node->build_type) {
	case EXISTS_NODE:
	    pqn = stringNewByRef(newstr("exists.%s", base_pqn));
	    break;
	case BUILD_NODE:
	    pqn = stringNewByRef(newstr("build.%s", base_pqn));
	    break;
	case DROP_NODE:
	    pqn = stringNewByRef(newstr("drop.%s", base_pqn));
	    break;
	case DIFF_NODE:
	    pqn = stringNewByRef(newstr("diff.%s", base_pqn));
	    break;
	default:
	    RAISE(TSORT_ERROR,
		  newstr("Unexpected build_type for dagnode %s",
			 node->fqn->value));
	}

	if (entry = (Cons *) hashGet(pqnhash, (Object *) pqn)) {
	    dbgSexp(pqnhash);
	    RAISE(NOT_IMPLEMENTED_ERROR,
		      newstr("We have two nodes with matching pqns.  "
			  "Add the new node to the match"));
	}
	else {
	    entry = consNew((Object *) objRefNew((Object *) node), NULL);
	    hashAdd(pqnhash, (Object *) pqn, (Object *) entry);
	}
	xmlFree(base_pqn);
    }
    return (Object *) node;
}


/* Build a hash of dagnodes keyed by pqn.  Each hash entry is a list of
 * all DagNodes matching the pqn.
 */
static Hash *
makePqnHash(Hash *allnodes)
{
    Hash *pqnhash = hashNew(TRUE);
    hashEach(allnodes, &addPqnEntry, (Object *) pqnhash);
    return pqnhash;
}

/* Do the sort. 
 * This is going to be an eventual replacement for gensort.  For now it
 * is run before the real gensort.
*/
Vector *
gensort2(Document *doc)
{
    Hash *dagnodes = NULL;
    Hash *pqnhash = NULL;

    fprintf(stderr, "GENSORT2\n");

    BEGIN {
	dagnodes = dagnodesFromDoc(doc);
	pqnhash = makePqnHash(dagnodes);
	recordParentage(doc, dagnodes);
	recordDependencies(doc, dagnodes, pqnhash);
    }
    EXCEPTION(ex);
    FINALLY {
	objectFree((Object *) dagnodes, TRUE);
	objectFree((Object *) pqnhash, TRUE);
    }
    END;
    return NULL;
}

