/**
 * @file   capture.c
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Capture stdout and stderr from test functions and make available for	
 * examination.
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <check.h>
#include "../src/skit_lib.h"
#include "suites.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>


static char *
flushCaptured(char *name)
{
    int len = 5000;
    char *buffer = malloc(len);
    char c;
    int i = 0;
	
    FILE *fp = fopen(name, "r");
    while ((c = getc(fp)) != EOF) {
	buffer[i++] = c;
	if (i >= len) {
	    len += 5000;
	    buffer = realloc(buffer, len);
	}
    }
    buffer[i] = '\0';
    fclose(fp);
    unlink(name);
    return buffer;
}

static int
capture_stdout()
{
    int old = dup(fileno(stdout));
    freopen("./test_stdout", "w", stdout);
    return old;
}

static char *
flush_stdout(int old)
{
    fflush(stdout);
    dup2(old, fileno(stdout));
    close(old);

    return flushCaptured("./test_stdout");
}

static int
capture_stderr()
{
    int old = dup(fileno(stderr));
    freopen("./test_stderr", "w", stderr);
    return old;
}

static char *
flush_stderr(int old)
{
    fflush(stderr);
    dup2(old, fileno(stderr));
    close(old);

    return flushCaptured("./test_stderr");
}

static jmp_buf back_to_capture;
static void *prev_handler = NULL;

static void
sigterm_handler(int sig)
{
    fflush(stderr);
    siglongjmp(back_to_capture, sig);
}


static void
capture_sigterm()
{
    prev_handler = signal(SIGTERM, &sigterm_handler);
}

static void
restore_handler()
{
    if (prev_handler) {
	signal(SIGTERM, prev_handler);
	prev_handler = NULL;
    }
}


/* Run test_fn with stdout and stderr captured. */
int 
captureOutput(TestRunner *test_fn,
	      void *param,
	      char **p_stdout,
	      char **p_stderr,
	      int *p_signal)
{
    int old_stdout = capture_stdout();
    int old_stderr = capture_stderr();
    int result;
    int sig;

    if ((sig = sigsetjmp(back_to_capture, 1)) == 0) {
	if (p_signal) {
	    capture_sigterm();
	}
	result = (*test_fn)(param);
    }

    *p_stdout = flush_stdout(old_stdout);
    *p_stderr = flush_stderr(old_stderr);

    if (p_signal) {
	signal(SIGTERM, NULL);
	*p_signal = sig;
    }

    return result;
}



