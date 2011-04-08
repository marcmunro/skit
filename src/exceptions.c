/**
 * @file   exceptions.c
 * \code
 *     Copyright (c) 2009, 2010, 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Signal/exception handling functions for skit
 * TODO Some documentation describing how to use this.
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include "skit_lib.h"
#include "exceptions.h"

static Exception *cur_exception_handler = NULL;
static int handlers_in_use = 0;

/* Return a static string that describes the numeric execption */
static char *
exceptionName(int e)
{
    switch (e) {
    case SIGTERM:	          return "SIGTERM";
    case LIST_ERROR:		  return "LIST_ERROR";
    case TYPE_MISMATCH:		  return "TYPE_MISMATCH";
    case UNKNOWN_TOKEN:           return "UNKNOWN_TOKEN";
    case UNHANDLED_OBJECT_TYPE:   return "UNHANDLED_OBJECT_TYPE";
    case FILEPATH_ERROR:          return "FILEPATH_ERROR";
    case MISSING_VALUE_ERROR:     return "MISSING_VALUE_ERROR";
    case OUT_OF_BOUNDS:           return "OUT_OF_BOUNDS";
    case PARAMETER_ERROR:         return "PARAMETER_ERROR";
    case NOT_IMPLEMENTED_ERROR:   return "NOT_IMPLEMENTED_ERROR";
    case XML_PROCESSING_ERROR:    return "XML_PROCESSING_ERROR";
    case GENERAL_ERROR:           return "GENERAL_ERROR";
    case REGEXP_ERROR:            return "REGEXP_ERROR";
    case SQL_ERROR:               return "SQL_ERROR";
    case UNPROCESSED_INCLUSION:   return "UNPROCESSED_INCLUSION";
    case MEMORY_ERROR: 	          return "MEMORY_ERROR";
    case ASSERTION_FAILURE: 	  return "ASSERTION_FAILURE";
    case XPATH_EXCEPTION: 	  return "XPATH_EXCEPTION";
    case INDEX_ERROR:        	  return "INDEX_ERROR";
    case TSORT_ERROR:        	  return "TSORT_ERROR";
    case TSORT_CYCLIC_DEPENDENCY: return "TSORT_CYCLIC_DEPENDENCY";
    default: return "UNKNOWN EXCEPTION";
    }
}

/* Free an exception object.  This is called on exit from an exception
 * handler. */
static void
exceptionFree(Exception *ex)
{
    assert(ex->type == OBJ_EXCEPTION,
	   "exceptionFree: ex is not an Exception");
    if (ex->text) {
	skfree(ex->text);
    }
    if (ex->prev) {
	exceptionFree(ex->prev);
    }
    if (ex->backtrace) {
	skfree(ex->backtrace);
    }
    if (ex->file_raised) {
	skfree(ex->file_raised);
    }
    if (ex->file_caught) {
	skfree(ex->file_caught);
    }

    skfree((void *) ex);
    handlers_in_use--;
    return;
}

/* Create a new exception object, recording the position in the file
 * where the BEGIN exists, and the depth of nesting of this handler
 * (the outermost exception block is at depth 1) */
Exception *
exceptionNew(char *file, int line)
{
    Exception *ex = (Exception *) skalloc(sizeof(Exception));
    memset(ex, 0, sizeof(Exception));
    ex->type = OBJ_EXCEPTION;
    ex->file_caught = newstr("%s", file);
    ex->line_caught = line;
    ex->depth = ++handlers_in_use;
    return ex;
} 

/* Push a new exception handler onto the exception stack.  The exception
 * stack provides the nesting of exception handlers. */
void
exceptionPush(Exception *ex)
{
    ex->prev = cur_exception_handler;
    cur_exception_handler = ex;
}

/* Pop the last exception handler from the exception stack and free it. */
void
exceptionPop()
{
    Exception *ex = cur_exception_handler;
    cur_exception_handler = ex->prev;
    ex->prev = NULL;
    exceptionFree(ex);
}

/* Create a string describing the backtrace for an exception.  This
 * makes use of any backtrace string for a previous exception, if
 * provided. */
static char *
backtraceStr(Exception *ex, Exception *prev)
{
    char *result;
    char *backtrace;
    char *caught;
    char *raised;

    caught = newstr("Caught at %s:%d", ex->file_caught, ex->line_caught);

    if (ex->file_raised) {
	raised = newstr("Raised at %s:%d", ex->file_raised,
			ex->line_raised);
    }
    else {
	raised = newstr("Raised at unknown location");
    }
    
    if (prev && prev->backtrace) {
	backtrace = prev->backtrace;
    }
    else {
	backtrace = "";
    }
    result = newstr("--> Exception %d:  %s\n    %s\n    %s\n%s", 
		    ex->signal, ex->text, caught, raised, backtrace);
    
    skfree(caught);
    skfree(raised);
    return result;
}

/* Handle an exception that has no handler. */
static void
unhandled_exception(char *msg, int signal, char *file, int line, char *txt)
{
    char *unhandled;
    unhandled = newstr(msg, signal, exceptionName(signal), file, line, txt);
    skfree(txt);
    fprintf(stderr, "%s\n", unhandled);
    raise(SIGTERM);
}

/* Raise a new exception.  Called indirectly from the RAISE() macro.
 */
static void 
doRaise(char *file, int line, int signal, char *txt, Object *param)
{
    Exception *exhandler = exceptionCurHandler();
    if (exhandler) {
	exhandler->signal = signal;
	exhandler->text = txt;
	exhandler->param = param;
	exhandler->file_raised = newstr("%s", file);
	exhandler->line_raised = line;
	exhandler->backtrace = backtraceStr(exhandler, NULL);
	longjmp(exhandler->handler, signal);
    }
    unhandled_exception("UNHANDLED EXCEPTION %d(%s) in %s at line %d\n%s\n",
			signal, file, line, txt);
}

/* Re-Raise an exception.  This happens from within an exception handler
 * when RE-RAISE is called, or when an exception is not handled by the
 * handler.
 */
static void
doReRaise(char *file, int line)
{
    int signal;
    char *txt;
    Exception *ex = exceptionCurHandler();;
    Exception *new_handler = ex->prev;

    if (new_handler) {
	new_handler->file_raised = newstr("%s", file);
	new_handler->line_raised = line;
	new_handler->signal = ex->signal;
	signal = ex->signal;
	if (ex->text) {
	    new_handler->text = newstr("%s", ex->text);
	    new_handler->backtrace = backtraceStr(new_handler, ex);
	}
	new_handler->param = ex->param;
	exceptionPop();

	longjmp(new_handler->handler, signal);
    }

    signal = ex->signal;
    if (ex->text) {
	txt = newstr("%s", ex->text);
    }
    else {
	txt = newstr("NO EXCEPTION TEXT");
    }
    exceptionPop();
    unhandled_exception("Unhandled Exception %d(%s): raised at %s:%d\n%s", 
			signal, file, line, txt);

}

/* Raise an exception.  Called directly from the RAISE() macro.
 */
void 
exceptionRaise(char *file, int line, ...)
{
    int signal;
    char *txt;
    va_list params;
    Object *exparam = NULL;
    Exception *exhandler = exceptionCurHandler();

    va_start(params, line);
    if (signal = va_arg(params, int)) {
	txt = (char *) va_arg(params, char *);
	exparam = (Object *) va_arg(params, Object *);
    }
    va_end(params);

    if (exhandler) {
	if (signal) {
	    /* We are raising a new exception */
	    if (exhandler->signal) {
		/* We are in the EXCEPTION block */
		exceptionPop();
	    }
	    doRaise(file, line, signal, txt, exparam);
	}
	else {
	    if (exhandler->signal) {
		doReRaise(file, line);
	    }
	    doRaise(file, line, UNKNOWN_EXCEPTION, 
		    newstr("Stupid attempt to re-raise an exception outside "
			   "of an exception block."), NULL);
	}
    }
    else {
	/* No exception handler defined. */
	if (!signal) {
	    signal = UNKNOWN_EXCEPTION;
	    txt = newstr("Stupid attempt to re-raise an exception outside "
			 "of an exception block.");
	}
	unhandled_exception("Unhandled Exception %d(%s): raised at %s:%d\n%s", 
			    signal, file, line, txt);
    }
}

/* Return the current exception handler (the head of the exception stack). */
Exception *
exceptionCurHandler()
{
    return cur_exception_handler;
}

/* Skit function for handling signals.  This allows an signal to cause a
 * skit exception.
 */
static void
skit_signal_handler(int sig)
{
    Exception *exh = cur_exception_handler;
    if (exh) {
	exh->text = newstr("Caught signal %s (%d)", exceptionName(sig), sig);
	siglongjmp(exh->handler, sig);
    }

    /* If the signal was not handled by our exception handling
     * subsystem, re-raise it for the standard signal handler to deal
     * with. */
    
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Either exit gracefully if the exception has been handled, or
 * re-raise the exception if it has not been.
 */
void
exceptionEnd(char *file, int line)
{
    Exception *ex = exceptionCurHandler();

    if (ex->signal && (!ex->caught)) {
	doReRaise(file, line);
    }
    exceptionPop();
}


/* Define signal handling, using the skit exception system, for a number
 * of standard signals.  This allows things like division by zero to be
 * trapped as exceptions.
 */
void
skit_register_signal_handler()
{
    signal(SIGFPE, &skit_signal_handler);
    signal(SIGINT, &skit_signal_handler);
    signal(SIGTERM, &skit_signal_handler);
}
