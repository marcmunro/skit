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

    if (deps = node->deps) {
	EACH(deps, i) {
	    dep = (DagNode *) ELEM(deps, i);
	    node->cur_dep = i;
	    tsort_node(nodes, dep, results);
	}
    }
}


static Vector *
nodesInCycle(DagNode *node)
{
    Vector *nodes_in_cycle = vectorNew(10);
    DagNode *this = node;
    do {
	vectorPush(nodes_in_cycle, (Object *) this);
	this = (DagNode*) ELEM(this->deps, this->cur_dep);
    } while (this != node);
    return nodes_in_cycle;
}

static Document *
applyDDL(Document *doc)
{
    Document *ddl_processor = getDDLProcessor();
    Document *result;
    BEGIN {
	docStackPush(doc);
	applyXSL(ddl_processor);
    }
    EXCEPTION(ex) {
	result = docStackPop();
	objectFree((Object *) result, TRUE);
	RAISE();
    }
    END;
    result = docStackPop();
    return result;
}

static boolean
breakCycle(DagNode *node)
{
    Vector *nodes_in_cycle = nodesInCycle(node);
    Document *doc = docFromVector(NULL, nodes_in_cycle);
    Vector *ddl_nodes;
    int i;
    DagNode *this;
    boolean result = FALSE;

    doc = applyDDL(doc);
    ddl_nodes = dagNodesFromDoc(doc->doc->children);

    EACH(ddl_nodes, i) {
	this = (DagNode *) ELEM(ddl_nodes, i);
	if (!isPrintable(this->dbobject->children)) {
	    /* The DDL for this node does nothing useful, so we can
	     * remove it from the cycle.  We do this by setting the
	     * node's build_type to DEACTIVATED_NODE. 
	     */
	    this = (DagNode *) ELEM(nodes_in_cycle, i);
	    this->build_type = DEACTIVATED_NODE;
	    result = TRUE;
	}
    }

    objectFree((Object *) nodes_in_cycle, FALSE);
    objectFree((Object *) doc, TRUE);
    objectFree((Object *) ddl_nodes, TRUE);
    return result;
}

static void
tsort_node(Vector *nodes, DagNode *node, Vector *results)
{
    DagNode *cycle_node;
    boolean cyclic_exception = FALSE;
    char *errmsg;
    char *tmpmsg;

    switch (node->status) {
    case VISITING:
	RAISE(TSORT_CYCLIC_DEPENDENCY, 
	      newstr("(%s) %s", nameForBuildType(node->build_type),
		     node->fqn->value), node);
    case UNVISITED: 
    case RESOLVED: 
	BEGIN {
	    node->status = VISITING;
	    if (node->build_type != DEACTIVATED_NODE) {
		tsort_deps(nodes, node, results);
	    }
	    vectorPush(results, (Object *) node);
	}
	EXCEPTION(ex);
	WHEN(TSORT_CYCLIC_DEPENDENCY) {
	    cyclic_exception = TRUE;
	    cycle_node = (DagNode *) ex->param;
	    errmsg = newstr("%s", ex->text);
	}
	END;
	if (cyclic_exception) {
	    if (node == cycle_node) {
		/* We are at the start of the cyclic dependency.
		 * Set errmsg to describe this, and reset cycle_node
		 * for the RAISE below. */ 
		if (breakCycle(node)) {
		    /* We have broken the cycle, so we retry this node,
		     * the simplest way possible. */
		    node->status = UNVISITED;
		    skfree(errmsg);
		    tsort_node(nodes, node, results);
		    return;
		}
		tmpmsg = newstr("Cyclic dependency detected: (%s) %s->%s", 
				nameForBuildType(node->build_type),
				node->fqn->value, errmsg);
		cycle_node = NULL;
	    }
	    else  {
		/* We are somewhere in the cycle of deps.  Add the
		 * current node to the error message. */ 
		tmpmsg = newstr("(%s) %s->%s", 
				nameForBuildType(node->build_type),
				node->fqn->value, errmsg);
	    }
	    
	    skfree(errmsg);
	    node->status = UNVISITED;
	    RAISE(TSORT_CYCLIC_DEPENDENCY, tmpmsg, cycle_node);
	}
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
tsort(Document *doc)
{
    Vector *volatile nodes = NULL;
    Vector *results = NULL;
    Vector *tmp;
    int i;
    DagNode *node;
    BEGIN {
	nodes = dagFromDoc(doc);
	results = simple_tsort(nodes);
	tmp = results;
	results = vectorNew(tmp->elems);
	EACH(tmp, i) {
	    node = (DagNode *) ELEM(tmp, i);
	    if (node->build_type == DEACTIVATED_NODE) {
		objectFree((Object *) node, TRUE);
	    }
	    else {
		vectorPush(results, (Object *) node);
	    }
	}
	objectFree((Object *) tmp, FALSE);
    }
    EXCEPTION(ex);
    WHEN_OTHERS {
	objectFree((Object *) nodes, TRUE);
	RAISE();
    }
    END;
    objectFree((Object *) nodes, FALSE);
    //fprintf(stderr, "\n\n");
    //dbgSexp(results);
    return results;
}

