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


static int stdoutpipe[2];
static int stderrpipe[2];
static int retoutpipe[2];
static int reterrpipe[2];
static int oldstdout;
static int oldstderr;
static pid_t outpid = 0;
static pid_t errpid = 0;

/* Use fd4 to provide an output stream for catcher to write the captured
 * output */
FILE *
returnStream()
{
    static FILE *fp = NULL;
    if (!fp) {
	fp = fdopen(4, "w");
    }
    return fp;
}

/* Use fd4 to provide an input stream for parent to read data from the kids
 */
FILE *
resultsStream()
{
    static FILE *fp = NULL;
    if (!fp) {
	fp = fdopen(4, "r");
    }
    return fp;
}

/* Run by the children to figure out whether to print the output they capture.
 */
static boolean
chkPrint()
{
    char *line = malloc(20);
    line[0] = '\0';

    fgets(line, 20, stdin);  /* Read first line to figure out whether to
			      * print */

    if (streq(line, "NOPRINT\n")) {
	free(line);
	return FALSE;
    }
    else if (!streq(line, "PRINT\n")) {
	printf("INVALID FIRST LINE \"%s\"\n", line);
	free(line);
	exit(2);
    }
    free(line);
    return TRUE;
}

/* malloc and realloc a dynamic buffer as required.
 */
static void
resizeBuf(char **p_buf, int *len)
{
    if (*len) {
	*len += 20000;
	*p_buf = realloc(*p_buf, *len);
    }
    else {
	*len = 5000;
	*p_buf = malloc(*len);
    }
}

/* Read into a dynamically sized buffer from the given FILE source,
 * printing to printstream if output is required */
static char *
readToBuffer(FILE *instream, FILE *printstream)
{
    int len = 0;
    char c;
    char *buf;
    char *end;
    char *next;
    char *buffer;

    resizeBuf(&buffer, &len);
    next = buf = buffer;
    while (TRUE) {
	end = buf + len;
	if (printstream) {
	    while (next < end) {
		if ((c = getc(instream)) == EOF) {
		    *next = '\0';
		    return buffer;
		}
		*next++ = c;
		putc(c, printstream);
	    }
	}
	else {
	    while (next < end) {
		if ((c = getc(instream)) == EOF) {
		    *next = '\0';
		    return buffer;
		}
		*next++ = c;
	    }
	}
	resizeBuf(&buffer, &len);
    }
}

static void
writeFromBuffer(char *buf, FILE *outstream)
{
    char c;
    while ((c = *buf++) != '\0') {
	putc(c, outstream);
    }
    fflush(outstream);
}

static void
catcher(FILE *out, boolean debug)
{
    char *buffer = NULL;
    boolean printing = chkPrint();
    FILE *stream;

    buffer = readToBuffer(stdin, printing? out: NULL);
    // We get here either because readToBuffer has finished or we have
    // been interupted.
    if (debug)
    {
	sleep(1);
	fprintf(stderr, "[[[<<<%s>>>]]]\n", buffer);
	sleep(1);
    }
    writeFromBuffer(buffer, stream = returnStream());
    free(buffer);
    fclose(stream);
}

static void
openPipe(int pipe_pair[])
{
    int ret = pipe(pipe_pair);
    if (ret) {
	fprintf(stderr, "Pipe error result: %d errno: %d\n", ret, errno);
	exit(2);
    }
}

static void
readStdout()
{
    close(stdoutpipe[1]);  // Close un-needed pipes
    close(retoutpipe[0]);

    /* Redirect stdoutpipe[0] to stdin */
    dup2(stdoutpipe[0], 0);
    close(stdoutpipe[0]);

    /* Redirect return pipe to fd4 */
    dup2(retoutpipe[1], 4);

    catcher(stdout, FALSE);
}

static void
readStderr()
{
    close(stdoutpipe[1]);  /* Close un-needed pipes */
    close(retoutpipe[0]);
    close(stderrpipe[1]);
    close(reterrpipe[0]);

    /* Redirect stdoutpipe[0] to stdin */
    dup2(stderrpipe[0], 0);
    close(stderrpipe[0]);

    /* Redirect return pipe to fd4 */
    dup2(reterrpipe[1], 4);

    catcher(stderr, FALSE);
}

/* Open pipes for communication with child process, and create those
 * children to capture stdout and stderr. */
static void
openPipes()
{
    static boolean done = FALSE;
    int ret;

    if (! done) {
	openPipe(stdoutpipe);  	/* Open pipes for capture of stdout
				   streams */
	openPipe(retoutpipe);	

	if ((outpid = fork()) == 0) {
	    /* Child process */
	    readStdout();
	    exit(0);
	}

	/* Parent process */
	close(stdoutpipe[0]);  	/* Close un-needed pipes */
	close(retoutpipe[1]);

	openPipe(stderrpipe);	/* Open pipes for stderr capture */
	openPipe(reterrpipe);

	if ((errpid = fork()) == 0) {
	    readStderr();
	    exit(0);
	}

	//fprintf(stderr, "stdout reader is %d\n", outpid);
	//fprintf(stderr, "stderr reader is %d\n", errpid);
	//fprintf(stderr, "I am %d\n", getpid());
	close(stderrpipe[0]);  /* Close un-needed pipes */
	close(reterrpipe[1]);
    }
    done = TRUE;
}

static void 
beginCapture(int print)
{
    openPipes();

    oldstdout = dup(1);		/* Redirect stdout */
    dup2(stdoutpipe[1], 1);
    oldstderr = dup(2);		/* Redirect stderr */
    dup2(stderrpipe[1], 2);

    if (print) {		/* Signal the kids to display or not the */
	printf("PRINT\n");	/* captured output. */
	fprintf(stderr, "PRINT\n");
    }
    else {
	printf("NOPRINT\n");
	fprintf(stderr, "NOPRINT\n");
    }
}

/* Describe the termination status of a kid.
 */
static void
showStatus(char *name, int status)
{
    fprintf(stderr, "%s reader: ", name);
    if (WIFEXITED(status)) {
	fprintf(stderr, "exitted with status %d\n", WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
	fprintf(stderr, " terminated by signal %d\n", WTERMSIG(status));
    }
    else {
	fprintf(stderr, ("(don't know what happened)\n"));
    }
}

static void
endCqpture(char **p_stdout, char **p_stderr)
{
    int status1;
    int status2;
    FILE *stream;

    close(stdoutpipe[1]);	/* Close stdout to kid */
    dup2(oldstdout, 1);  	/* Restore previous stdout */
    dup2(retoutpipe[0], 4);     /* Redirect fd4 for stdout from kid */

    *p_stdout = readToBuffer(stream = resultsStream(), NULL);
    close(retoutpipe[0]);
    close(stderrpipe[1]);

    dup2(oldstderr, 2);
    dup2(reterrpipe[0], 4);     /* Redirect fd4 for stderr from kid */
    *p_stderr = readToBuffer(resultsStream(), NULL);

    if (waitpid(outpid, &status1, 0) != outpid) {
	showStatus("stdout", status1);
    }
    if (waitpid(errpid, &status2, 0) != errpid) {
	showStatus("stderr", status2);
    }
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
	      int print,
	      char **p_stdout,
	      char **p_stderr,
	      int *p_signal)
{
    int sig;
    int result;
    beginCapture(print);
    if ((sig = sigsetjmp(back_to_capture, 1)) == 0) {
	if (p_signal) {
	    capture_sigterm();
	}
	result = (*test_fn)(param);
    }
    endCqpture(p_stdout, p_stderr);
    if (p_signal) {
	signal(SIGTERM, NULL);
	*p_signal = sig;
    }
    restore_handler();
    return result;
}

