/**
 * @file   exceptions.h
 * \code
 *     Copyright (c) 2009, 2010, 2011 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Definitions for exception handling.
 */

#include <setjmp.h>
#include <glib.h>
#include <signal.h>

/* This file defines macros to support exception handling syntax
 * A typical code block that provides an exception handler will look
 * like this:
 * 
 * {
 *     BEGIN {
 *       do_something("that may raise an exception")
 *     }
 *     EXCEPTION(ex);
 *     WHEN(LIST_ERROR) {
 * 	 // Ignore list errors
 *     }
 *     WHEN(REGEXP_ERROR) {
 * 	objectFree(my_regexp);  // take remedial action
 * 	RAISE();                // and propagate the error 
 *     }
 *     END;
 *     return 0;
 * }
 * 
 * The argument to EXCEPTION() is the name of a variable that may be
 * referenced within the exception handler section.
 * WHEN_OTHERS may be used to trap all untrapped exceptions.
 * FINALLY(code) may be used to run cleanup code that is to be run
 * regardless of whether exceptions have occurred or not.  This must be
 * the final clause before END.
 *
 * The current implementation of the macros is quite complex, made worse
 * by the wish to allow a FINALLY clause.  Prior to the FINALLY clause
 * being implemented, the following code:
 * 
 * {
 *     BEGIN {
 *         // Protected code
 *     }
 *     EXCEPTION(x);
 *     WHEN_OTHERS {
 *       // Handler
 *     }
 *     END;
 * }
 * 
 * would have expanded as follows:
 * {
 *     exceptionPush(exceptionNew(__FILE__, __LINE__));
 *     if (sigsetjmp(exceptionCurHandler()->handler, 1) == 0) {
 * 	// Protected Code
 *         exceptionPop();
 *     }
 *     else {
 * 	Exception *x = exceptionCurHandler(); {
 * 	}
 * 	if (!(exceptionCurHandler()->caught)) { 
 * 	    exceptionCurHandler()->caught = TRUE;
 * 	    // Handler;
 * 	}
 * 	exceptionEnd(__FILE__, __LINE__);
 *     }
 * }
 * Adding the ability to add a finally clause, it now expands to:
 * 
 * {
 *     { 
 * 	boolean exception_raised_jgjlksljksch = TRUE;
 * 	exceptionPush(exceptionNew(__FILE__, __LINE__));
 * 	if (sigsetjmp(exceptionCurHandler()->handler, 1) == 0) {
 * 	    // Protected Code
 * 	    exception_raised_jgjlksljksch = FALSE;
 * 	    //exceptionPop();
 * 	}
 * 	else {
 * 	    Exception *x = exceptionCurHandler(); {
 * 	    }
 * 	    if (!(exceptionCurHandler()->caught)) { 
 * 		exceptionCurHandler()->caught = TRUE;
 * 		// Handler;
 * 	    }
 * 	    //exceptionEnd(__FILE__, __LINE__);
 * 	}
 * 	// Finally code goes here!
 * 	if (exception_raised_jgjlksljksch) {
 * 	    exceptionEnd(__FILE__, __LINE__);
 * 	}
 * 	else {
 * 	    exceptionPop();
 * 	}
 *     }
 * }
 * 
 */


// EXCEPTION HANDLING STUFF

#define LIST_ERROR		40
#define TYPE_MISMATCH		41
#define UNKNOWN_TOKEN           42
#define UNHANDLED_OBJECT_TYPE   43
#define FILEPATH_ERROR          44
#define MISSING_VALUE_ERROR     45
#define OUT_OF_BOUNDS           46
#define PARAMETER_ERROR         47
#define NOT_IMPLEMENTED_ERROR   48
#define XML_PROCESSING_ERROR    49
#define GENERAL_ERROR           50
#define REGEXP_ERROR            51
#define SQL_ERROR               52
#define UNPROCESSED_INCLUSION   53
#define MEMORY_ERROR            54
#define ASSERTION_FAILURE       55
#define XPATH_EXCEPTION         56
#define INDEX_ERROR             57
#define TSORT_ERROR             58
#define TSORT_CYCLIC_DEPENDENCY 59
#define DEPS_ERROR              60
#define UNKNOWN_EXCEPTION       61

/* Create a new exception object, pushing it onto the exception stack. 
 * exceptionNotRaised is TRUE on entry to this block, and the block
 * following the if statement will be executed.  If an exception is
 * raised from anywhere within the block a longjmp returns to the setjmp
 * location and exceptioNotRaised returns false.  The exception handler
 * appears in the else statement that matches the if below.
 */
#define BEGIN								\
    {									\
	boolean exception_avoided_jgjlksljksch = FALSE;			\
	int     exception_signal_jgjlksljksch;                          \
	exceptionPush(exceptionNew(__FILE__, __LINE__));		\
	if (0 == (exception_signal_jgjlksljksch =                       \
                    sigsetjmp(exceptionCurHandler()->handler, 1))) {	\

/* End the if-block opened by BEGIN above, removing the exception
 * handler from the exception stack if we successfully reach the end of
 * the block.  If not, the else clause will be executed, where a local
 * variable is created to contain the exception object.  If no matching
 * WHEN clause is encountered in the block, to handle the exception it
 * will be re-raised on exit from the block (at the END token).
 */
#define EXCEPTION(x)						\
        exception_avoided_jgjlksljksch = TRUE;			\
    }								\
    else {							\
        volatile Exception *x = exceptionCurHandler();		\
        x->signal = exception_signal_jgjlksljksch; {

/* Catch a specific numbered exception.  This block may re-raise the
 * exception using RAISE() with no arguments, may raise a different
 * exception using RAISE() with arguments.  If RAISE is not called, the
 * exception is considered to have been successfully handled.
 */
#define WHEN(x) }					\
        if (exceptionCurHandler()->signal == x) {	\
	    exceptionCurHandler()->caught = TRUE;

/* Catch any exception not already caught by another handler. */
#define WHEN_OTHERS }					\
        if (!(exceptionCurHandler()->caught)) {		\
	    exceptionCurHandler()->caught = TRUE;

/* FINALLY performs code that must be performed before exitting the 
 * exception block, regardless of whether it has handled an exception or
 * not.   Note that this does not currently handle the case where an 
 * exception is raised within the exception block - in this case the 
 * finally clause will not be executed. */
#define FINALLY }}{{

/* End an exception block, re-raising any unhandled exceptions. */
#define END }						\
        }						\
        if (exception_avoided_jgjlksljksch) {		\
	    exceptionPop();				\
        }						\
        else {						\
	    exceptionEnd(__FILE__, __LINE__);		\
        }						\
    }

/* Return from within an exception block, closing off the exception */
#define RETURN(x)					\
    {							\
	typeof(x) result = (x);				\
	exceptionEnd(__FILE__, __LINE__);		\
	return result;					\
    }

/* Return from within an exception block, closing off the exception */
#define RETURN_VOID					\
    {							\
	exceptionEnd(__FILE__, __LINE__);		\
	return;						\
    }

/* Raise an exception.  The args are exception_number and a dynamically
 * allocated text string to provide more information about the exception.
 */
#define RAISE(args...)				\
    exceptionRaise(__FILE__, __LINE__, ##args, 0)


// from exception.c
extern char *exceptionStr(Exception *ex);
extern Exception *exceptionNew(char *file, int line);
extern Exception *exceptionCurHandler();
extern void exceptionPush(Exception *ex);
extern void exceptionPop();
extern void exceptionRaise(char *file, int line, ...);
extern void exceptionEnd(char *file, int line);

