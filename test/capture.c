/**
 * @file   capture.c
 * \code
 *     Copyright (c) 2009 - 2014 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * 
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

static char *stdout_file = NULL;
static char *stderr_file = NULL;

static char *
flushCaptured(char *name)
{
    int len = 20000;
    char *buffer = malloc(len);
    char c;
    int i = 0;
	
    FILE *fp = fopen(name, "r");
    if (!fp) {
	sprintf(buffer, 
		"==CAPTUREFAIL==\nUnable to open file %s, errno %d: \"%s\"\n", 
		name, errno, strerror(errno));
	return buffer;
    }
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
    freopen(stdout_file, "w", stdout);
    return old;
}

static char *
flush_stdout(int old)
{
    fflush(stdout);
    dup2(old, fileno(stdout));
    close(old);

    return flushCaptured(stdout_file);
}

static int
capture_stderr()
{
    int old = dup(fileno(stderr));
    freopen(stderr_file, "w", stderr);
    return old;
}

static char *
flush_stderr(int old)
{
    fflush(stderr);
    dup2(old, fileno(stderr));
    close(old);

    return flushCaptured(stderr_file);
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

/* Unused - maybe it should be used.  Commenting out for now. 
static void
restore_handler()
{
    if (prev_handler) {
	signal(SIGTERM, prev_handler);
	prev_handler = NULL;
    }
}
*/

static char *
filenameFor(char *name)
{
    char *result = malloc(100);
    sprintf(result, "./test_%s_%x", name, getpid());
    return result;
}

/* Run test_fn with stdout and stderr captured. */
int 
captureOutput(TestRunner *test_fn,
	      void *param,
	      char **p_stdout,
	      char **p_stderr,
	      int *p_signal)
{
    int old_stdout;
    int old_stderr;
    int result = 0;
    int sig;
    
    stdout_file = filenameFor("stdout");
    stderr_file = filenameFor("stderr");
    old_stdout = capture_stdout();
    old_stderr = capture_stderr();

    if ((sig = sigsetjmp(back_to_capture, 1)) == 0) {
	if (p_signal) {
	    capture_sigterm();
	}
	result = (*test_fn)(param);
    }

    *p_stdout = flush_stdout(old_stdout);
    *p_stderr = flush_stderr(old_stderr);
    if (strncmp(*p_stdout, "==CAPTUREFAIL==", 15) == 0) {
	fprintf(stderr, "STDOUT CAPTURE FAIL: %s\n", *p_stdout);
    }

    if (strncmp(*p_stderr, "==CAPTUREFAIL==", 15) == 0) {
	fprintf(stderr, "STDERR CAPTURE FAIL: %s\n", *p_stderr);
    }

    if (p_signal) {
	signal(SIGTERM, NULL);
	*p_signal = sig;
    }

    free(stdout_file);
    free(stderr_file);
    stdout_file = NULL;
    stderr_file = NULL;
    return result;
}



