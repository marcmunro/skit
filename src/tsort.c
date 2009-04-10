/**
 * @file   tsort.c
 * \code
 *     Copyright (c) 2009 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Provides functions for performing a topological sort.
 */

/* Marc's traversal cost-optimised tsort algorithm:
 * Each DagNode contains a list of dependencies and dependents
 * The sort runs in a number of passes.  Each pass runs for 
 * given starting node (null for the first pass).
 * pass(current_node, nodelist)
 *   generate a list of nodes that have no dependencies
 *   remove those nodes from nodelist
 *   sort the list by parent, type and name
 *     (parent == current node sorts first)
 *   count = 0
 *   repeat
 *     kids = 0
 *     for each node in the list
 *       append node to results
 *       count++
 *       kids += pass(node, nodelist)
 *     end
 *     count += kids
 *   until kids = 0
 *   return count
 * end   
 *   
 */

#ifdef NOWT

static void
vectorRemoveObj(Vector *list, Object *obj)
{
    int i;

    if (list) {
	for (i = 0; i < list->elems; i++) {
	    if (obj == list->vector[i]) {
		vectorRemove(list, i);
	    }
	}
    }
}

static void
disconnect(DagNode *cur, DagNode *from)
{
    if (from) {
	vectorRemoveObj(cur->dependencies, (Object *) from);
	vectorRemoveObj(from->dependents, (Object *) cur);
    }
}

static boolean
hasDependencies(DagNode *cur, DagNode *ignore)
{
    int i;
    DagNode *dep;

    if (!cur->dependencies) {
	return FALSE;
    }

    for (i = 0; i < cur->dependencies->elems; i++) {
	dep = (DagNode *) cur->dependencies->vector[i];
	if (dep == ignore) {
	    continue;  /* Don't count this dependency */ 
	}
	if (dep->status == DAGNODE_SORTED) {
	    /* Dependencies on nodes already dealt with are no longer
	     * relevant */
	    disconnect(cur, dep);
	    continue;
	}
	/* Get here if there is a relevant dependency. */
	return TRUE;
    }
    return FALSE;
}

static Vector *
getBuildNodes(DagNode *parent, Hash *allnodes)
{
    /* Return a list of nodes, from alnnodes, that have no dependencies
     * on unbuilt nodes, or are dependent only on parent.  */

    Vector *nodes = vectorElemsFromHash(allnodes);
    Vector *results = vectorNew(nodes->elems);
    DagNode *cur;
    int i;

    for (i = 0; i < nodes->elems; i++) {
	cur = (DagNode *) nodes->vector[i];
	if (cur->status == DAGNODE_READY) {
	    if (!hasDependencies(cur->dependencies, parent)) {
		disconnect(cur, parent);
		cur->status = DAGNODE_SORTING;
		vectorPush(results, (Object *) cur);
	    }
	}
    }
    objectFree(nodes, FALSE);

    return results;
}

static void
sortBuildNodes(Vector *build_nodes, DagNode *parent)
{
    /* Sort the build_nodes into an appropriate order */
}

static int
tsortOnePass(DagNode *current, Hash *allnodes, 
	     Vector *results)
{
    Vector *build_nodes = getBuildNodes(current, allnodes);
    int i;
    int nodes_built = 0;
    int descendants_built;
    boolean first_pass = TRUE;
    boolean moretodo;
    DagNode this;

    sortBuildNodes(build_nodes, current);
    do {
	moretodo = FALSE;
	for (i = 0; i < build_nodes->elems; i++) {
	    this = (DagNode *) build_nodes->vector[i];
	    vectorPush(results, (Object *) this);
	    this->status = DAGNODE->SORTED;
	    if (first_pass) {
		nodes_built++;
	    }
	    descendants_built = 0;
	    if (this->dependencies) {
		descendants_built += tsortOnePass(this, remaining, 
						  allnodes, results);
	    }
	    if (this->dependencies) {
		moretodo = TRUE;
	    }
	    else {
		disconnect(this, current);
	    }
	}
	nodes_built += descendants_built;
	first_pass = FALSE;
    } while (moretodo);
    return nodes_built;
}


/**
 * Takes a DAG recorded in a hash, and returns a list.  Each DagNode in
 * allnodes must have status DAGNODE_READY.
 */
Vector *
tsort(Hash *allnodes)
{
    Vector *results;
    int     elems;
    elems = hashElems(allnodes);
    results = vectorNew(elems);
    (void) tsortOnePass(NULL, allnodes, results);
    return results;
}

#endif
