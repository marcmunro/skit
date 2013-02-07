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


static void
tsort_node(Vector *nodes, DagNode *node, Vector *results);

static void
tsort_deps(Vector *nodes, DagNode *node, Vector *results)
{
    Vector *deps;
    int i;
    DagNode *dep;

    if (deps = node->forward_deps) {
	EACH(deps, i) {
	    dep = (DagNode *) ELEM(deps, i);
	    tsort_node(nodes, dep, results);
	}
    }
}


static void
tsort_node(Vector *nodes, DagNode *node, Vector *results)
{
    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("Cyclic dep found in %s", node->fqn->value), node);
    case UNVISITED: 
    case RESOLVED: 
	BEGIN {
	    node->status = VISITING;
	    tsort_deps(nodes, node, results);
	    vectorPush(results, (Object *) node);
	}
	EXCEPTION(ex) {
	    node->status = UNVISITED;
	    RAISE();
	}
	END;
	node->status = VISITED;
	break;
    case VISITED: 
	break;
    default:
	RAISE(TSORT_ERROR,
		  newstr("Unexpected status for dagnode %s: %d",
			 node->fqn->value, node->status));
    }
}

Vector *
simple_tsort(Vector *nodes)
{
    Vector *volatile results = 
	vectorNew(nodes->elems + 10); // Allow for expansion
    DagNode *node;
    int i;
    BEGIN {
	EACH(nodes, i) {
	    node = (DagNode *) ELEM(nodes, i);
	    tsort_node(nodes, node, results);
	}
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) results, FALSE);
	RAISE();
    }
    END;
    return results;
}

/* Do the sort. 
*/
Vector *
gensort(Document *doc)
{
    Vector *volatile nodes = NULL;
    Vector *results = NULL;

    BEGIN {
	nodes = dagFromDoc(doc);
	results = simple_tsort(nodes);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes, TRUE);
	RAISE();
    }
    END;
    objectFree((Object *) nodes, FALSE);
    return results;
}

